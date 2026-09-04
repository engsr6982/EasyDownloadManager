#include "EdmApplication.h"

#include "Dispatcher.h"
#include "EdmConfig.h"
#include "ITaskObserver.h"
#include "database/DownloadDatabase.h"
#include "database/TaskModel.h"
#include "database/TaskModelConversion.h"
#include "qml/QmlController.h"
#include "qml/QmlSettings.h"
#include "qml/QmlTaskListModel.h"
#include "utils/Utils.h"

#include <ctime>

#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QMenu>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSystemTrayIcon>
#include <QUrl>

#include <qqml.h>
#include <winsock2.h>

namespace edm {


class TaskObserverImpl final : public ITaskObserver {
public:
    void onStateChanged(std::shared_ptr<TaskContext>& ctx, TaskState oldState) override {
        // TODO: Implement this pure virtual method.
    }

    void onError(std::shared_ptr<TaskContext>& ctx, const std::string& error) override {
        // TODO: Implement this pure virtual method.
    }

    void onFileNameChanged(std::shared_ptr<TaskContext>& ctx) override {
        // TODO: Implement this pure virtual method.
    }
};


struct EdmApplication::Impl {
    std::unique_ptr<DownloadDatabase>      database_{nullptr};
    std::unique_ptr<Dispatcher>            dispatcher_{nullptr};
    std::unique_ptr<QQmlApplicationEngine> qmlEngine_{nullptr};
    std::unique_ptr<QSystemTrayIcon>       trayIcon_{nullptr};
    std::unique_ptr<TaskObserverImpl>      taskObserver_{nullptr};

    // QML bridge objects (owned by the application)
    QmlTaskListModel* taskListModel_{nullptr};
    QmlSettings*      settingsModel_{nullptr};
    QmlController*    controller_{nullptr};

    QQuickWindow* mainWindow_{nullptr};

    void init() {
        // 1. Database
        database_ = std::make_unique<DownloadDatabase>();

        // 2. Task observer
        taskObserver_ = std::make_unique<TaskObserverImpl>();
        // dispatcherOptions.taskLoader = [this](TaskId id) -> std::shared_ptr<TaskContext> {
        //     if (!database_) return nullptr;
        //     auto model = database_->getTaskById(id);
        //     if (!model) return nullptr;
        //     auto header = database_->getTaskHeaderById(id);
        //     return toTaskContext(model, header);
        // };
        // dispatcherOptions.onTaskChanged = [this](std::shared_ptr<TaskContext> const& cfg) {
        //     if (!database_ || !cfg) return;
        //     // 从数据库加载已有模型，如果不存在则创建
        //     auto model = database_->getTaskById(cfg->id);
        //     if (!model) {
        //         model           = TaskModel::make();
        //         model->id       = cfg->id;
        //         model->url      = cfg->url;
        //         model->saveDir  = cfg->saveDir;
        //         model->fileName = utils::resolveFileName(cfg->url);
        //         model->category = utils::resolveFileCategory(model->fileName);
        //         model->firstTry = std::time(nullptr);
        //     }
        //     model->lastTry = std::time(nullptr); // 每次状态变更都更新时间戳
        //     applyContextToModel(*model, *cfg);
        //     database_->insertTask(model);
        // };
        dispatcher_ = std::make_unique<Dispatcher>(taskObserver_.get());

        // 3. QML engine
        qmlEngine_ = std::make_unique<QQmlApplicationEngine>();

        // Register singletons so QML can access them via their type names.
        taskListModel_ = new QmlTaskListModel();
        taskListModel_->setDispatcher(dispatcher_.get());
        taskListModel_->setDatabase(database_.get());

        settingsModel_ = new QmlSettings();
        controller_    = new QmlController();

        // The objects are parented to the engine for cleanup.
        taskListModel_->setParent(qmlEngine_.get());
        settingsModel_->setParent(qmlEngine_.get());
        controller_->setParent(qmlEngine_.get());

        qmlRegisterSingletonInstance("edm.qml.ui", 1, 0, "TaskListModel", taskListModel_);
        qmlRegisterSingletonInstance("edm.qml.ui", 1, 0, "SettingsModel", settingsModel_);
        qmlRegisterSingletonInstance("edm.qml.ui", 1, 0, "AppController", controller_);

        QObject::connect(
            qmlEngine_.get(),
            &QQmlApplicationEngine::objectCreationFailed,
            QCoreApplication::instance(),
            [](const QUrl& url) { qCritical() << "[ERROR] Object creation failed: " << url; },
            Qt::QueuedConnection
        );
        QObject::connect(qmlEngine_.get(), &QQmlApplicationEngine::warnings, [](const QList<QQmlError>& warnings) {
            for (const auto& error : warnings) {
                qCritical() << "[WARN] " << error.toString();
            }
        });

        QUrl const url(u"qrc:/qt/qml/edm/qml/ui/EasyDownloadManagerUI/Src/App.qml"_qs);
        qmlEngine_->load(url);

        // Get root window for tray icon interaction
        auto roots = qmlEngine_->rootObjects();
        if (!roots.isEmpty()) {
            mainWindow_ = qobject_cast<QQuickWindow*>(roots.first());
            mainWindow_->setIcon(QIcon(":/assets/icons/logo.ico"));
        }

        // 6. Load persisted tasks into the model
        taskListModel_->reloadFromDatabase();

        // 7. Start the live refresh timer
        taskListModel_->startRefresh(1000);

        // 8. System tray icon
        _initSystemTrayIcon();
    }

