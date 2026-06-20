#include "Dispatcher.h"

#include "downloader/DownloadTask.h"
#include "downloader/TaskConfigure.h"
#include "dto/TaskContext.h"
#include "utils/TaskLogger.h"

#include <utility>

namespace edm {


Dispatcher::Dispatcher(Options options)
: taskLoader_(std::move(options.taskLoader)),
  onTaskChanged_(std::move(options.onTaskChanged)),
  configureFactory_(std::move(options.configureFactory)) {
    EDM_LOG_INFO(kInvalidTaskID, "Dispatcher initialized");
}

Dispatcher::~Dispatcher() = default;

namespace {

std::shared_ptr<TaskContext> makeContextFromModel(std::shared_ptr<TaskModel> model) {
    if (!model) return nullptr;

    auto ctx       = std::make_shared<TaskContext>();
    ctx->model     = std::move(model);
    return ctx;
}

} // namespace

std::shared_ptr<TaskConfigure> Dispatcher::makeConfigure(std::shared_ptr<TaskModel> const& model) const {
    if (configureFactory_) {
        return configureFactory_(model);
    }
    return std::make_shared<TaskConfigure>(model);
}

void Dispatcher::persistTask(std::shared_ptr<TaskContext> const& ctx) const {
    if (onTaskChanged_) {
        onTaskChanged_(ctx);
    }
}


std::shared_ptr<downloader::DownloadState> Dispatcher::getTaskState(TaskId id) {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    auto                        it = activeTasks_.find(id);
    if (it == activeTasks_.end()) return nullptr;
    return it->second->getStateObject();
}
std::shared_ptr<TaskModel> Dispatcher::getTaskModel(TaskId id) {
    {
        std::lock_guard<std::mutex> lock(tasksMutex_);

        auto it = activeTasks_.find(id);
        if (it != activeTasks_.end()) {
            return it->second->getTaskContext()->model;
        }
    }
    return taskLoader_ ? taskLoader_(id) : nullptr;
}

void Dispatcher::updateAllSpeeds() {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    for (auto& [id, task] : activeTasks_) {
        task->updateSpeed();
    }
}

void Dispatcher::updateSpeed(TaskId id) {
    std::lock_guard<std::mutex> lock(tasksMutex_);

    auto iter = activeTasks_.find(id);
    if (iter != activeTasks_.end()) {
        iter->second->updateSpeed();
    }
}

Expected<> Dispatcher::pauseTask(TaskId id) {
    EDM_LOG_INFO(id, "Dispatcher: pauseTask requested");
    std::lock_guard<std::mutex> lock(tasksMutex_);

    auto iter = activeTasks_.find(id);
    if (iter != activeTasks_.end()) {
        return iter->second->pause();
    }
    EDM_LOG_WARN(id, "Dispatcher: pauseTask failed, task is not active");
    return makeStringError("Task is not active");
}

Expected<> Dispatcher::resumeTask(TaskId id) {
    EDM_LOG_INFO(id, "Dispatcher: resumeTask requested");
    std::shared_ptr<downloader::DownloadTask> task;
    {
        std::lock_guard<std::mutex> lock(tasksMutex_);

        auto iter = activeTasks_.find(id);
        if (iter != activeTasks_.end()) {
            task = iter->second;
        }
    }

    if (task) {
        return task->resume();
    }

    auto model = taskLoader_ ? taskLoader_(id) : nullptr;
    if (!model) {
        EDM_LOG_ERROR(id, "Dispatcher: resumeTask failed, task does not exist");
        return makeStringError("Task does not exist");
    }
    if (model->state != TaskState::Paused && model->state != TaskState::Failed && model->state != TaskState::Pending) {
        EDM_LOG_WARN(id, "Dispatcher: resumeTask failed, task is not resumable from state {}", static_cast<int>(model->state));
        return makeStringError("Task is not resumable from current state");
    }

    auto ctx = makeContextFromModel(model);
    ctx->configure = makeConfigure(ctx->model);
    auto resumedTask =
        std::make_shared<downloader::DownloadTask>(ctx, [this](std::shared_ptr<edm::TaskContext> const& changedCtx) {
            persistTask(changedCtx);
        });

    {
        std::lock_guard<std::mutex> lock(tasksMutex_);
        activeTasks_[id] = resumedTask;
    }

    auto started = resumedTask->resume();
    if (!started) {
        EDM_LOG_ERROR(id, "Dispatcher: resumeTask start failed: {}", started.error().message());
        std::lock_guard<std::mutex> lock(tasksMutex_);
        activeTasks_.erase(id);
    }
    return started;
}

Expected<> Dispatcher::cancelTask(TaskId id) {
    EDM_LOG_INFO(id, "Dispatcher: cancelTask requested");
    std::lock_guard<std::mutex> lock(tasksMutex_);

    auto iter = activeTasks_.find(id);
    if (iter != activeTasks_.end()) {
        return iter->second->cancel();
    }
    EDM_LOG_WARN(id, "Dispatcher: cancelTask failed, task is not active");
    return makeStringError("Task is not active");
}

Expected<> Dispatcher::scheduleTask(std::shared_ptr<edm::TaskContext> task) {
    if (!task || !task->model) {
        return makeStringError("Task context is missing");
    }

    auto taskId = task->model->id;
    EDM_LOG_DEBUG(taskId, "Dispatcher: scheduling task");

    {
        std::lock_guard<std::mutex> lock(tasksMutex_);
        if (activeTasks_.find(taskId) != activeTasks_.end()) {
            EDM_LOG_DEBUG(taskId, "Dispatcher: task already active, skipping");
            return {}; // 已经在跑了
        }
    }

    // 从 Model 转换为 Configure
    if (!task->configure) {
        task->configure = makeConfigure(task->model);
    }

    auto downloadTask =
        std::make_shared<downloader::DownloadTask>(task, [this](std::shared_ptr<edm::TaskContext> const& ctx) {
            persistTask(ctx);
        });

    {
        std::lock_guard<std::mutex> lock(tasksMutex_);
        if (activeTasks_.find(taskId) != activeTasks_.end()) {
            return {};
        }
        activeTasks_[taskId] = downloadTask;
    }

    if (auto started = downloadTask->start(); !started) {
        EDM_LOG_ERROR(taskId, "Dispatcher: failed to start task: {}", started.error().message());
        std::lock_guard<std::mutex> lock(tasksMutex_);
        auto it = activeTasks_.find(taskId);
        if (it != activeTasks_.end() && it->second == downloadTask) {
            activeTasks_.erase(it);
        }
        return forwardError(started.error());
    }
    EDM_LOG_INFO(taskId, "Dispatcher: task scheduled and started");
    return {};
}

Expected<std::shared_ptr<TaskContext>>
Dispatcher::createTask(std::shared_ptr<TaskModel> model, std::shared_ptr<TaskConfigure> configure) {
    if (!model) {
        return makeStringError("Task model is missing");
    }
    if (model->url.empty()) {
        return makeStringError("Task URL is empty");
    }
    if (model->state == kInvalidTaskState) {
        model->state = TaskState::Pending;
    }
    if (model->retryCount < 0) {
        model->retryCount = kRetryCount;
    }

    EDM_LOG_INFO(model->id, "Dispatcher: createTask, url={}", model->url);

    auto ctx       = std::make_shared<TaskContext>();
    ctx->model     = std::move(model);
    ctx->configure = std::move(configure);
    if (!ctx->configure) {
        ctx->configure = makeConfigure(ctx->model);
    }

    persistTask(ctx);
    if (ctx->model->id <= kInvalidTaskID) {
        ctx->model->id = nextTransientId_.fetch_add(1, std::memory_order_relaxed);
        EDM_LOG_DEBUG(ctx->model->id, "Dispatcher: assigned transient id");
        persistTask(ctx);
    }

    if (auto scheduled = scheduleTask(ctx); !scheduled) {
        return forwardError(scheduled.error());
    }
    EDM_LOG_INFO(ctx->model->id, "Dispatcher: task created successfully");
    return ctx;
}


} // namespace edm
