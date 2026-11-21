#pragma once
#include "Global.h"

#include <memory>
#include <string>

namespace edm {

struct TaskContext;
struct FetchedMetaInfo;

/// 脚本钩子执行结果：cancelled = 请求被脚本取消（cancel()），reason 为取消原因。
struct HookResult {
    bool        cancelled{false};
    std::string reason;
};

struct ITaskObserver {
    virtual ~ITaskObserver() = default;

    virtual void onStateChanged(std::shared_ptr<TaskContext>& ctx, TaskState oldState) = 0;

    virtual void onError(std::shared_ptr<TaskContext>& ctx, std::string const& error) = 0;

    virtual void onFileNameChanged(std::shared_ptr<TaskContext>& ctx) = 0;

    // ---- 脚本扩展系统钩子 ----
    // 由 DownloadTask::prepareTask() 在元信息探测前后调用（引擎后台线程）。
    // 宿主实现负责把请求投递到脚本线程并阻塞等待结果；ctx 的可变字段（url、
    // headers 等）允许被脚本改写。返回 cancelled=true 表示请求被脚本取消。

    /// 元信息探测前。脚本可改写 url / userAgent / headers / origin / referer / cookie / proxy。
    virtual HookResult onBeforeFetchMeta(std::shared_ptr<TaskContext>& ctx) { return {}; }

    /// 元信息探测且文件名解析完成后、磁盘预分配前。脚本可改写 fileName 或取消任务。
    virtual HookResult onAfterFetchMeta(std::shared_ptr<TaskContext>& ctx, FetchedMetaInfo const& meta) { return {}; }
};


} // namespace edm