#pragma once

#include "../api/ScriptConsole.h"
#include "../api/ScriptHostServices.h"
#include "../api/ScriptManifest.h"

// EngineOwnData 的 Global<Object> 成员需要完整类型
#include "jspp/core/Reference.h"
#include "jspp/core/Value.h"

#include <atomic>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace jspp {
class Engine;
} // namespace jspp

namespace edm::script {

class ScriptingBootstrap;
class ScriptEventBus;

/// 挂起的异步调用（http 等）的回收接口。
/// dispose() 必须在对应引擎的 EngineScope 内调用（内部持有引擎句柄）。
struct IPendingScriptCall {
    virtual ~IPendingScriptCall() = default;
    virtual void dispose()        = 0;
};

/// 引擎附加数据（jspp Engine::setData 载体）。引擎创建点挂载；
/// jspp 保证其析构先于 runtime 释放，析构函数即全部 JS 资源的回收点。
/// 绑定层经 jspp::Engine::getData<EngineOwnData>() 访问。
struct EngineOwnData {
    class ScriptEngineState* state{nullptr};
    ScriptingBootstrap*      bootstrap{nullptr};

    // ---- 长期 JS 句柄（随本对象析构，在 runtime 存活期内释放）----
    jspp::Global<jspp::Object> entryInstance; // default export 类实例

    // ---- 注册表 ----
    std::set<std::int64_t>                                      timerIds;
    std::set<std::int64_t>                                      commandIds;
    std::map<std::int64_t, std::shared_ptr<IPendingScriptCall>> pendingCalls;
    std::int64_t                                                nextCallId{1};

    EngineOwnData(ScriptEngineState* s, ScriptingBootstrap* b);
    ~EngineOwnData(); // 统一清理（RAII），实现见 ScriptEngineState.cc

    EngineOwnData(EngineOwnData const&)            = delete;
    EngineOwnData& operator=(EngineOwnData const&) = delete;

    // ---- 定时器（经宿主 services 调度）----
    std::int64_t addTimer(std::function<void()> const& fn, int ms, bool singleShot);
    bool         removeTimer(std::int64_t id);
    void         stopAllTimers();

    // ---- 命令面板（经宿主 services 注册；析构时统一注销）----
    std::int64_t addCommand(
        std::string const&           source,
        std::string const&           title,
        std::string const&           description,
        std::function<void()> const& fn
    );
    bool removeCommand(std::int64_t id);
    void unregisterAllCommands();

    // ---- 挂起的异步调用 ----
    std::int64_t                        registerPendingCall(std::shared_ptr<IPendingScriptCall> call);
    std::shared_ptr<IPendingScriptCall> takePendingCall(std::int64_t id);
    /// 在 EngineScope 内释放全部挂起调用并标记失效（幂等）
    void abortPendingCalls();

    // ---- 工作线程到主线程的任务投递 ----
    /// 线程安全；引擎已销毁（或正在销毁）时返回 false 并丢弃任务
    bool postTask(std::function<void()> fn);
    /// 取出全部待执行任务（主线程；返回后须在 EngineScope 内执行）
    std::deque<std::function<void()>> drainTasks();
    /// 销毁：丢弃全部待执行任务并拒绝后续投递
    void disposeTasks();

private:
    std::mutex                        tasksMutex_;
    std::deque<std::function<void()>> queuedTasks_;
    bool                              tasksDisposed_{false};
};

/// 单个脚本的运行时状态：独立 jspp 引擎 + 各类登记，仅主线程访问。
class ScriptEngineState {
public:
    /// @param console 控制台日志缓冲（由 ScriptingBootstrap 持有，生命周期长于引擎，
    ///                脚本卸载后仍可回看历史）
    ScriptEngineState(
        ScriptManifest      manifest,
        std::string         dir,
        int                 loadOrder,
        ScriptingBootstrap* bootstrap,
        ScriptConsole*      console,
        ScriptHostServices* services
    );
    ~ScriptEngineState();

    ScriptEngineState(ScriptEngineState const&)            = delete;
    ScriptEngineState& operator=(ScriptEngineState const&) = delete;

    /// 创建引擎、挂载 EngineOwnData、注册 "edm" 模块、加载入口模块并调用 onLoad()。
    /// @return 失败时返回错误描述（引擎内部状态保持可安全销毁）。
    std::optional<std::string> loadAndCallOnLoad();

    /// 调用入口实例上的生命周期方法；失败返回错误描述。
    std::optional<std::string> callLifecycleHook(char const* name);

    /// 卸载：停掉定时器/挂起请求并销毁引擎（监听器与句柄由 ~EngineOwnData 回收）。
    /// onDisable() 应由调用方在本方法之前调用。
    void teardown();

    // ---- 访问器 ----
    [[nodiscard]] ScriptManifest const& manifest() const { return manifest_; }
    [[nodiscard]] std::string const&    name() const { return name_; }
    [[nodiscard]] std::string const&    dir() const { return dir_; }
    [[nodiscard]] std::string const&    entryPath() const { return entryPath_; }
    [[nodiscard]] int                   loadOrder() const { return loadOrder_; }
    [[nodiscard]] ScriptConsole*        console() const { return console_; }
    [[nodiscard]] jspp::Engine*         engine() const { return engine_.get(); }
    [[nodiscard]] ScriptingBootstrap*   bootstrap() const { return bootstrap_; }
    [[nodiscard]] ScriptHostServices*   services() const { return services_; }
    [[nodiscard]] bool                  enabled() const { return enabled_; }
    void                                setEnabled(bool v) { enabled_ = v; }

private:
    ScriptManifest      manifest_;
    std::string         name_;
    std::string         dir_;
    std::string         entryPath_;
    int                 loadOrder_{0};
    bool                enabled_{false};
    ScriptingBootstrap* bootstrap_{nullptr};
    ScriptConsole*      console_{nullptr}; // 非持有：由 ScriptingBootstrap 统一管理
    ScriptHostServices* services_{nullptr};

    std::unique_ptr<jspp::Engine> engine_;
};

} // namespace edm::script
