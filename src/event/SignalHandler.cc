#include "SignalHandler.h"

#include "Dispatcher.h"
#include "EdmApplication.h"
#include "EdmConfig.h"
#include "EventBus.h"
#include "database/DownloadDatabase.h"
#include "database/TaskModel.h"
#include "database/TaskModelConversion.h"
#include "qml/QmlTaskListModel.h"
#include "utils/Utils.h"
#include <fmt/format.h>

namespace edm {

namespace {
static SignalHandler inst = SignalHandler{};
} // namespace

SignalHandler::SignalHandler() {
    auto bus = EventBus::instance();
    connect(bus, &EventBus::onRequestCreateTask, this, &SignalHandler::handleRequestCreateTask);
}

SignalHandler::~SignalHandler() = default;
SignalHandler* SignalHandler::instance() { return &inst; }

void SignalHandler::handleRequestCreateTask(QString const& url, QString const& saveDir, bool useProxy) const {
    qDebug() << "SignalHandler::handleRequestCreateTask: "
             << fmt::format("url: {}, saveDir: {}, useProxy: {}", url.toStdString(), saveDir.toStdString(), useProxy);

    // 委托给 QmlTaskListModel (QML 路径)
    // model->createTask 内部会插入 UI 行并发出 onTaskCreated 信号
    if (auto* model = EdmApplication::getInstance().getTaskListModel()) {
        model->createTask(url, saveDir, useProxy);
        return;
    }

    // 回退: 直接调用 Dispatcher (QML 模式下通常不会到达这里)
    auto& conf       = EdmConfig::getInstance();
    auto* db         = EdmApplication::getInstance().getDatabase();
    auto  stdUrl     = url.toStdString();
    auto  stdSaveDir = saveDir.toStdString();

    // 构建 TaskModel 并持久化以获取有效 ID
    auto model         = TaskModel::make();
    model->url         = stdUrl;
    model->bandLimit   = conf.getBandwidthLimit();
    model->threadCount = conf.getThreadCount();
    model->retryCount  = kRetryCount;
    model->firstTry    = std::time(nullptr);
    model->lastTry     = model->firstTry;
    model->userAgent   = conf.getUserAgent().toStdString();
    model->saveDir     = stdSaveDir;
    model->fileName    = utils::resolveFileName(stdUrl);

    if (db) db->insertTask(model);

    // 转换为引擎配置
    auto configure = toTaskContext(model);
    if (useProxy) {
        if (auto proxy = conf.getProxyConfig(); !proxy.isEnabled()) {
            configure->proxyUrl = proxy.toProxyUrl();
        }
    }

    auto created = EdmApplication::getInstance().getDispatcher()->createTask(configure);
    if (!created) {
        qWarning() << "SignalHandler: create task failed:" << QString::fromStdString(created.error().message());
        emit EventBus::instance() -> onTaskCreated(model);
        return;
    }
    emit EventBus::instance() -> onTaskCreated(model);
}

} // namespace edm
