#pragma once
#include "expected.h"
#include "model/TaskModel.h"

#include <functional>
#include <memory>
#include <mutex>
#include <atomic>
#include <unordered_map>


namespace edm {

struct TaskContext;
struct TaskModel;
struct TaskConfigure;

namespace downloader {
class DownloadState;
class DownloadTask;
} // namespace downloader


class Dispatcher {
public:
    using TaskLoader           = std::function<std::shared_ptr<TaskModel>(TaskId)>;
    using TaskChangedCallback  = std::function<void(std::shared_ptr<TaskContext> const&)>;
    using TaskConfigureFactory = std::function<std::shared_ptr<TaskConfigure>(std::shared_ptr<TaskModel> const&)>;

    struct Options {
        TaskLoader           taskLoader;
        TaskChangedCallback  onTaskChanged;
        TaskConfigureFactory configureFactory;
    };

private:
    std::unordered_map<TaskId, std::shared_ptr<downloader::DownloadTask>> activeTasks_;
    std::mutex                                                            tasksMutex_;
    TaskLoader                                                            taskLoader_;
    TaskChangedCallback                                                   onTaskChanged_;
    TaskConfigureFactory                                                  configureFactory_;
    std::atomic<TaskId>                                                    nextTransientId_{1};

    [[nodiscard]] std::shared_ptr<TaskConfigure> makeConfigure(std::shared_ptr<TaskModel> const& model) const;
    void persistTask(std::shared_ptr<TaskContext> const& ctx) const;
    [[nodiscard]] Expected<> scheduleTask(std::shared_ptr<TaskContext> task);

public:
    explicit Dispatcher(Options options = {});
    ~Dispatcher();

    std::shared_ptr<downloader::DownloadState> getTaskState(TaskId id);
    std::shared_ptr<TaskModel> getTaskModel(TaskId id);

    void updateAllSpeeds();

    void updateSpeed(TaskId id);

    Expected<> pauseTask(TaskId id);
    Expected<> resumeTask(TaskId id);
    Expected<> cancelTask(TaskId id);

    [[nodiscard]] Expected<std::shared_ptr<TaskContext>>
    createTask(std::shared_ptr<TaskModel> model, std::shared_ptr<TaskConfigure> configure = nullptr);
};

} // namespace edm
