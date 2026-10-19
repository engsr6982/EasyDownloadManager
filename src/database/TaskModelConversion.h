#pragma once

#include "Global.h"
#include "TaskModel.h"
#include "downloader/DownloadTypes.h"
#include "utils/Utils.h"

#include <memory>
#include <string>

namespace edm {

/// 将 int 线程数转换为 AvailableThreads 枚举
inline AvailableThreads toAvailableThreads(int threadCount) {
    switch (threadCount) {
    case 4:
        return AvailableThreads::k4;
    case 8:
        return AvailableThreads::k8;
    case 16:
        return AvailableThreads::k16;
    case 32:
        return AvailableThreads::k32;
    default:
        return kDefaultThreadCount;
    }
}

/// 将 TaskModel (持久化模型) 转换为 TaskContext (引擎配置)
///
/// @param model  持久化的任务模型
/// @param header 可选的 HTTP 头部信息 (origin/referer/cookie)
/// @return 引擎可用的 TaskContext
inline std::shared_ptr<TaskContext>
toTaskContext(std::shared_ptr<TaskModel> const& model, std::shared_ptr<TaskHeaderModel> const& header = nullptr) {
    if (!model) return nullptr;

    auto cfg       = std::make_shared<TaskContext>(model->id, model->url, model->saveDir);
    cfg->threads   = toAvailableThreads(model->threadCount);
    cfg->bandLimit = model->bandLimit;
    cfg->userAgent = model->userAgent;
    cfg->fileName  = model->fileName; // 初始文件名（URL 解析值），引擎可在获取 meta 后更新
    cfg->state     = model->state;    // 初始状态（DB 中的持久化状态）

    // 从 TaskHeaderModel 填充 HTTP 头字段
    if (header) {
        if (!header->origin.empty()) cfg->origin = header->origin;
        if (!header->referer.empty()) cfg->referer = header->referer;
        if (!header->cookie.empty()) cfg->cookie = header->cookie;
    }

    return cfg;
}

/// 将 TaskContext 的变更同步回 TaskModel (用于 onTaskChanged 持久化回调)
///
/// @param model 要更新的持久化模型
/// @param cfg   引擎中的最新配置 (可能包含已获取的 metaInfo)
inline void applyContextToModel(TaskModel& model, TaskContext const& cfg) {
    model.state       = cfg.state;
    model.bandLimit   = cfg.bandLimit;
    model.threadCount = static_cast<int>(cfg.threads);
    model.userAgent   = cfg.userAgent;

    // 将 FetchedMetaInfo 中的信息同步到 TaskModel
    if (cfg.metaInfo) {
        auto const& meta = *cfg.metaInfo;

        model.resumable = meta.supportRange ? Resumable::Yes : Resumable::No;

        if (meta.fileSize) {
            model.fileSize = *meta.fileSize;
        }

        if (meta.contentType && !meta.contentType->empty()) {
            model.mimeType = *meta.contentType;
        }

        // 如果服务器提供了 Content-Disposition 文件名，优先使用
        if (meta.contentDisposition && !meta.contentDisposition->empty()) {
            // Content-Disposition 通常形如 attachment; filename="xxx"
            auto const& disp = *meta.contentDisposition;
            auto        pos  = disp.find("filename=");
            if (pos != std::string::npos) {
                auto start = pos + 9; // strlen("filename=")
                auto fname = disp.substr(start);
                // 去除可能的引号
                if (!fname.empty() && (fname.front() == '"' || fname.front() == '\'')) {
                    fname = fname.substr(1);
                }
                if (!fname.empty() && (fname.back() == '"' || fname.back() == '\'')) {
                    fname.pop_back();
                }
                if (!fname.empty()) {
                    model.fileName = fname;
                }
            }
        }
    }

    // cfg.fileName 由引擎解析（优先 Content-Disposition，其次 URL），具有最高优先级
    if (!cfg.fileName.empty()) {
        model.fileName = cfg.fileName;
        // 分类跟随最终文件名解析
        model.category = utils::resolveFileCategory(model.fileName);
    }
}

} // namespace edm
