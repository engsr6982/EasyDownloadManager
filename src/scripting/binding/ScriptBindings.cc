#include "ScriptBindings.h"

#include "../api/ScriptEvents.h"
#include "../api/ScriptFs.h"
#include "../api/ScriptHttp.h"
#include "../engine/ScriptEngineState.h"
#include "../engine/ScriptingBootstrap.h"

#include <fmt/args.h>
#include <fmt/format.h>
#include <magic_enum/magic_enum.hpp>

#include "utils/Utils.h"

#include "jspp/Jspp.h"
#include "jspp/binding/MetaBuilder.h"
#include "jspp/binding/TypeConverter.h"

JSPP_WARNING_GUARD_BEGIN
#include "quickjs.h"
JSPP_WARNING_GUARD_END

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <mutex>
#include <thread>

// ---------------------------------------------------------------------------
// options 对象的整体转换（plain object 与 options 结构体互转，供参数直接 toCpp）
// ---------------------------------------------------------------------------
namespace jspp::binding {

template <>
struct TypeConverter<edm::script::HttpRequestOptions> {
    using T = edm::script::HttpRequestOptions;

    static Local<Value> toJs(T const& options, ReturnValuePolicy /*policy*/, Local<Value> const& /*parent*/) {
        auto obj = Object::newObject();
        obj.set(
            String::newString("headers"),
            TypeConverter<std::unordered_map<std::string, std::string>>::toJs(
                options.headers,
                ReturnValuePolicy::kCopy,
                {}
            )
        );
        obj.set(String::newString("proxy"), String::newString(options.proxy));
        obj.set(String::newString("timeoutMs"), Number::newNumber(options.timeoutMs));
        return obj;
    }

    static T toCpp(Local<Value> const& value) {
        if (!value.isObject()) {
            throw Exception("options must be an object", Exception::Type::TypeError);
        }
        T          options;
        auto const obj = value.asObject();
        if (obj.has(String::newString("headers"))) {
            auto const headers = obj.get(String::newString("headers"));
            if (!headers.isNullOrUndefined()) {
                options.headers = TypeConverter<std::unordered_map<std::string, std::string>>::toCpp(headers);
            }
        }
        if (obj.has(String::newString("proxy"))) {
            auto const proxy = obj.get(String::newString("proxy"));
            if (!proxy.isNullOrUndefined()) options.proxy = TypeConverter<std::string>::toCpp(proxy);
        }
        if (obj.has(String::newString("timeoutMs"))) {
            auto const timeout = obj.get(String::newString("timeoutMs"));
            if (timeout.isNumber()) options.timeoutMs = std::max(1, timeout.asNumber().getInt32());
        }
        return options;
    }
};

template <>
struct TypeConverter<edm::script::NotifyOptions> {
    using T = edm::script::NotifyOptions;

    static Local<Value> toJs(T const& options, ReturnValuePolicy /*policy*/, Local<Value> const& /*parent*/) {
        auto obj = Object::newObject();
        if (options.title) obj.set(String::newString("title"), String::newString(*options.title));
        if (options.level) obj.set(String::newString("level"), Number::newNumber(static_cast<int>(*options.level)));
        return obj;
    }

    static T toCpp(Local<Value> const& value) {
        if (!value.isObject()) {
            throw Exception("options must be an object", Exception::Type::TypeError);
        }
        T          options;
        auto const obj = value.asObject();
        if (obj.has(String::newString("title"))) {
            auto const title = obj.get(String::newString("title"));
            if (title.isString()) options.title = title.asString().getValue();
        }
        if (obj.has(String::newString("level"))) {
            auto const level = obj.get(String::newString("level"));
            if (level.isNumber()) options.level = static_cast<edm::script::NotifyLevel>(level.asNumber().getInt32());
        }
        return options;
    }
};

} // namespace jspp::binding

