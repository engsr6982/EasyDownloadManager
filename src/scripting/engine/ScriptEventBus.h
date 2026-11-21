#pragma once

#include "../api/ScriptConsole.h"
#include "../api/ScriptEvents.h"
#include "../api/ScriptHostServices.h"

#include "jspp/core/Reference.h"
#include "jspp/core/TrackedHandle.h"
#include "jspp/core/Value.h"


namespace edm::script {

class ScriptEngineState;

/// 脚本事件总线（仅主线程访问）。
/// 监听器经 edm.listen() 注册；可变钩子逐监听器同步执行并等待 Promise settle
/// （受 hookTimeoutMs 约束）；通知钩子 fire-and-forget。单监听器异常写入其控制台。
class ScriptEventBus {
public:
    explicit ScriptEventBus(ScriptHostServices* services) : services_(services) {}
    ~ScriptEventBus() = default;

    ScriptEventBus(ScriptEventBus const&)            = delete;
    ScriptEventBus& operator=(ScriptEventBus const&) = delete;

    /// 注册监听器（脚本侧 edm.listen）。@return 监听器 id
    uint32_t addListener(
        HookId                             hook,
        jspp::Local<jspp::Function> const& fn,
        ListenerPriority                   priority,
        ScriptEngineState*                 engine
    );
    bool removeListener(uint32_t id);
    void removeEngineListeners(ScriptEngineState* engine);
    bool hasListeners(HookId hook) const;

    // ---- 派发（全部要求在主线程调用）----

    bool fireBeforeCreateTask(std::shared_ptr<BeforeCreateTaskEvent> const& ev);
    bool fireBeforeFetchMeta(std::shared_ptr<BeforeFetchMetaEvent> const& ev);
    bool fireAfterFetchMeta(std::shared_ptr<AfterFetchMetaEvent> const& ev);
    void fireNotify(HookId hook, std::shared_ptr<TaskNotifyEvent> const& ev);

private:
    struct Listener {
        uint32_t           id{0};
        HookId             hook{HookId::BeforeCreateTask};
        ListenerPriority   priority{ListenerPriority::Normal};
        uint64_t           seq{0};
        ScriptEngineState* engine{nullptr};
        /// 脚本回调；TrackedGlobal 观察引擎失效（销毁时自动置空）
        std::shared_ptr<jspp::TrackedGlobal<jspp::Function>> fn;
    };

    void insertListener(Listener&& listener);
    /// 事件包装经 jspp 懒绑定解析（模块导出的类元）
    template <typename E>
    bool dispatchEach(HookId hook, std::shared_ptr<E> const& ev);
    void dispatchNotify(HookId hook, std::shared_ptr<TaskNotifyEvent> const& ev);

    std::vector<Listener> listeners_;
    uint32_t              nextListenerId_{1};
    uint64_t              nextSeq_{0};
    ScriptHostServices*   services_{nullptr};
};

} // namespace edm::script
