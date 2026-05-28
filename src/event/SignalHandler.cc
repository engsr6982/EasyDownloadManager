#include "SignalHandler.h"

#include "EdmApplication.h"
#include "EdmConfig.h"
#include "EventBus.h"
#include "database/DownloadDatabase.h"
#include "downloader/TaskConfigure.h"
#include "dto/TaskContext.h"
#include "ui/main/MainWindow.h"
#include "ui/new_task/NewTaskDialog.h"
#include "ui/task_information/TaskInformationDialog.h"

#include "fmt/format.h"
#include "model/TaskModel.h"
#include "ui/task_downloading/TaskDownloadingDialog.h"
#include "utils/Utils.h"

#include <QApplication>
#include <qmessagebox.h>

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

    // 为了用户 UI 体验，采取异步处理任务
    // 所以这里先入库，后台拉取任务信息后再更新

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

    // 存入数据库
    EdmApplication::getInstance().getDatabase()->insertTask(model);

    auto ctx       = std::make_shared<TaskContext>();
    ctx->model     = model;
    ctx->configure = TaskConfigure::fromUrl(url.toStdString(), saveDir.toStdString(), useProxy);

    emit EventBus::instance() -> onTaskCreated(ctx); // 任务已创建事件
}

} // namespace edm
