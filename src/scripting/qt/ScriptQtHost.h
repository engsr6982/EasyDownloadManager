#pragma once

#include "../api/ScriptHostServices.h"

#include <QObject>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace edm::script::qt {

/// ScriptHostServices 的 Qt 实现：QTimer 定时器/事件泵；环境值由宿主填充，
/// notify/task 查询经回调中转给宿主对象（托盘、数据库等）。
class QtHostServices : public QObject, public ScriptHostServices {
    Q_OBJECT

public:
    explicit QtHostServices(QObject* parent = nullptr);
    ~QtHostServices() override;

    QtHostServices(QtHostServices const&)            = delete;
    QtHostServices& operator=(QtHostServices const&) = delete;

    // ---- ScriptHostServices ----
    [[nodiscard]] std::string                 scriptsDir() const override;
    [[nodiscard]] std::string                 appDataDir() const override;
    [[nodiscard]] std::string                 userAgent() const override;
    [[nodiscard]] int                         hookTimeoutMs() const override;
    [[nodiscard]] std::vector<std::string>    disabledScripts() const override;
    void                                      setDisabledScripts(std::vector<std::string> const& names) override;
    [[nodiscard]] std::optional<TaskSnapshot> taskGet(int id) const override;
    [[nodiscard]] std::vector<TaskSnapshot>   taskGetAll() const override;
    bool                                      taskControl(int id, TaskAction action) override;
    std::int64_t startTimer(std::function<void()> const& fn, int ms, bool singleShot) override;
    void         stopTimer(std::int64_t handle) override;
    std::int64_t registerCommand(
        std::string const&           source,
        std::string const&           title,
        std::string const&           description,
        std::function<void()> const& fn
    ) override;
    void unregisterCommand(std::int64_t handle) override;
    void setPumpHandler(std::function<void()> const& fn, int intervalMs) override;
    void notify(NotifyLevel level, std::string const& title, std::string const& message) override;
    void logWarning(std::string const& message) override;

    // ---- 宿主填充的环境值 ----
    std::string scriptsDirValue;
    std::string appDataDirValue;
    std::string userAgentValue;
    int         hookTimeoutMsValue{10000};

    // ---- 宿主回调 ----
    std::function<std::vector<std::string>()>            disabledScriptsHandler;
    std::function<void(std::vector<std::string> const&)> setDisabledScriptsHandler;
    std::function<std::optional<TaskSnapshot>(int)>      taskGetHandler;
    std::function<std::vector<TaskSnapshot>()>           taskGetAllHandler;
    std::function<bool(int, TaskAction)>                 taskControlHandler;
    std::function<
        std::int64_t(std::string const&, std::string const&, std::string const&, std::function<void()> const&)>
                                                                             commandRegisterHandler;
    std::function<void(std::int64_t)>                                        commandUnregisterHandler;
    std::function<void(NotifyLevel, std::string const&, std::string const&)> notifyHandler;
    std::function<void(std::string const&)>                                  warningHandler;

private:
    struct TimerHost;
    std::shared_ptr<TimerHost> timerHost_;
};

} // namespace edm::script::qt