    void destroy() {
        taskListModel_->stopRefresh();
        trayIcon_.reset();
        mainWindow_ = nullptr;
        qmlEngine_.reset();
        // The objects are parented to the engine for cleanup.
        taskListModel_ = nullptr;
        settingsModel_ = nullptr;
        controller_    = nullptr;
        dispatcher_.reset();
        taskObserver_.reset();
        database_.reset();
    }

    void _initSystemTrayIcon() {
        if (!QSystemTrayIcon::isSystemTrayAvailable()) {
            qDebug() << "System tray is not available";
            return;
        }

        trayIcon_ = std::make_unique<QSystemTrayIcon>(QIcon(":/assets/icons/logo.ico"));
        auto menu = new QMenu();
        auto show = menu->addAction(QStringLiteral("显示主窗口"));
        auto exit = menu->addAction(QStringLiteral("退出"));
        trayIcon_->setContextMenu(menu);
        trayIcon_->show();

        QObject::connect(show, &QAction::triggered, [this]() {
            if (mainWindow_) {
                mainWindow_->showNormal();
                mainWindow_->requestActivate();
            }
        });

        QObject::connect(exit, &QAction::triggered, []() { QApplication::quit(); });

        QObject::connect(
            trayIcon_.get(),
            &QSystemTrayIcon::activated,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger) {
                    if (mainWindow_) {
                        mainWindow_->showNormal();
                        mainWindow_->requestActivate();
                    }
                }
            }
        );
    }
};

EdmApplication::EdmApplication() : impl(std::make_unique<Impl>()) {}
EdmApplication::~EdmApplication() = default;

EdmApplication& EdmApplication::getInstance() {
    static EdmApplication instance;
    return instance;
}

void EdmApplication::initApp() { impl->init(); }
void EdmApplication::destroyApp() { impl->destroy(); }

Dispatcher*            EdmApplication::getDispatcher() const { return impl->dispatcher_.get(); }
DownloadDatabase*      EdmApplication::getDatabase() const { return impl->database_.get(); }
QSystemTrayIcon*       EdmApplication::getTrayIcon() const { return impl->trayIcon_.get(); }
QQmlApplicationEngine* EdmApplication::getQmlEngine() const { return impl->qmlEngine_.get(); }

QQuickWindow* EdmApplication::getMainWindow() const { return impl->mainWindow_; }

QmlTaskListModel* EdmApplication::getTaskListModel() const { return impl->taskListModel_; }
QmlSettings*      EdmApplication::getSettingsModel() const { return impl->settingsModel_; }
QmlController*    EdmApplication::getController() const { return impl->controller_; }

} // namespace edm
