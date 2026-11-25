#include "ScriptingBootstrap.h"

#include "ScriptEngineState.h"


#include <fmt/format.h>

#include <algorithm>
#include <filesystem>

#include "jspp/Jspp.h"
#ifdef JSPP_BACKEND_QUICKJS
#include "jspp-backend/queue/JobQueue.h"
#endif

namespace edm::script {

ScriptingBootstrap::ScriptingBootstrap() { bus_ = std::make_unique<ScriptEventBus>(nullptr); }

ScriptingBootstrap::~ScriptingBootstrap() { shutdown(); }

void ScriptingBootstrap::initialize(ScriptHostServices* services) {
    services_ = services;
    bus_      = std::make_unique<ScriptEventBus>(services);

    services_->setPumpHandler([this] { pumpEngines(); }, 50);

    auto scripts = discoverScripts();
    for (auto const& script : scripts) {
        if (script.enabled) {
            loadScript(script.manifest.name);
        }
    }
    emitChanged();
}

void ScriptingBootstrap::shutdown() {
    onChanged_   = nullptr; // 展示层可能先于本对象析构，先断开回调
    auto engines = std::move(engines_);
    engines_.clear();
    for (auto& engine : engines) {
        teardownEngine(engine.get());
    }
}

// ---------------------------------------------------------------------------
// 发现
// ---------------------------------------------------------------------------

std::vector<DiscoveredScript> ScriptingBootstrap::discoverScripts() const {
    std::vector<DiscoveredScript> result;
    if (!services_) return result; // 未初始化（initialize 之前 QML 可能触发刷新）
    auto const disabled = services_->disabledScripts();

    std::error_code             fsEc;
    std::filesystem::path const root{scriptsDir()};
    if (!std::filesystem::exists(root, fsEc)) return result;

    std::vector<std::string> folders;
    for (auto const& entry : std::filesystem::directory_iterator(root, fsEc)) {
        if (entry.is_directory()) folders.push_back(entry.path().filename().string());
    }
    std::sort(folders.begin(), folders.end());

    for (auto const& folder : folders) {
        auto const subDir = (root / folder).string();
        auto       loaded = loadManifest(subDir);
        if (!loaded) {
            services_->logWarning(fmt::format("invalid manifest in {}: {}", subDir, loaded.error().message()));
            continue;
        }
        DiscoveredScript item;
        item.manifest = std::move(*loaded);
        item.dir      = subDir;
        item.enabled  = std::find(disabled.begin(), disabled.end(), item.manifest.name) == disabled.end();
        item.loaded   = findEngine(item.manifest.name) != nullptr;
        if (auto it = lastLoadErrors_.find(item.manifest.name); it != lastLoadErrors_.end()) {
            item.error     = true;
            item.errorText = it->second;
        }
        result.push_back(std::move(item));
    }
    return result;
}

ScriptEngineState* ScriptingBootstrap::findEngine(std::string const& name) const {
    for (auto const& engine : engines_) {
        if (engine->name() == name) return engine.get();
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// 加载 / 卸载
// ---------------------------------------------------------------------------

bool ScriptingBootstrap::loadScript(std::string const& name) {
    if (findEngine(name)) return true; // 幂等

    // 重扫目录取 manifest（支持外部改动后热加载）
    auto scripts = discoverScripts();
    auto it      = std::find_if(scripts.begin(), scripts.end(), [&](DiscoveredScript const& s) {
        return s.manifest.name == name;
    });
    if (it == scripts.end()) {
        reportLoadError(name, fmt::format("script not found in {}", scriptsDir()));
        emitChanged();
        return false;
    }
    if (!it->enabled) {
        reportLoadError(name, "script is disabled in configuration");
        emitChanged();
        return false;
    }

    auto* console = ensureConsole(name).get();
    console->append(LogLevel::Info, fmt::format("loading {} v{} ...", name, it->manifest.version));

    auto state = std::make_unique<ScriptEngineState>(
        it->manifest,
        it->dir,
        static_cast<int>(engines_.size()),
        this,
        console,
        services_
    );
    if (auto error = state->loadAndCallOnLoad()) {
        console->append(LogLevel::Error, fmt::format("load failed: {}", *error));
        reportLoadError(name, *error);
        state->teardown();
        emitChanged();
        return false;
    }
    if (auto error = state->callLifecycleHook("onEnable")) {
        console->append(LogLevel::Error, fmt::format("enable failed: {}", *error));
        reportLoadError(name, *error);
        state->callLifecycleHook("onDisable");
        state->teardown();
        emitChanged();
        return false;
    }
    state->setEnabled(true);
    clearLoadError(name);
    console->append(LogLevel::Info, "enabled");
    engines_.push_back(std::move(state));
    emitChanged();
    return true;
}

void ScriptingBootstrap::teardownEngine(ScriptEngineState* state) {
    // onDisable 在脚本尚存活时调用；监听器与句柄由 ~EngineOwnData 回收
    state->callLifecycleHook("onDisable");
    state->teardown();
}

void ScriptingBootstrap::unloadScript(std::string const& name) {
    auto it =
        std::find_if(engines_.begin(), engines_.end(), [&](auto const& engine) { return engine->name() == name; });
    if (it == engines_.end()) return; // 幂等

    if (auto* console = (*it)->console()) console->append(LogLevel::Info, "disabled");
    teardownEngine(it->get());
    engines_.erase(it);
    emitChanged();
}

bool ScriptingBootstrap::setEnabled(std::string const& name, bool enabled) {
    auto disabled = services_->disabledScripts();
    if (enabled) {
        std::erase(disabled, name);
    } else if (std::find(disabled.begin(), disabled.end(), name) == disabled.end()) {
        disabled.push_back(name);
    }
    services_->setDisabledScripts(disabled);

    if (enabled) return loadScript(name);
    unloadScript(name);
    return true;
}

std::optional<std::string> ScriptingBootstrap::importScript(std::string const& sourceDir) {
    auto loaded = loadManifest(sourceDir);
    if (!loaded) return loaded.error().message();

    auto const      name = loaded->name;
    std::error_code fsEc;
    auto const      dst = (std::filesystem::path{scriptsDir()} / name);
    if (std::filesystem::exists(dst, fsEc)) {
        return fmt::format("script \"{}\" already exists", name);
    }
    std::filesystem::create_directories(scriptsDir(), fsEc);
    std::filesystem::copy(
        sourceDir,
        dst,
        std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing,
        fsEc
    );
    if (fsEc) {
        return fmt::format("copy failed: {}", fsEc.message());
    }

    if (!loadScript(name)) {
        auto it = lastLoadErrors_.find(name);
        return it != lastLoadErrors_.end() ? it->second : fmt::format("load script \"{}\" failed", name);
    }
    return std::nullopt;
}

std::optional<std::string> ScriptingBootstrap::uninstallScript(std::string const& name) {
    // 目录取自发现列表（scriptsDir 的直接子目录），不信任外部传入的路径
    auto scripts = discoverScripts();
    auto it      = std::find_if(scripts.begin(), scripts.end(), [&](DiscoveredScript const& s) {
        return s.manifest.name == name;
    });
    if (it == scripts.end()) {
        return fmt::format("script \"{}\" not found", name);
    }

    // 已加载则安全禁用：onDisable + teardown 回收监听器/命令/定时器，此后才动磁盘
    unloadScript(name);

    // 从禁用配置移除，避免残留指向已删除插件的条目
    auto disabled = services_->disabledScripts();
    if (std::erase(disabled, name) > 0) {
        services_->setDisabledScripts(disabled);
    }

    std::error_code fsEc;
    std::filesystem::remove_all(it->dir, fsEc);
    if (fsEc) {
        return fmt::format("delete {} failed: {}", it->dir, fsEc.message());
    }

    clearLoadError(name);
    emitChanged();
    return std::nullopt;
}

bool ScriptingBootstrap::reloadScript(std::string const& name) {
    unloadScript(name);
    return loadScript(name);
}

void ScriptingBootstrap::reloadAll() {
    auto engines = std::move(engines_);
    engines_.clear();
    for (auto& engine : engines) {
        teardownEngine(engine.get());
    }

    for (auto const& script : discoverScripts()) {
        if (script.enabled) {
            loadScript(script.manifest.name);
        }
    }
    emitChanged();
}

// ---------------------------------------------------------------------------
// 控制台
// ---------------------------------------------------------------------------

ScriptConsole* ScriptingBootstrap::console(std::string const& name) const {
    auto it = consoles_.find(name);
    return it == consoles_.end() ? nullptr : it->second.get();
}

std::shared_ptr<ScriptConsole> ScriptingBootstrap::ensureConsole(std::string const& name) {
    auto it = consoles_.find(name);
    if (it != consoles_.end()) return it->second;
    auto [pos, inserted] = consoles_.try_emplace(name, std::make_shared<ScriptConsole>());
    (void)inserted;
    return pos->second;
}

void ScriptingBootstrap::clearConsole(std::string const& name) {
    if (auto* console = this->console(name)) console->clear();
}

// ---------------------------------------------------------------------------
// 泵 / 错误登记
// ---------------------------------------------------------------------------

void ScriptingBootstrap::pumpEngines() {
    for (auto const& engine : engines_) {
        auto* e = engine->engine();
        if (!e) continue;
        jspp::EngineScope scope(e);
        if (auto data = e->getData<EngineOwnData>()) {
            for (auto& task : data->drainTasks()) {
                task();
            }
        }
        e->pumpPendingJobs();
        while (e->getJobQueue()->loopOnce()) {}
    }
}

void ScriptingBootstrap::reportLoadError(std::string const& name, std::string const& message) {
    lastLoadErrors_[name] = message;
    services_->logWarning(fmt::format("[scripting] {}: {}", name, message));
}

void ScriptingBootstrap::clearLoadError(std::string const& name) { lastLoadErrors_.erase(name); }

void ScriptingBootstrap::emitChanged() {
    if (onChanged_) onChanged_();
}

} // namespace edm::script