namespace edm::script::binding {

namespace {

// ---------------------------------------------------------------------------
// 上下文与公共辅助
// ---------------------------------------------------------------------------

EngineOwnData& currentContext() {
    auto& engine = jspp::EngineScope::currentEngineChecked();
    auto  data   = engine.getData<EngineOwnData>();
    if (!data) {
        throw jspp::Exception("engine is missing its scripting context", jspp::Exception::Type::ReferenceError);
    }
    return *data;
}

/// magic_enum 自动生成枚举绑定
template <typename E>
jspp::EnumMeta bindEnum(std::string const& name) {
    auto builder = jspp::binding::defEnum<E>(name);
    for (auto value : magic_enum::enum_values<E>()) {
        builder.value(std::string{magic_enum::enum_name(value)}, value);
    }
    return builder.build();
}

/// 相对路径解析：以脚本目录为根
std::string resolvePath(ScriptEngineState* state, std::string const& path) {
    if (path.empty() || std::filesystem::path{path}.is_absolute()) return path;
    return (std::filesystem::path{state->dir()} / path).string();
}

std::string stringifyValue(jspp::Local<jspp::Value> const& value) {
    if (value.isNullOrUndefined()) return value.isNull() ? "null" : "undefined";
    if (value.isString()) return value.asString().getValue();
    if (value.isBoolean()) return value.asBoolean().getValue() ? "true" : "false";
    if (value.isNumber()) {
        double const d = value.asNumber().getDouble();
        if (std::isfinite(d) && d == std::floor(d) && std::abs(d) < 1e15) {
            return fmt::format("{}", static_cast<long long>(d));
        }
        return fmt::format("{}", d);
    }
    if (value.isBigInt()) return fmt::format("{}", value.asBigInt().getInt64());

    // 对象/数组：一次原生 JSON.stringify 调用（失败回退占位）
    try {
        auto*   ctx = jspp::EngineScope::currentEngineChecked().context();
        JSValue str = JS_JSONStringify(ctx, jspp::ValueHelper::unwrap(value), JS_UNDEFINED, JS_UNDEFINED);
        if (JS_IsException(str)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
            return "[object]";
        }
        std::size_t       len  = 0;
        char const*       text = JS_ToCStringLen(ctx, &len, str);
        std::string const out(text == nullptr ? std::string{} : std::string{text, len});
        JS_FreeCString(ctx, text);
        JS_FreeValue(ctx, str);
        return out;
    } catch (...) {
        return "[object]";
    }
}

/// "{}" 占位符格式化：参数按类型推入 fmt 动态参数存储
template <std::size_t fmtStartOffset = 0>
inline std::string formatLogArgs(jspp::Arguments const& args) {
    auto const argc = args.length();
    if (argc < fmtStartOffset + 1) {
        throw jspp::Exception{"Formatting requires a string to be formatted", jspp::Exception::Type::TypeError};
    }
    auto const& maybe = args[fmtStartOffset];
    if (!maybe.isString()) {
        throw jspp::Exception{"The string to be formatted must be a string", jspp::Exception::Type::TypeError};
    }

    fmt::dynamic_format_arg_store<fmt::format_context> store;
    for (std::size_t i = fmtStartOffset + 1; i < argc; ++i) {
        auto const& v = args[i];
        if (v.isString()) {
            store.push_back(v.asString().getValue());
        } else if (v.isNumber()) {
            double const d = v.asNumber().getDouble();
            if (d == std::floor(d) && std::abs(d) < 1e15) {
                store.push_back(static_cast<std::int64_t>(d));
            } else {
                store.push_back(d);
            }
        } else if (v.isBoolean()) {
            store.push_back(v.asBoolean().getValue());
        } else if (v.isBigInt()) {
            store.push_back(v.asBigInt().getInt64());
        } else {
            store.push_back(stringifyValue(v));
        }
    }
    return fmt::vformat(maybe.asString().getValue(), store);
}


// ---------------------------------------------------------------------------
// http 胶水：Promise + 工作线程回调经托管任务队列回注主线程泵
// ---------------------------------------------------------------------------

struct HttpCall : IPendingScriptCall {
    jspp::PromiseResolver resolver;
    std::int64_t          id{0};

