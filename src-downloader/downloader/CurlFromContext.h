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

} // namespace edm::downloader
