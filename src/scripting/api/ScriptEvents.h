#pragma once

#include "Global.h"

#include <optional>
#include <string>
#include <vector>

namespace edm::script {

/// 生命周期钩子（按任务底层阶段划分）
enum class HookId : int {
    BeforeCreateTask = 0, ///< TaskModel 组装后、落库前（主线程同步）
    BeforeFetchMeta,      ///< 元信息探测前（可改写 URL/请求头；支持异步）
    AfterFetchMeta,       ///< 元信息+文件名确定后、磁盘预分配前（可改写文件名；支持异步）
    TaskStarted,
    TaskProgress,
    TaskPaused,
    TaskResumed,
    TaskCompleted,
    TaskFailed,
    TaskCanceled,
    TaskRemoved,

    Count,
};

char const*           hookName(HookId id);
std::optional<HookId> hookFromName(std::string_view name);

/// 可变钩子（事件字段可被脚本改写）
bool isMutableHook(HookId id);
/// 支持返回 Promise 的异步 handler
bool hookSupportsAsync(HookId id);

/// 监听优先级：数值小的先派发；同优先级按注册顺序
enum class ListenerPriority : int { Highest = 0, High = 1, Normal = 2, Low = 3, Lowest = 4 };

// ---------------------------------------------------------------------------
// 事件数据：经 jspp 绑定类以 shared_ptr 交给脚本，字段赋值直接落在结构上，
// 派发结束后宿主读取结果。
// ---------------------------------------------------------------------------

/// 事件基类：仅持有取消状态（private，操作由 Cancellable 经 friend 注入）
struct ScriptEvent {
    virtual ~ScriptEvent() = default;

private:
    bool        cancelled{false};
    std::string cancelReason_;

    template <typename Derived>
    friend struct Cancellable;
};

/// 可取消事件的 CRTP 注入
template <typename Derived>
struct Cancellable : ScriptEvent {
    void cancel() { cancelled = true; }
    void cancel(std::string const& reason) {
        cancelled     = true;
        cancelReason_ = reason;
    }
    void                             setCancelled(bool v) { cancelled = v; }
    [[nodiscard]] bool               isCancelled() const { return cancelled; }
    [[nodiscard]] std::string const& cancelReason() const { return cancelReason_; }
};

/// beforeCreateTask 事件
struct BeforeCreateTaskEvent : Cancellable<BeforeCreateTaskEvent> {
    TaskId                     taskId{kInvalidTaskID}; // 创建时尚未分配，恒为 kInvalidTaskID
    std::string                url;
    std::string                fileName;
    std::string                saveDir;
    BandLimit                  bandLimit{kDefaultBandLimit}; // KB/s，0 = 不限
    int                        threadCount{8};
    std::string                userAgent;
    std::optional<std::string> origin;
    std::optional<std::string> referer;
    std::optional<std::string> cookie;
    std::vector<std::string>   headers; // "Key: Value" 形式
    bool                       useProxy{false};
    std::string                proxyUrl; // 会话级，不持久化
    bool                       overwriteExisting{false};
};

/// beforeFetchMeta 事件
struct BeforeFetchMetaEvent : Cancellable<BeforeFetchMetaEvent> {
    TaskId                     taskId{kInvalidTaskID};
    std::string                url;
    std::string                userAgent;
    std::optional<std::string> origin;
    std::optional<std::string> referer;
    std::optional<std::string> cookie;
    std::vector<std::string>   headers;
    bool                       useProxy{false};
    std::string                proxyUrl;
};

/// afterFetchMeta 事件
struct AfterFetchMetaEvent : Cancellable<AfterFetchMetaEvent> {
    TaskId                     taskId{kInvalidTaskID};
    std::string                url;
    std::string                userAgent;
    std::optional<std::string> origin;
    std::optional<std::string> referer;
    std::optional<std::string> cookie;
    std::vector<std::string>   headers;
    std::string                fileName; // 可改写（重命名点）

    // ---- 服务端元信息（只读）----
    std::string                finalUrl;
    bool                       supportRange{false};
    FileSize                   fileSize{kInvalidFileSize};
    std::optional<std::string> contentType;
    std::optional<std::string> etag;
    std::optional<std::string> lastModified;
    std::optional<std::string> contentDisposition;
    std::optional<std::string> md5;
};

/// 通知类事件的统一载荷（按钩子类型取用相关字段）
struct TaskNotifyEvent {
    TaskId      taskId{kInvalidTaskID};
    std::string url;
    std::string fileName;
    std::string errorMsg;

    double downloaded{0};
    double total{0};
    double speed{0};
    double percent{-1};
};

} // namespace edm::script
