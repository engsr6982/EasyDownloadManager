#include "SignalHandler.h"

#include "EdmApplication.h"
#include "EventBus.h"
#include "Events.h"
#include "config/EdmGlobalConfig.h"
#include "database/DownloadDatabase.h"
#include "downloader/MetaInfoFetcher.h"
#include "downloader/TaskConfigure.h"
#include "ui/main/MainWindow.h"
#include "ui/new_task/NewTaskDialog.h"
#include "ui/task_information/TaskInformationDialog.h"
#include "utils/StringUtils.h"

#include "fmt/format.h"

#include <QApplication>
#include <qmessagebox.h>

namespace edm {

SignalHandler* SignalHandler::instance_ = new SignalHandler{};


SignalHandler::SignalHandler() {
    auto bus = EventBus::instance();

    connect(bus, &EventBus::onRequestOpenNewTaskDialog, this, &SignalHandler::handleRequestOpenNewTaskDialog);

    connect(bus, &EventBus::onRequestCreateTask, this, &SignalHandler::handleRequestCreateTask);

    connect(bus, &EventBus::onRequestOpenSettingDialog, this, &SignalHandler::handleRequestOpenSettingDialog);

    connect(bus, &EventBus::onRequestOpenTaskInfoDialog, this, &SignalHandler::handleRequestOpenTaskInfoDialog);

    qRegisterMetaType<edm::MetaInfoResultEvent>("edm::MetaInfoResultEvent");
    connect(bus, &EventBus::onTaskMetaInfoFetched, this, &SignalHandler::handleTaskMetaInfoFetched);
}


SignalHandler::~SignalHandler() = default;

SignalHandler* SignalHandler::instance() { return instance_; }

void SignalHandler::handleRequestOpenNewTaskDialog(bool /*checked*/) const {
    auto dialog = new NewTaskDialog(EdmApplication::getInstance().getMainWindow());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void SignalHandler::handleRequestCreateTask(QString const& url, QString const& saveDir, bool useProxy) const {
    qDebug() << "SignalHandler::handleRequestCreateTask: "
             << fmt::format("url: {}, saveDir: {}, useProxy: {}", url.toStdString(), saveDir.toStdString(), useProxy);

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

    // 发起任务信息获取异步任务, 下游组件创建下载任务
    downloader::MetaInfoFetcher::fetchAsync(configure);
}
void SignalHandler::handleRequestOpenSettingDialog(bool checked) const {
    EdmApplication::getInstance().tryShowSettingDialog();
}

void SignalHandler::handleRequestOpenTaskInfoDialog(int id) const {
    auto db   = EdmApplication::getInstance().getDatabase();
    auto info = db->getTaskById(id);
    if (!info) {
        return;
    }
    auto dialog = new TaskInformationDialog{*info, EdmApplication::getInstance().getMainWindow()};
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void SignalHandler::handleTaskMetaInfoFetched(edm::MetaInfoResultEvent const& ev) const {
    qDebug() << "SignalHandler::handleTaskMetaInfoFetched";
    assert(ev.hold<std::monostate>() == false);
    if (ev.hold<std::string>()) {
        QMessageBox::warning(
            EdmApplication::getInstance().getMainWindow(),
            "Error",
            QString::fromStdString(std::get<std::string>(ev.result))
        );
        return;
    }

    auto info = std::get<downloader::FetchedMetaInfo>(ev.result);
    qDebug() << "FetchedMetaInfo: "
             << fmt::format(
                    "finalUrl={}, supportRange={}, fileSize={}, contentType={}, etag={}, lastModified={}, "
                    "contentDisposition={}, md5={}",
                    info.finalUrl,
                    info.supportRange,
                    info.fileSize ? std::to_string(*info.fileSize) : "nullopt",
                    info.contentType.value_or("nullopt"),
                    info.etag.value_or("nullopt"),
                    info.lastModified.value_or("nullopt"),
                    info.contentDisposition.value_or("nullopt"),
                    info.md5.value_or("nullopt")
                );
    // TODO: 获取到文件信息后，正式构造 TaskModel
    // EdmApplication::getInstance().getDatabase()->insertTask(model);
    // 将任务丢给 Dispatcher 去实例化 DownloadTask 并执行
}


} // namespace edm