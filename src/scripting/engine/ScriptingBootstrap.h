#pragma once

#include "../api/ScriptConsole.h"
#include "../api/ScriptHostServices.h"
#include "../api/ScriptManifest.h"
#include "ScriptEventBus.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace edm::script {

class ScriptEngineState;

/// 一个被发现的脚本条目（含未加载/被禁用的）
struct DiscoveredScript {
    ScriptManifest manifest;
    std::string    dir;
    bool           loaded{false};  ///< 引擎已加载且启用
    bool           enabled{false}; ///< 配置为启用
    bool           error{false};   ///< 最近一次加载失败
    std::string    errorText;
};

/// 脚本扩展子系统编排器（仅主线程访问）。
/// 扫描脚本目录、校验 manifest，为每个启用脚本创建独立 jspp 引擎并加载入口模块。
/// 展示层（Qt/QML）通过回调与注入的 services 接入，本层不依赖任何 UI 框架。
class ScriptingBootstrap {
public:
    ScriptingBootstrap();
    ~ScriptingBootstrap();

    ScriptingBootstrap(ScriptingBootstrap const&)            = delete;
    ScriptingBootstrap& operator=(ScriptingBootstrap const&) = delete;

    /// 扫描脚本目录并加载全部启用脚本。
    void initialize(ScriptHostServices* services);
    /// 卸载全部脚本引擎（逐个 onDisable）
    void shutdown();

    [[nodiscard]] ScriptHostServices const* services() const { return services_; }
    [[nodiscard]] ScriptEventBus*           eventBus() const { return bus_.get(); }
    [[nodiscard]] std::string scriptsDir() const { return services_ ? services_->scriptsDir() : std::string{}; }

    /// 展示层刷新回调（脚本列表变化时触发，主线程）
    void setScriptsChangedHandler(std::function<void()> handler) { onChanged_ = std::move(handler); }

    // ---- 发现/状态 ----
    std::vector<DiscoveredScript> discoverScripts() const;
    ScriptEngineState*            findEngine(std::string const& name) const;

    // ---- 管理 ----
    /// 从外部目录导入脚本包（校验 manifest 后拷入脚本目录并加载）
    /// @return 失败时返回错误说明
    std::optional<std::string> importScript(std::string const& sourceDir);
    /// 卸载并物理删除插件：先安全禁用（onDisable + teardown 回收监听器/命令/定时器），
    /// 再从禁用配置移除并删除插件目录。目录经发现列表解析，不接受外部路径。
    /// @return 失败时返回错误说明
    std::optional<std::string> uninstallScript(std::string const& name);
    bool                       setEnabled(std::string const& name, bool enabled);
    bool                       reloadScript(std::string const& name);
    void                       reloadAll();
    bool                       loadScript(std::string const& name);
    void                       unloadScript(std::string const& name);

    // ---- 虚拟控制台 ----
    /// 每个脚本一份控制台缓冲；脚本未加载/已卸载时依然存在（保留历史）。
    ScriptConsole*                 console(std::string const& name) const;
    std::shared_ptr<ScriptConsole> ensureConsole(std::string const& name);
    void                           clearConsole(std::string const& name);

    /// 引擎泵：推进各引擎的微任务与挂起 Promise（宿主低频驱动）
    void pumpEngines();

private:
    void teardownEngine(ScriptEngineState* state);
    void reportLoadError(std::string const& name, std::string const& message);
    void clearLoadError(std::string const& name);
    void emitChanged();

    ScriptHostServices*                                   services_{nullptr};
    std::unique_ptr<ScriptEventBus>                       bus_;
    std::vector<std::unique_ptr<ScriptEngineState>>       engines_;
    std::map<std::string, std::shared_ptr<ScriptConsole>> consoles_;
    std::map<std::string, std::string>                    lastLoadErrors_;
    std::function<void()>                                 onChanged_;
};

} // namespace edm::script
