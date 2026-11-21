#include "ScriptEngineState.h"

#include "ScriptingBootstrap.h"
#include "binding/ScriptBindings.h"

#include <fmt/chrono.h>
#include <fmt/format.h>

#include <chrono>
#include <ctime>
#include <filesystem>

#include "jspp/Jspp.h"

namespace edm::script {

// ---------------------------------------------------------------------------
// EngineOwnData（Data 槽位载体，RAII 清理点）
// ---------------------------------------------------------------------------

EngineOwnData::EngineOwnData(ScriptEngineState* s, ScriptingBootstrap* b) : state(s), bootstrap(b) {}

EngineOwnData::~EngineOwnData() {
    // jspp 在 runtime 释放之前调用本析构：监听器/定时器/挂起请求在此回收，
    // 长期句柄随成员析构自动释放。
    if (bootstrap) {
        bootstrap->eventBus()->removeEngineListeners(state);
    }
    stopAllTimers();
    disposeTasks();
    abortPendingCalls();
}

std::int64_t EngineOwnData::addTimer(std::function<void()> const& fn, int ms, bool singleShot) {
    if (!state || !state->services()) return -1;
    auto const handle = state->services()->startTimer(fn, ms, singleShot);
    if (handle >= 0) timerIds.insert(handle);
    return handle;
}

bool EngineOwnData::removeTimer(std::int64_t id) {
    auto const n = timerIds.erase(id);
    if (n > 0 && state && state->services()) {
        state->services()->stopTimer(id);
        return true;
    }
    return false;
}

void EngineOwnData::stopAllTimers() {
    if (state && state->services()) {
        for (auto const handle : timerIds) {
            state->services()->stopTimer(handle);
        }
    }
    timerIds.clear();
}

std::int64_t EngineOwnData::registerPendingCall(std::shared_ptr<IPendingScriptCall> call) {
    auto const id = nextCallId++;
    pendingCalls.emplace(id, std::move(call));
    return id;
}

std::shared_ptr<IPendingScriptCall> EngineOwnData::takePendingCall(std::int64_t id) {
    auto it = pendingCalls.find(id);
    if (it == pendingCalls.end()) return nullptr;
    auto call = std::move(it->second);
    pendingCalls.erase(it);
    return call;
}

bool EngineOwnData::postTask(std::function<void()> fn) {
    {
        std::lock_guard lock(tasksMutex_);
        if (tasksDisposed_) return false;
        queuedTasks_.push_back(std::move(fn));
    }
    return true;
}

std::deque<std::function<void()>> EngineOwnData::drainTasks() {
    std::lock_guard lock(tasksMutex_);
    return std::move(queuedTasks_);
}

void EngineOwnData::disposeTasks() {
    std::lock_guard lock(tasksMutex_);
    tasksDisposed_ = true;
    queuedTasks_.clear();
}

void EngineOwnData::abortPendingCalls() {
    if (pendingCalls.empty()) return;

    auto* engine = state ? state->engine() : nullptr;
    if (engine) {
        jspp::EngineScope scope(engine);
        for (auto& [id, call] : pendingCalls) {
            if (call) call->dispose();
        }
    } else {
        for (auto& [id, call] : pendingCalls) {
            if (call) call->dispose();
        }
    }
    pendingCalls.clear();
}

// ---------------------------------------------------------------------------
// ScriptEngineState
// ---------------------------------------------------------------------------

ScriptEngineState::ScriptEngineState(
    ScriptManifest      manifest,
    std::string         dir,
    int                 loadOrder,
    ScriptingBootstrap* bootstrap,
    ScriptConsole*      console,
    ScriptHostServices* services
)
: manifest_(std::move(manifest)),
  dir_(std::move(dir)),
  loadOrder_(loadOrder),
  bootstrap_(bootstrap),
  console_(console),
  services_(services),
  name_(manifest_.name),
  entryPath_(dir_ + "/" + manifest_.entry) {}

ScriptEngineState::~ScriptEngineState() {
    // 兜底：未走 teardown 的路径也要保证引擎先销毁（~EngineOwnData 负责 RAII 清理）
    engine_.reset();
}

std::optional<std::string> ScriptEngineState::loadAndCallOnLoad() {
    if (!engine_) {
        try {
            engine_ = std::make_unique<jspp::Engine>();
        } catch (std::exception const& e) {
            return fmt::format("failed to create script engine: {}", e.what());
        }
    }

    try {
        jspp::EngineScope scope(engine_.get());
        // 引擎创建点挂载数据槽（RAII 清理点见 ~EngineOwnData）
        engine_->setData(std::make_shared<EngineOwnData>(this, bootstrap_));
        engine_->registerModule(binding::edmModule());

        // 加载入口 ES module 并取 namespace（不支持 top-level await 的入口）。
        // jspp::Exception 的 message/stacktrace 须在 EngineScope 存活期内消费，
        // 因此 catch 位于 scope 之内。
        try {
            auto data          = engine_->getData<EngineOwnData>();
            auto ns            = engine_->loadModule(std::filesystem::path(entryPath_));
            auto defaultExport = ns.get(jspp::String::newString("default"));
            if (!defaultExport.isFunction()) {
                return std::string{"entry module must default-export a class"};
            }
            auto ctor = defaultExport.asFunction();
            if (!ctor.isConstructor()) {
                return std::string{"the entry module's default export must be a class"};
            }
            auto inst = ctor.callAsConstructor();
            if (!inst.isObject()) {
                return std::string{"failed to construct the entry instance"};
            }
            data->entryInstance.reset(inst.asObject());
        } catch (jspp::Exception const& e) {
            console_->append(LogLevel::Error, fmt::format("load failed: {}\n{}", e.what(), e.stacktrace()));
            return e.message();
        }
    } catch (std::exception const& e) {
        console_->append(LogLevel::Error, fmt::format("load failed: {}", e.what()));
        return e.what();
    }

    return callLifecycleHook("onLoad");
}

std::optional<std::string> ScriptEngineState::callLifecycleHook(char const* name) {
    if (!engine_) {
        return std::string{"script is not loaded"};
    }
    try {
        jspp::EngineScope scope(engine_.get());
        try {
            auto data = engine_->getData<EngineOwnData>();
            if (!data || data->entryInstance.isEmpty()) {
                return std::string{"script is not loaded"};
            }
            auto hook = data->entryInstance.get().get(jspp::String::newString(name));
            if (hook.isUndefined() || hook.isNull()) {
                return std::nullopt; // 生命周期方法是可选的
            }
            if (!hook.isFunction()) {
                return fmt::format("{} is not a function", name);
            }
            (void)hook.asFunction().call(data->entryInstance.get(), {});
        } catch (jspp::Exception const& e) {
            console_->append(LogLevel::Error, fmt::format("[{}] {}\n{}", name, e.what(), e.stacktrace()));
            return fmt::format("{}: {}", name, e.message());
        }
    } catch (std::exception const& e) {
        console_->append(LogLevel::Error, fmt::format("[{}] {}", name, e.what()));
        return fmt::format("{}: {}", name, e.what());
    }
    return std::nullopt;
}

void ScriptEngineState::teardown() {
    if (!engine_) {
        enabled_ = false;
        return;
    }

    // 尽早停掉定时器与挂起请求；监听器与句柄由 ~EngineOwnData 回收。
    if (auto data = engine_->getData<EngineOwnData>()) {
        data->stopAllTimers();
        data->abortPendingCalls();
    }
    engine_.reset();
    enabled_ = false;
}


} // namespace edm::script
