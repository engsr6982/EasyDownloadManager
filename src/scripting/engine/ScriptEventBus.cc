#include "ScriptEventBus.h"

#include "ScriptEngineState.h"

#include <fmt/format.h>

#include <chrono>
#include <thread>

#include "jspp/Jspp.h"
#include "jspp/binding/MetaBuilder.h"
#include "jspp/binding/TypeConverter.h"
#ifdef JSPP_BACKEND_QUICKJS
#include "jspp-backend/queue/JobQueue.h"
#endif

namespace edm::script {

// ---------------------------------------------------------------------------
// 钩子元数据
// ---------------------------------------------------------------------------

namespace {

constexpr char const* kHookNames[] = {
    "beforeCreateTask",
    "beforeFetchMeta",
    "afterFetchMeta",
    "taskStarted",
    "taskProgress",
    "taskPaused",
    "taskResumed",
    "taskCompleted",
    "taskFailed",
    "taskCanceled",
    "taskRemoved",
};
static_assert(std::size(kHookNames) == static_cast<size_t>(HookId::Count));

/// 监听器回调抛出的异常写入脚本控制台（异常隔离）
void reportScriptError(
    ScriptEngineState* state,
    char const*        hook,
    std::string const& message,
    std::string const& stacktrace
) {
    auto* console = state ? state->console() : nullptr;
    if (!console) return;
    console->append(LogLevel::Error, fmt::format("[{}] {}\n{}", hook, message, stacktrace));
}

/// 等待脚本返回的 Promise settle：驱动引擎任务队列，期间不持有 EngineScope
/// （跨线程 resolve 需要）。超时放弃并告警。
void waitPromiseSettled(
    jspp::Engine*              engine,
    jspp::Local<jspp::Promise> promise,
    int                        timeoutMs,
    char const*                hook,
    ScriptConsole*             console
) {
    struct SettleState {
        std::atomic_bool done{false};
        std::atomic_bool rejected{false};
        std::string      error;
    };
    auto settle = std::make_shared<SettleState>();

    try {
        auto onFulfilled = jspp::Function::newFunction([settle](jspp::Arguments const&) -> jspp::Local<jspp::Value> {
            settle->done.store(true, std::memory_order_release);
            return jspp::Undefined::newUndefined();
        });
        auto onRejected =
            jspp::Function::newFunction([settle](jspp::Arguments const& args) -> jspp::Local<jspp::Value> {
                settle->rejected.store(true, std::memory_order_release);
                settle->done.store(true, std::memory_order_release);
                if (args.length() > 0) {
                    try {
                        settle->error = args[0].isString() ? args[0].asString().getValue()
                                                           : jspp::binding::toJs(args[0]).toString().getValue();
                    } catch (...) {
                        settle->error = "<unprintable rejection>";
                    }
                }
                return jspp::Undefined::newUndefined();
            });
        (void)promise.then(onFulfilled, onRejected);
    } catch (jspp::Exception const& e) {
        if (console) console->append(LogLevel::Warn, fmt::format("failed to attach promise handler: {}", e.what()));
        return;
    }

    auto const deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{timeoutMs};
    while (!settle->done.load(std::memory_order_acquire)) {
        {
            jspp::EngineScope scope(engine);
            if (auto data = engine->getData<EngineOwnData>()) {
                for (auto& task : data->drainTasks()) {
                    task();
                }
            }
            engine->pumpPendingJobs();
            while (engine->getJobQueue()->loopOnce()) {}
        }
        if (settle->done.load(std::memory_order_acquire)) break;
        if (std::chrono::steady_clock::now() >= deadline) {
            if (console) {
                console->append(
                    LogLevel::Warn,
                    fmt::format(
                        "[{}] async handler timed out after {}ms; continuing with current values",
                        hook,
                        timeoutMs
                    )
                );
            }
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }

    if (settle->rejected.load(std::memory_order_acquire) && console) {
        console->append(LogLevel::Error, fmt::format("[{}] async handler promise rejected: {}", hook, settle->error));
    }
}

/// 事件对象以 shared_ptr 进 NativeInstance：异步 handler 的闭包可延长其寿命。
template <typename E>
jspp::Local<jspp::Object> wrapEvent(std::shared_ptr<E> const& ev) {
    auto value = jspp::binding::toJs(ev, jspp::binding::ReturnValuePolicy::kAutomatic, jspp::Local<jspp::Value>{});
    return value.asObject();
}

} // namespace

char const* hookName(HookId id) {
    auto const idx = static_cast<int>(id);
    if (idx < 0 || idx >= static_cast<int>(HookId::Count)) return "<unknown>";
    return kHookNames[idx];
}

std::optional<HookId> hookFromName(std::string_view name) {
    for (std::size_t i = 0; i < std::size(kHookNames); ++i) {
        if (name == kHookNames[i]) return static_cast<HookId>(i);
    }
    return std::nullopt;
}

bool isMutableHook(HookId id) {
    return id == HookId::BeforeCreateTask || id == HookId::BeforeFetchMeta || id == HookId::AfterFetchMeta;
}

bool isCancellableHook(HookId id) { return isMutableHook(id); }

bool hookSupportsAsync(HookId id) { return id == HookId::BeforeFetchMeta || id == HookId::AfterFetchMeta; }

// ---------------------------------------------------------------------------
// 注册表
// ---------------------------------------------------------------------------

void ScriptEventBus::insertListener(Listener&& listener) {
    auto const key = std::pair<int, uint64_t>{static_cast<int>(listener.priority), listener.seq};
    auto       pos = listeners_.begin();
    while (pos != listeners_.end()) {
        auto const cur = std::pair<int, uint64_t>{static_cast<int>(pos->priority), pos->seq};
        if (cur > key) break;
        ++pos;
    }
    listeners_.insert(pos, std::move(listener));
}

uint32_t ScriptEventBus::addListener(
    HookId                             hook,
    jspp::Local<jspp::Function> const& fn,
    ListenerPriority                   priority,
    ScriptEngineState*                 engine
) {
    Listener l;
    l.id       = nextListenerId_++;
    l.hook     = hook;
    l.priority = priority;
    l.seq      = nextSeq_++;
    l.engine   = engine;
    l.fn       = jspp::TrackedGlobal<jspp::Function>::create(jspp::Local<jspp::Function>(fn));
    insertListener(std::move(l));
    return l.id;
}

bool ScriptEventBus::removeListener(uint32_t id) {
    auto const n = std::erase_if(listeners_, [id](Listener const& l) { return l.id == id; });
    return n > 0;
}

void ScriptEventBus::removeEngineListeners(ScriptEngineState* engine) {
    std::erase_if(listeners_, [engine](Listener const& l) { return l.engine == engine; });
}

bool ScriptEventBus::hasListeners(HookId hook) const {
    for (auto const& l : listeners_) {
        if (l.hook == hook) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// 派发
// ---------------------------------------------------------------------------

template <typename E>
bool ScriptEventBus::dispatchEach(HookId hook, std::shared_ptr<E> const& ev) {
    int const timeoutMs = services_ ? services_->hookTimeoutMs() : 10000;

    for (std::size_t i = 0; i < listeners_.size(); ++i) {
        if (ev->isCancelled()) break;

        Listener const& l = listeners_[i]; // 每轮重新取：handler 可能调用 listen() 改变向量
        if (l.hook != hook) continue;
        auto* state = l.engine;
        if (!state || !state->engine()) continue;
        if (!l.fn || l.fn->global().isEmpty()) continue; // 引擎已销毁

        auto* engine = state->engine();
        try {
            jspp::EngineScope scope(engine);
            // jspp::Exception 须在 EngineScope 存活期内消费，catch 位于 scope 之内
            try {
                auto obj = wrapEvent(ev);
                auto res = l.fn->global().get().call(obj, {obj});
                if (res.isPromise()) {
                    if (hookSupportsAsync(hook)) {
                        waitPromiseSettled(engine, res.asPromise(), timeoutMs, hookName(hook), state->console());
                    } else {
                        state->console()->append(
                            LogLevel::Warn,
                            "handler returned a Promise; this hook only supports synchronous handlers, use "
                            "beforeFetchMeta for async rewrites"
                        );
                    }
                } else if (!res.isNullOrUndefined()) {
                    state->console()->append(
                        LogLevel::Warn,
                        fmt::format("[{}] handler return value ignored (expected undefined or Promise)", hookName(hook))
                    );
                }
            } catch (jspp::Exception const& e) {
                reportScriptError(state, hookName(hook), e.message(), e.stacktrace());
            } catch (std::exception const& e) {
                reportScriptError(state, hookName(hook), e.what(), {});
            }
        } catch (std::exception const& e) {
            reportScriptError(state, hookName(hook), e.what(), {});
        }

        if (ev->isCancelled()) break;
    }

    return ev->isCancelled();
}

void ScriptEventBus::dispatchNotify(HookId hook, std::shared_ptr<TaskNotifyEvent> const& ev) {
    for (std::size_t i = 0; i < listeners_.size(); ++i) {
        Listener const& l = listeners_[i];
        if (l.hook != hook) continue;
        auto* state = l.engine;
        if (!state || !state->engine()) continue;
        if (!l.fn || l.fn->global().isEmpty()) continue;

        auto* engine = state->engine();
        try {
            jspp::EngineScope scope(engine);
            try {
                auto value =
                    jspp::binding::toJs(ev, jspp::binding::ReturnValuePolicy::kAutomatic, jspp::Local<jspp::Value>{});
                auto res = l.fn->global().get().call(value, {value});
                (void)res; // fire-and-forget：返回的 Promise 交由引擎任务队列自然推进
            } catch (jspp::Exception const& e) {
                reportScriptError(state, hookName(hook), e.message(), e.stacktrace());
            } catch (std::exception const& e) {
                reportScriptError(state, hookName(hook), e.what(), {});
            }
        } catch (std::exception const& e) {
            reportScriptError(state, hookName(hook), e.what(), {});
        }
    }
}

bool ScriptEventBus::fireBeforeCreateTask(std::shared_ptr<BeforeCreateTaskEvent> const& ev) {
    return dispatchEach(HookId::BeforeCreateTask, ev);
}

bool ScriptEventBus::fireBeforeFetchMeta(std::shared_ptr<BeforeFetchMetaEvent> const& ev) {
    return dispatchEach(HookId::BeforeFetchMeta, ev);
}

bool ScriptEventBus::fireAfterFetchMeta(std::shared_ptr<AfterFetchMetaEvent> const& ev) {
    return dispatchEach(HookId::AfterFetchMeta, ev);
}

void ScriptEventBus::fireNotify(HookId hook, std::shared_ptr<TaskNotifyEvent> const& ev) { dispatchNotify(hook, ev); }

// 显式实例化（模板定义保持在 .cc 内）
template bool
ScriptEventBus::dispatchEach<BeforeCreateTaskEvent>(HookId, std::shared_ptr<BeforeCreateTaskEvent> const&);
template bool ScriptEventBus::dispatchEach<BeforeFetchMetaEvent>(HookId, std::shared_ptr<BeforeFetchMetaEvent> const&);
template bool ScriptEventBus::dispatchEach<AfterFetchMetaEvent>(HookId, std::shared_ptr<AfterFetchMetaEvent> const&);

} // namespace edm::script
