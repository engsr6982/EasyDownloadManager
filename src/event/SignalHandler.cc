#include "SignalHandler.h"

#include "EasyDownloadManager.h"
#include "EventBus.h"
#include "config/EdmGlobalConfig.h"
#include "database/DownloadDatabase.h"
#include "downloader/TaskConfigure.h"
#include "downloader/TaskMetaInfoFetcher.h"
#include "ui/main/MainWindow.h"
#include "ui/new_task/NewTaskDialog.h"
#include "ui/task_information/TaskInformationDialog.h"
#include "utils/StringUtils.h"

#include <QApplication>

namespace edm {

SignalHandler* SignalHandler::instance_ = new SignalHandler{};


SignalHandler::SignalHandler() {
    connect(
        EventBus::instance(),
        &EventBus::onRequestOpenNewTaskDialog,
        this,
        &SignalHandler::handleRequestOpenNewTaskDialog
    );

    connect(EventBus::instance(), &EventBus::onRequestCreateTask, this, &SignalHandler::handleRequestCreateTask);

    connect(
        EventBus::instance(),
        &EventBus::onRequestOpenSettingDialog,
        this,
        &SignalHandler::handleRequestOpenSettingDialog
    );

    connect(
        EventBus::instance(),
        &EventBus::onRequestOpenTaskInfoDialog,
        this,
        &SignalHandler::handleRequestOpenTaskInfoDialog
    );
}


SignalHandler::~SignalHandler() = default;

SignalHandler* SignalHandler::instance() { return instance_; }

void SignalHandler::handleRequestOpenNewTaskDialog(bool /*checked*/) const {
    auto dialog = new NewTaskDialog(EasyDownloadManager::getOrNewInstance().getMainWindow());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void SignalHandler::handleRequestCreateTask(QString const& url, QString const& saveDir, bool useProxy) const {
    qDebug() << "SignalHandler::handleRequestCreateTask" << url << saveDir << useProxy;

    downloader::TaskConfigure configure;
    configure.url_     = string_utils::qstring2string(url);
    configure.saveDir_ = string_utils::qstring2string(saveDir);

    auto& cfg                 = EdmGlobalConfig::instance();
    configure.tempDir_        = string_utils::qstring2string(cfg.getTempDir());
    configure.threadCount_    = cfg.getThreadCount();
    configure.userAgent_      = string_utils::qstring2string(cfg.getUserAgent());
    configure.bandWidthLimit_ = cfg.getBandwidthLimit();
    if (auto proxy = cfg.getProxyConfig(); useProxy && !proxy.isNone()) {
        configure.proxyUrl_ = proxy_utils::toProxyUrl(proxy);
    }
}
void SignalHandler::handleRequestOpenSettingDialog(bool checked) const {
    EasyDownloadManager::getOrNewInstance().tryShowSettingDialog();
}
void SignalHandler::handleRequestOpenTaskInfoDialog(int id) const {
    auto db   = EasyDownloadManager::getOrNewInstance().getDatabase();
    auto info = db->getTaskById(id);
    if (!info) {
        return;
    }
    // TODO: 传递任务信息
    auto dialog = new TaskInformationDialog{EasyDownloadManager::getOrNewInstance().getMainWindow()};
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}


} // namespace edm