    void dispose() override { resolver.reset(); }
};

jspp::Local<jspp::Value> implHttpRequest(jspp::Arguments const& args, char const* method) {
    if (args.length() < 1 || !args[0].isString()) {
        throw jspp::Exception(
            fmt::format("http.{}(url, options?) requires a url string", method),
            jspp::Exception::Type::TypeError
        );
    }

    auto& data  = currentContext();
    auto* state = data.state;
    // shared_ptr 保活：工作线程回调时引擎可能已销毁，postTask 内部按 disposed 丢弃
    auto const dataPtr  = state->engine()->getData<EngineOwnData>();
    auto       resolver = jspp::PromiseResolver::newResolver();
    auto       promise  = resolver.promise();

    auto call      = std::make_shared<HttpCall>();
    call->id       = data.registerPendingCall(call);
    call->resolver = std::move(resolver);

    auto const url       = args[0].asString().getValue();
    auto const options   = args.length() > 1 ? jspp::binding::toCpp<HttpRequestOptions>(args[1]) : HttpRequestOptions{};
    auto const userAgent = state->services()->userAgent();

    ScriptHttp::request(method, url, options, userAgent, [dataPtr, state, call](HttpResult result) {
        // 工作线程：托管队列投递（dataPtr 保活；引擎已销毁则丢弃）
        dataPtr->postTask([dataPtr, state, call, result = std::move(result)] {
            // 主线程（泵方已进入 EngineScope）
            try {
                auto pending = dataPtr->takePendingCall(call->id);
                if (!pending || pending != call) return;
                if (!result.ok) {
                    call->resolver.reject(jspp::String::newString(result.error));
                } else {
                    call->resolver.resolve(
                        jspp::binding::toJs(
                            std::make_shared<HttpResult>(std::move(result)),
                            jspp::binding::ReturnValuePolicy::kMove,
                            jspp::Local<jspp::Value>{}
                        )
                    );
                }
            } catch (jspp::Exception const& e) {
                state->console()->append(LogLevel::Error, fmt::format("[http] {}\n{}", e.what(), e.stacktrace()));
            } catch (std::exception const& e) {
                state->console()->append(LogLevel::Error, fmt::format("[http] {}", e.what()));
            }
        });
    });
    return promise;
}

// ---------------------------------------------------------------------------
// 定时器 / 日志 / 监听 / 通知胶水
// ---------------------------------------------------------------------------

void invokeProtected(ScriptEngineState* state, std::function<void()> const& invoke) {
    try {
        jspp::EngineScope scope(state->engine());
        try {
            invoke();
        } catch (jspp::Exception const& e) {
            state->console()->append(LogLevel::Error, fmt::format("[timer] {}\n{}", e.what(), e.stacktrace()));
        }
    } catch (std::exception const& e) {
        state->console()->append(LogLevel::Error, fmt::format("[timer] {}", e.what()));
    }
}

jspp::Local<jspp::Value> implTimer(jspp::Arguments const& args, bool repeat) {
    if (args.length() < 1 || !args[0].isFunction()) {
        throw jspp::Exception(
            "setTimeout(callback, ms?) / setInterval(callback, ms?) require a function as the first argument",
            jspp::Exception::Type::TypeError
        );
    }
    int ms = 0;
    if (args.length() > 1 && args[1].isNumber()) {
        ms = std::max(0, args[1].asNumber().getInt32());
    }
    auto const intervalMs = repeat ? std::max(1, ms) : ms; // setInterval 至少 1ms，避免空转

    auto&      data    = currentContext();
    auto*      state   = data.state;
    auto const tracked = jspp::TrackedGlobal<jspp::Function>::create(args[0].asFunction());

    auto const id = data.addTimer(
        [state, tracked] {
            invokeProtected(state, [tracked] {
                if (tracked->global().isEmpty()) return; // 引擎已销毁（TrackedGlobal 自动置空）
                (void)tracked->global().get().call({}, {});
            });
        },
        intervalMs,
        !repeat
    );
    return jspp::Number::newNumber(static_cast<int32_t>(id));
}

jspp::Local<jspp::Value> implClearTimer(jspp::Arguments const& args) {
    if (args.length() < 1 || !args[0].isNumber()) {
        throw jspp::Exception(
            "clearTimeout(id) / clearInterval(id) require a timer id",
            jspp::Exception::Type::TypeError
        );
    }
    return jspp::Boolean::newBoolean(currentContext().removeTimer(args[0].asNumber().getInt32()));
}

jspp::Local<jspp::Value> implListen(jspp::Arguments const& args) {
    if (args.length() < 2 || !args[0].isString() || !args[1].isFunction()) {
        throw jspp::Exception(
            "events.listen(event, handler, priority?) requires (string, function[, priority])",
            jspp::Exception::Type::TypeError
        );
    }
    auto const name = args[0].asString().getValue();
    auto const hook = hookFromName(name);
    if (!hook) {
        throw jspp::Exception(
            fmt::format("events.listen: unknown event \"{}\"", name),
            jspp::Exception::Type::TypeError
        );
    }

    auto priority = ListenerPriority::Normal;
    if (args.length() >= 3) {
        if (!args[2].isNumber() && !args[2].isBigInt()) {
            throw jspp::Exception("events.listen: priority must be a Priority value", jspp::Exception::Type::TypeError);
        }
        int const p =
            args[2].isNumber() ? args[2].asNumber().getInt32() : static_cast<int>(args[2].asBigInt().getInt64());
        if (p < static_cast<int>(ListenerPriority::Highest) || p > static_cast<int>(ListenerPriority::Lowest)) {
            throw jspp::Exception("events.listen: priority out of range", jspp::Exception::Type::RangeError);
        }
        priority = static_cast<ListenerPriority>(p);
    }

    auto& data = currentContext();
    return jspp::Number::newNumber(
        data.bootstrap->eventBus()->addListener(*hook, args[1].asFunction(), priority, data.state)
    );
}

jspp::Local<jspp::Value> implUnlisten(jspp::Arguments const& args) {
    if (args.length() < 1 || (!args[0].isNumber() && !args[0].isBigInt())) {
        throw jspp::Exception("events.unlisten(id) requires a listener id", jspp::Exception::Type::TypeError);
    }
    uint32_t const id = args[0].isNumber() ? static_cast<uint32_t>(args[0].asNumber().getInt32())
                                           : static_cast<uint32_t>(args[0].asBigInt().getUint64());
    return jspp::Boolean::newBoolean(currentContext().bootstrap->eventBus()->removeListener(id));
}

jspp::Local<jspp::Value> implNotify(jspp::Arguments const& args) {
    if (args.length() < 1 || !args[0].isString()) {
        throw jspp::Exception("notify(message, options?) requires a message string", jspp::Exception::Type::TypeError);
    }
    auto& data     = currentContext();
    auto* services = data.state->services();

    auto const  message = args[0].asString().getValue();
    std::string title   = data.state->name();
    auto        level   = NotifyLevel::Info;

    if (args.length() > 1) {
        auto const opts = jspp::binding::toCpp<NotifyOptions>(args[1]);
        if (opts.title) title = *opts.title;
        if (opts.level) {
            if (*opts.level < NotifyLevel::Info || *opts.level > NotifyLevel::Error) {
                throw jspp::Exception("notify: level out of range", jspp::Exception::Type::RangeError);
            }
            level = *opts.level;
        }
    }

    services->notify(level, title, message);
    return jspp::Undefined::newUndefined();
}

jspp::Local<jspp::Value> implTaskControl(jspp::Arguments const& args, TaskAction action) {
    if (args.length() < 1 || (!args[0].isNumber() && !args[0].isBigInt())) {
        throw jspp::Exception(
            fmt::format("tasks.{}(id) requires a task id", magic_enum::enum_name(action)),
            jspp::Exception::Type::TypeError
        );
    }
    int const id = args[0].isNumber() ? args[0].asNumber().getInt32() : static_cast<int>(args[0].asBigInt().getInt64());
    auto*     services = currentContext().state->services();
    return jspp::Boolean::newBoolean(services->taskControl(id, action));
}

/// 事件类包装：禁脚本构造；类型具备 cancel 能力（Cancellable 注入）时绑定命令式方法与取消状态
template <typename E>
auto defEventClass(std::string const& name) {
    auto builder = jspp::binding::defClass<E>(name).ctor(nullptr);
    if constexpr (requires(E& ev, std::string const& r) {
                      ev.cancel(r);
                      ev.setCancelled(true);
                      { ev.isCancelled() } -> std::same_as<bool>;
                  }) {
        builder
            .method(
                "cancelReason",
                [](jspp::InstancePayload& p, jspp::Arguments const&) -> jspp::Local<jspp::Value> {
                    return jspp::String::newString(p.unwrap<E>()->cancelReason());
                }
            )
            .method(
                "cancel",
                [](jspp::InstancePayload& p, jspp::Arguments const& args) -> jspp::Local<jspp::Value> {
                    auto* ev = p.unwrap<E>();
                    if (args.length() > 0 && !args[0].isNullOrUndefined()) {
                        ev->cancel(args[0].isString() ? args[0].asString().getValue() : stringifyValue(args[0]));
                    } else {
                        ev->cancel();
                    }
                    return jspp::Undefined::newUndefined();
                }
            )
            .method(
                "setCancelled",
                [](jspp::InstancePayload& p, jspp::Arguments const& args) -> jspp::Local<jspp::Value> {
                    p.unwrap<E>()->setCancelled(args.length() > 0 && args[0].asBoolean().getValue());
                    return jspp::Undefined::newUndefined();
                }
            )
            .method("isCancelled", [](jspp::InstancePayload& p, jspp::Arguments const&) -> jspp::Local<jspp::Value> {
                return jspp::Boolean::newBoolean(p.unwrap<E>()->isCancelled());
            });
    }
    return std::move(builder);
}

} // namespace

jspp::ModuleMeta const& edmModule() {
    // 元对象为函数级 static：首次调用（引擎注册期，main 之后）构造，进程内持久

    // ---- 枚举 ----
    static auto const kPriorityEnum    = bindEnum<ListenerPriority>("Priority");
    static auto const kTaskStateEnum   = bindEnum<TaskState>("TaskState");
    static auto const kNotifyLevelEnum = bindEnum<NotifyLevel>("NotifyLevel");

    // ---- 可变事件 ----
    static auto const kBeforeCreateTaskMeta = defEventClass<BeforeCreateTaskEvent>("BeforeCreateTaskEvent")
                                                  .prop_readonly("taskId", &BeforeCreateTaskEvent::taskId)
                                                  .prop("url", &BeforeCreateTaskEvent::url)
                                                  .prop("fileName", &BeforeCreateTaskEvent::fileName)
                                                  .prop("saveDir", &BeforeCreateTaskEvent::saveDir)
                                                  .prop("bandLimit", &BeforeCreateTaskEvent::bandLimit)
                                                  .prop("threadCount", &BeforeCreateTaskEvent::threadCount)
                                                  .prop("userAgent", &BeforeCreateTaskEvent::userAgent)
                                                  .prop("origin", &BeforeCreateTaskEvent::origin)
                                                  .prop("referer", &BeforeCreateTaskEvent::referer)
                                                  .prop("cookie", &BeforeCreateTaskEvent::cookie)
                                                  .prop("headers", &BeforeCreateTaskEvent::headers)
                                                  .prop("useProxy", &BeforeCreateTaskEvent::useProxy)
                                                  .prop("proxyUrl", &BeforeCreateTaskEvent::proxyUrl)
                                                  .prop("overwriteExisting", &BeforeCreateTaskEvent::overwriteExisting)
                                                  .build();

    static auto const kBeforeFetchMetaMeta = defEventClass<BeforeFetchMetaEvent>("BeforeFetchMetaEvent")
                                                 .prop_readonly("taskId", &BeforeFetchMetaEvent::taskId)
                                                 .prop("url", &BeforeFetchMetaEvent::url)
                                                 .prop("userAgent", &BeforeFetchMetaEvent::userAgent)
                                                 .prop("origin", &BeforeFetchMetaEvent::origin)
                                                 .prop("referer", &BeforeFetchMetaEvent::referer)
                                                 .prop("cookie", &BeforeFetchMetaEvent::cookie)
                                                 .prop("headers", &BeforeFetchMetaEvent::headers)
                                                 .prop("useProxy", &BeforeFetchMetaEvent::useProxy)
                                                 .prop("proxyUrl", &BeforeFetchMetaEvent::proxyUrl)
                                                 .build();

    static auto const kAfterFetchMetaMeta =
        defEventClass<AfterFetchMetaEvent>("AfterFetchMetaEvent")
            .prop_readonly("taskId", &AfterFetchMetaEvent::taskId)
            .prop("url", &AfterFetchMetaEvent::url)
            .prop("userAgent", &AfterFetchMetaEvent::userAgent)
            .prop("origin", &AfterFetchMetaEvent::origin)
            .prop("referer", &AfterFetchMetaEvent::referer)
            .prop("cookie", &AfterFetchMetaEvent::cookie)
            .prop("headers", &AfterFetchMetaEvent::headers)
            .prop("fileName", &AfterFetchMetaEvent::fileName)
            .prop_readonly("finalUrl", &AfterFetchMetaEvent::finalUrl)
            .prop_readonly("supportRange", &AfterFetchMetaEvent::supportRange)
            .prop_readonly("fileSize", &AfterFetchMetaEvent::fileSize)
            .prop_readonly("contentType", &AfterFetchMetaEvent::contentType)
            .prop_readonly("etag", &AfterFetchMetaEvent::etag)
            .prop_readonly("lastModified", &AfterFetchMetaEvent::lastModified)
            .prop_readonly("contentDisposition", &AfterFetchMetaEvent::contentDisposition)
            .prop_readonly("md5", &AfterFetchMetaEvent::md5)
            .build();

    static auto const kNotifyEventMeta = jspp::binding::defClass<TaskNotifyEvent>("TaskEvent")
                                             .ctor(nullptr)
                                             .prop_readonly("taskId", &TaskNotifyEvent::taskId)
                                             .prop_readonly("url", &TaskNotifyEvent::url)
                                             .prop_readonly("fileName", &TaskNotifyEvent::fileName)
                                             .prop_readonly("error", &TaskNotifyEvent::errorMsg)
                                             .prop_readonly("downloaded", &TaskNotifyEvent::downloaded)
                                             .prop_readonly("total", &TaskNotifyEvent::total)
                                             .prop_readonly("speed", &TaskNotifyEvent::speed)
                                             .prop_readonly("percent", &TaskNotifyEvent::percent)
                                             .build();

    static auto const kHttpResponseMeta = jspp::binding::defClass<HttpResult>("HttpResponse")
                                              .ctor(nullptr)
                                              .prop_readonly("status", &HttpResult::status)
                                              .prop_readonly("ok", &HttpResult::ok)
                                              .prop_readonly("finalUrl", &HttpResult::finalUrl)
                                              .prop_readonly("body", &HttpResult::body)
                                              .prop_readonly("headers", &HttpResult::headers)
                                              .prop_readonly("error", &HttpResult::error)
                                              .build();

    static auto const kTaskInfoMeta = jspp::binding::defClass<TaskSnapshot>("TaskInfo")
                                          .ctor(nullptr)
                                          .prop_readonly("id", &TaskSnapshot::id)
                                          .prop_readonly("url", &TaskSnapshot::url)
                                          .prop_readonly("fileName", &TaskSnapshot::fileName)
                                          .prop_readonly("saveDir", &TaskSnapshot::saveDir)
                                          .prop_readonly("state", &TaskSnapshot::state)
                                          .prop_readonly("fileSize", &TaskSnapshot::fileSize)
                                          .prop_readonly("downloaded", &TaskSnapshot::downloaded)
                                          .prop_readonly("speed", &TaskSnapshot::speed)
                                          .prop_readonly("threadCount", &TaskSnapshot::threadCount)
                                          .build();

    // ---- 静态类 ----
    static auto const kEventsMeta =
        jspp::binding::defClass<void>("events")
            .func("listen", [](jspp::Arguments const& args) { return implListen(args); })
            .func("unlisten", [](jspp::Arguments const& args) { return implUnlisten(args); })
            .build();

    static auto const kLoggerMeta =
        jspp::binding::defClass<void>("logger")
            .func(
                "info",
                [](jspp::Arguments const& args) -> jspp::Local<jspp::Value> {
                    currentContext().state->console()->append(LogLevel::Info, formatLogArgs(args));
                    return {};
                }
            )
            .func(
                "warn",
                [](jspp::Arguments const& args) -> jspp::Local<jspp::Value> {
                    currentContext().state->console()->append(LogLevel::Warn, formatLogArgs(args));
                    return {};
                }
            )
            .func(
                "error",
                [](jspp::Arguments const& args) -> jspp::Local<jspp::Value> {
                    currentContext().state->console()->append(LogLevel::Error, formatLogArgs(args));
                    return {};
                }
            )
            .func(
                "debug",
                [](jspp::Arguments const& args) -> jspp::Local<jspp::Value> {
                    currentContext().state->console()->append(LogLevel::Debug, formatLogArgs(args));
                    return {};
                }
            )
            .build();

    static auto const kHttpMeta =
        jspp::binding::defClass<void>("http")
            .func("get", [](jspp::Arguments const& args) { return implHttpRequest(args, "GET"); })
            .func("head", [](jspp::Arguments const& args) { return implHttpRequest(args, "HEAD"); })
            .build();

    static auto const kTasksMeta =
        jspp::binding::defClass<void>("tasks")
            .func(
                "get",
                [](jspp::Arguments const& args) -> jspp::Local<jspp::Value> {
                    if (args.length() < 1 || (!args[0].isNumber() && !args[0].isBigInt())) {
                        throw jspp::Exception("tasks.get(id) requires a task id", jspp::Exception::Type::TypeError);
                    }
                    int const  id       = args[0].isNumber() ? args[0].asNumber().getInt32()
                                                             : static_cast<int>(args[0].asBigInt().getInt64());
                    auto*      services = currentContext().state->services();
                    auto const snap     = services->taskGet(id);
                    if (!snap) return jspp::Null::newNull();
                    return jspp::binding::toJs(
                        std::make_shared<TaskSnapshot>(*snap),
                        jspp::binding::ReturnValuePolicy::kCopy,
                        jspp::Local<jspp::Value>{}
                    );
                }
            )
            .func(
                "getAll",
                [](jspp::Arguments const&) -> jspp::Local<jspp::Value> {
                    auto*                                      services = currentContext().state->services();
                    std::vector<std::shared_ptr<TaskSnapshot>> all;
                    for (auto& snap : services->taskGetAll()) {
                        all.push_back(std::make_shared<TaskSnapshot>(std::move(snap)));
                    }
                    return jspp::binding::toJs(
                        all,
                        jspp::binding::ReturnValuePolicy::kAutomatic,
                        jspp::Local<jspp::Value>{}
                    );
                }
            )
            .func("pause", [](jspp::Arguments const& args) { return implTaskControl(args, TaskAction::Pause); })
            .func("resume", [](jspp::Arguments const& args) { return implTaskControl(args, TaskAction::Resume); })
            .func("cancel", [](jspp::Arguments const& args) { return implTaskControl(args, TaskAction::Cancel); })
            .build();

    static auto const kFsMeta =
        jspp::binding::defClass<void>("fs")
            .func(
                "exists",
                [](std::string const& path) { return ScriptFs::exists(resolvePath(currentContext().state, path)); }
            )
            .func(
                "readFile",
                [](std::string const& path) { return ScriptFs::readFile(resolvePath(currentContext().state, path)); }
            )
            .func(
                "writeFile",
                [](std::string const& path, std::string const& content) {
                    return ScriptFs::writeFile(resolvePath(currentContext().state, path), content);
                }
            )
            .func(
                "copy",
                [](std::string const& src, std::string const& dst) {
                    return ScriptFs::copy(
                        resolvePath(currentContext().state, src),
                        resolvePath(currentContext().state, dst)
                    );
                }
            )
            .func(
                "move",
                [](std::string const& src, std::string const& dst) {
                    return ScriptFs::move(
                        resolvePath(currentContext().state, src),
                        resolvePath(currentContext().state, dst)
                    );
                }
            )
            .func(
                "mkdir",
                [](std::string const& path, std::optional<bool> const& recursive) {
                    return ScriptFs::mkdir(resolvePath(currentContext().state, path), recursive.value_or(true));
                }
            )
            .func(
                "appendFile",
                [](std::string const& path, std::string const& content) {
                    return ScriptFs::appendFile(resolvePath(currentContext().state, path), content);
                },
                [](std::string const& path, std::string const& content, bool createIfNotExists) {
                    return ScriptFs::appendFile(resolvePath(currentContext().state, path), content, createIfNotExists);
                }
            )
            .func(
                "remove",
                [](std::string const& path) { return ScriptFs::remove(resolvePath(currentContext().state, path)); }
            )
            .func(
                "isDirectory",
                [](std::string const& path) { return ScriptFs::isDirectory(resolvePath(currentContext().state, path)); }
            )
            .func(
                "isFile",
                [](std::string const& path) { return ScriptFs::isFile(resolvePath(currentContext().state, path)); }
            )
            .build();

    static auto const kPathMeta = jspp::binding::defClass<void>("path")
                                      .func(
                                          "join",
                                          [](jspp::Arguments const& args) -> jspp::Local<jspp::Value> {
                                              std::vector<std::string> parts;
                                              parts.reserve(args.length());
                                              for (std::size_t i = 0; i < args.length(); ++i) {
                                                  if (!args[i].isString()) {
                                                      throw jspp::Exception(
                                                          "path.join(...parts) requires string parts",
                                                          jspp::Exception::Type::TypeError
                                                      );
                                                  }
                                                  parts.push_back(args[i].asString().getValue());
                                              }
                                              return jspp::String::newString(ScriptPath::join(parts));
                                          }
                                      )
                                      .func("basename", &ScriptPath::basename)
                                      .func("dirname", &ScriptPath::dirname)
                                      .func("extname", &ScriptPath::extname)
                                      .func("normalize", &ScriptPath::normalize)
                                      .func(
                                          "relative",
                                          [](std::string const& from, std::string const& to) {
                                              auto& data = currentContext();
                                              return ScriptPath::relative(data.state->dir(), from, to);
                                          }
                                      )
                                      .func(
                                          "absolute",
                                          [](std::string const& path) {
                                              auto& data = currentContext();
                                              return ScriptPath::absolute(data.state->dir(), path);
                                          }
                                      )
                                      .build();

    static auto const kUtilsMeta =
        jspp::binding::defClass<void>("utils")
            .func(
                "formatBytes",
                [](double bytes) -> std::string {
                    if (bytes <= 0) return "0 B";
                    return utils::FileSize2String(static_cast<FileSize>(bytes));
                }
            )
            .func(
                "formatSpeed",
                [](double bytesPerSecond) -> std::string {
                    if (bytesPerSecond <= 0.0) return "N/A";
                    return utils::FileSize2String(static_cast<FileSize>(bytesPerSecond)) + "/s";
                }
            )
            .build();

    static auto const kConstantsMeta =
        jspp::binding::defClass<void>("constants")
            .var_readonly(
                "invalidTaskId",
                [] -> jspp::Local<jspp::Value> { return jspp::binding::toJs(kInvalidTaskID); }
            )
            .var_readonly(
                "unknownFileName",
                [] -> jspp::Local<jspp::Value> { return jspp::String::newString(kInvalidFileName); }
            )
            .var_readonly(
                "unknownFileSize",
                [] -> jspp::Local<jspp::Value> { return jspp::binding::toJs(kInvalidFileSize); }
            )
            .var_readonly("unknownPercent", [] -> jspp::Local<jspp::Value> { return jspp::Number::newNumber(-1.0); })
            .build();

    static auto const module =
        jspp::binding::defModule("edm")
            .export_class(kEventsMeta)
            .export_class(kLoggerMeta)
            .export_function(
                "setTimeout",
                [](jspp::Arguments const& args) { return implTimer(args, /*repeat=*/false); }
            )
            .export_function("clearTimeout", [](jspp::Arguments const& args) { return implClearTimer(args); })
            .export_function(
                "setInterval",
                [](jspp::Arguments const& args) { return implTimer(args, /*repeat=*/true); }
            )
            .export_function("clearInterval", [](jspp::Arguments const& args) { return implClearTimer(args); })
            .export_class(kHttpMeta)
            .export_class(kTasksMeta)
            .export_class(kFsMeta)
            .export_class(kPathMeta)
            .export_class(kUtilsMeta)
            .export_function("notify", [](jspp::Arguments const& args) { return implNotify(args); })
            .export_enum(kPriorityEnum)
            .export_enum(kTaskStateEnum)
            .export_enum(kNotifyLevelEnum)
            .export_class(kBeforeCreateTaskMeta)
            .export_class(kBeforeFetchMetaMeta)
            .export_class(kAfterFetchMetaMeta)
            .export_class(kNotifyEventMeta)
            .export_class(kHttpResponseMeta)
            .export_class(kTaskInfoMeta)
            .export_class(kConstantsMeta)
            .build();
    return module;
}

} // namespace edm::script::binding
