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


SignalHandler::SignalHandler() {
    auto bus = EventBus::instance();

    connect(bus, &EventBus::onShowNewTaskDialog, this, &SignalHandler::handleShowNewTaskDialog);

    connect(bus, &EventBus::onRequestCreateTask, this, &SignalHandler::handleRequestCreateTask);

    connect(bus, &EventBus::onShowSettingDialog, this, &SignalHandler::handleShowSettingDialog);

    connect(bus, &EventBus::onShowTaskInfoDialog, this, &SignalHandler::handleShowTaskInfoDialog);

    connect(bus, &EventBus::onShowDownloadingDialog, this, &SignalHandler::handleShowDownloadingDialog);
}

SignalHandler::~SignalHandler() = default;
SignalHandler* SignalHandler::instance() {
    static SignalHandler inst;
    return &inst;
}

void SignalHandler::handleShowNewTaskDialog(bool /*checked*/) const {
    auto dialog = new NewTaskDialog(EdmApplication::getInstance().getMainWindow());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void SignalHandler::handleRequestCreateTask(QString const& url, QString const& saveDir, bool useProxy) const {
    qDebug() << "SignalHandler::handleRequestCreateTask: "
             << fmt::format("url: {}, saveDir: {}, useProxy: {}", url.toStdString(), saveDir.toStdString(), useProxy);

    // 为了用户 UI 体验，采取异步处理任务
    // 所以这里先入库，后台拉取任务信息后再更新

    auto& conf = EdmConfig::getInstance();

    auto model = TaskModel::make();

    model->url            = url.toStdString();
    model->bandLimit = conf.getBandwidthLimit();
    model->threadCount    = conf.getThreadCount();
    model->firstTry       = std::time(nullptr);
    model->lastTry        = model->firstTry;
    model->userAgent      = conf.getUserAgent().toStdString();
    model->saveDir        = conf.getSaveDir().toStdString();

    // 存入数据库
    EdmApplication::getInstance().getDatabase()->insertTask(model);

    auto ctx       = std::make_shared<TaskContext>();
    ctx->model     = std::move(model);
    ctx->configure = TaskConfigure::fromUrl(url.toStdString(), saveDir.toStdString(), useProxy);

    emit EventBus::instance() -> onTaskCreated(ctx); // 任务已创建事件
}

void SignalHandler::handleShowSettingDialog(bool checked) const {
    EdmApplication::getInstance().tryShowSettingDialog();
}

void SignalHandler::handleShowTaskInfoDialog(int id) const {
    auto db   = EdmApplication::getInstance().getDatabase();
    auto info = db->getTaskById(id);
    if (!info) {
        return;
    }
    auto dialog = new TaskInformationDialog{info, EdmApplication::getInstance().getMainWindow()};
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void SignalHandler::handleShowDownloadingDialog(int id) const {
    auto dialog = new TaskDownloadingDialog(id, EdmApplication::getInstance().getMainWindow());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}


} // namespace edm