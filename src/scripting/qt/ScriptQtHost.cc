#include "ScriptQtHost.h"

#include <QHash>
#include <QTimer>

#include <utility>

namespace edm::script::qt {

namespace {
constexpr std::int64_t kInvalidTimerHandle = -1;
} // namespace

struct QtHostServices::TimerHost {
    QObject*                     owner{nullptr};
    QHash<std::int64_t, QTimer*> timers;
    std::int64_t                 nextId{1};
    QTimer*                      pump{nullptr};
};

QtHostServices::QtHostServices(QObject* parent) : QObject(parent), timerHost_(std::make_shared<TimerHost>()) {
    timerHost_->owner = this;
}

QtHostServices::~QtHostServices() = default;

std::string QtHostServices::scriptsDir() const { return scriptsDirValue; }
std::string QtHostServices::appDataDir() const { return appDataDirValue; }
std::string QtHostServices::userAgent() const { return userAgentValue; }
int         QtHostServices::hookTimeoutMs() const { return hookTimeoutMsValue; }

std::vector<std::string> QtHostServices::disabledScripts() const {
    return disabledScriptsHandler ? disabledScriptsHandler() : std::vector<std::string>{};
}

void QtHostServices::setDisabledScripts(std::vector<std::string> const& names) {
    if (setDisabledScriptsHandler) setDisabledScriptsHandler(names);
}

std::optional<TaskSnapshot> QtHostServices::taskGet(int id) const {
    return taskGetHandler ? taskGetHandler(id) : std::nullopt;
}

std::vector<TaskSnapshot> QtHostServices::taskGetAll() const {
    return taskGetAllHandler ? taskGetAllHandler() : std::vector<TaskSnapshot>{};
}

bool QtHostServices::taskControl(int id, TaskAction action) {
    return taskControlHandler ? taskControlHandler(id, action) : false;
}

std::int64_t QtHostServices::registerCommand(
    std::string const&           source,
    std::string const&           title,
    std::string const&           description,
    std::function<void()> const& fn
) {
    return commandRegisterHandler ? commandRegisterHandler(source, title, description, fn) : -1;
}

void QtHostServices::unregisterCommand(std::int64_t handle) {
    if (commandUnregisterHandler) commandUnregisterHandler(handle);
}

std::int64_t QtHostServices::startTimer(std::function<void()> const& fn, int ms, bool singleShot) {
    auto* timer = new QTimer(timerHost_->owner);
    timer->setSingleShot(singleShot);
    auto const handle = timerHost_->nextId++;
    timerHost_->timers.insert(handle, timer);
    QObject::connect(timer, &QTimer::timeout, timerHost_->owner, [host = timerHost_, handle, singleShot, fn] {
        if (singleShot) {
            if (auto* t = host->timers.take(handle)) t->deleteLater();
        }
        fn();
    });
    timer->start(ms);
    return handle;
}

void QtHostServices::stopTimer(std::int64_t handle) {
    if (auto* timer = timerHost_->timers.take(handle)) {
        timer->stop();
        timer->deleteLater();
    }
}

void QtHostServices::setPumpHandler(std::function<void()> const& fn, int intervalMs) {
    if (timerHost_->pump) {
        timerHost_->pump->stop();
        timerHost_->pump->deleteLater();
    }
    timerHost_->pump = new QTimer(timerHost_->owner);
    timerHost_->pump->setInterval(intervalMs);
    QObject::connect(timerHost_->pump, &QTimer::timeout, timerHost_->owner, fn);
    timerHost_->pump->start();
}

void QtHostServices::notify(NotifyLevel level, std::string const& title, std::string const& message) {
    if (notifyHandler) notifyHandler(level, title, message);
}

void QtHostServices::logWarning(std::string const& message) {
    if (warningHandler) warningHandler(message);
}

} // namespace edm::script::qt
