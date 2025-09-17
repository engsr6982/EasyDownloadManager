#include "SignalHandler.h"

#include "EasyDownloadManager.h"
#include "EventBus.h"
#include "config/EdmGlobalConfig.h"
#include "downloader/TaskConfigure.h"
#include "downloader/TaskMetaInfoFetcher.h"
#include "ui/main/MainWindow.h"
#include "ui/new_task/NewTaskDialog.h"
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
}


SignalHandler::~SignalHandler() = default;

SignalHandler* SignalHandler::instance() { return instance_; }

void SignalHandler::handleRequestOpenNewTaskDialog(QWidget* parent) const {
    if (!parent || !parent->isVisible()) {
        parent = EasyDownloadManager::getOrNewInstance().getMainWindow();
    }
    auto dialog = new NewTaskDialog(parent);
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
void SignalHandler::handleRequestOpenSettingDialog() const {
    EasyDownloadManager::getOrNewInstance().tryShowSettingDialog();
}


} // namespace edm