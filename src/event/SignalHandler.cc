#include "SignalHandler.h"

#include "EdmApplication.h"
#include "EdmConfig.h"
#include "EventBus.h"
#include "Events.h"
#include "database/DownloadDatabase.h"
#include "downloader/MetaInfoFetcher.h"
#include "downloader/TaskConfigure.h"
#include "ui/main/MainWindow.h"
#include "ui/new_task/NewTaskDialog.h"
#include "ui/task_information/TaskInformationDialog.h"

#include "fmt/format.h"
#include "utils/Utils.h"

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

    auto configure = downloader::TaskConfigure::fromUrl(url.toStdString(), saveDir.toStdString(), useProxy);

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

    std::string fileName = "unknown_file.dat";
    auto        urlStr   = info.finalUrl;
    auto        pos      = urlStr.find_last_of('/');
    if (pos != std::string::npos && pos + 1 < urlStr.length()) {
        fileName = urlStr.substr(pos + 1);
        // 去除可能的 URL 参数 ?xxx=yyy
        auto qMark = fileName.find('?');
        if (qMark != std::string::npos) fileName = fileName.substr(0, qMark);
    }

    auto& conf = EdmConfig::getInstance();

    TaskModel model{};
    model.id             = 0;
    model.url            = info.finalUrl;
    model.fileName       = fileName;
    model.fileSize       = info.fileSize.value_or(0);
    model.category       = utils::resolveFileCategory(fileName);
    model.state          = TaskState::Pending;
    model.bandWidthLimit = conf.getBandwidthLimit();
    model.threadCount    = conf.getThreadCount();
    model.firstTry       = std::time(nullptr);
    model.lastTry        = model.firstTry;
    model.userAgent      = conf.getUserAgent().toStdString();
    model.resumable      = info.supportRange ? Resumable::Yes : Resumable::No;
    model.saveDir        = conf.getSaveDir().toStdString();

    // 存入数据库
    EdmApplication::getInstance().getDatabase()->insertTask(model);

    // 通知主窗口插入新行
    emit EventBus::instance() -> onTaskAddedToDatabase(model);

    // 通知 Dispatcher 将任务加入调度队列并启动
    emit EventBus::instance() -> onRequestDispatchTask(model);
}


} // namespace edm