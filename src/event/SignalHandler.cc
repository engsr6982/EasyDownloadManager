#include "SignalHandler.h"

#include "Dispatcher.h"
#include "EdmApplication.h"
#include "EdmConfig.h"
#include "EventBus.h"
#include "downloader/TaskConfigure.h"
#include "dto/TaskContext.h"

#include "fmt/format.h"
#include "model/TaskModel.h"

#include <qmessagebox.h>
#include <utility>

namespace edm {

namespace {
static SignalHandler inst = SignalHandler{}; // 全局初始化，避免无调用方导致未初始化信号连接
}

SignalHandler::SignalHandler() {
    auto bus = EventBus::instance();

    connect(bus, &EventBus::onRequestCreateTask, this, &SignalHandler::handleRequestCreateTask);
}

SignalHandler::~SignalHandler() = default;
SignalHandler* SignalHandler::instance() { return &inst; }

void SignalHandler::handleRequestCreateTask(QString const& url, QString const& saveDir, bool useProxy) const {
    qDebug() << "SignalHandler::handleRequestCreateTask: "
             << fmt::format("url: {}, saveDir: {}, useProxy: {}", url.toStdString(), saveDir.toStdString(), useProxy);

    auto& conf = EdmConfig::getInstance();

    auto model = TaskModel::make();

    model->url         = url.toStdString();
    model->bandLimit   = conf.getBandwidthLimit();
    model->threadCount = conf.getThreadCount();
    model->retryCount  = kRetryCount;
    model->firstTry    = std::time(nullptr);
    model->lastTry     = model->firstTry;
    model->userAgent   = conf.getUserAgent().toStdString();
    model->saveDir     = saveDir.toStdString();

    TaskConfigureDefaults defaults;
    defaults.threadCount = model->threadCount;
    defaults.bandLimit   = model->bandLimit;
    defaults.retryCount  = model->retryCount;
    defaults.userAgent   = model->userAgent;
    if (auto proxy = conf.getProxyConfig(); useProxy && !proxy.isNone()) {
        defaults.proxyUrl = proxy.toProxyUrl();
    }

    auto configure = TaskConfigure::fromUrl(url.toStdString(), saveDir.toStdString(), std::move(defaults));
    auto created   = EdmApplication::getInstance().getDispatcher()->createTask(model, configure);
    if (!created) {
        if (model->id > kInvalidTaskID) {
            auto failedCtx       = std::make_shared<TaskContext>();
            failedCtx->model     = model;
            failedCtx->configure = configure;
            emit EventBus::instance() -> onTaskCreated(failedCtx);
        }
        QMessageBox::warning(nullptr, "创建任务失败", QString::fromStdString(created.error().message()));
        return;
    }

    emit EventBus::instance() -> onTaskCreated(created.value()); // 任务已创建事件
}

} // namespace edm
