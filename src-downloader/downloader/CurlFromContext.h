#pragma once
#include "CurlEx.h"
#include "Global.h"

#include <memory>

namespace edm {
struct TaskContext;
}

namespace edm::downloader {

/// 按 TaskContext 组装已配置的 curl 实例（UA/Cookie/Referer/Origin/代理/限速/自定义头）
Expected<curl::CurlEx> makeCurlFromContext(std::shared_ptr<TaskContext> const& cfg);

/// 将 TaskContext 的会话级选项应用到已有 curl 句柄。
/// 配合 CurlEx::reset() 使用：句柄跨传输复用（连接缓存保留），选项按次重放。
void applyContextOptions(curl::CurlEx& curl, std::shared_ptr<TaskContext> const& cfg);

} // namespace edm::downloader
