#pragma once
#include "Global.h"

#include <memory>
#include <string>

namespace edm {

struct TaskModel {
    TaskPrimaryKeyID id{kInvalidTaskID};               // 主键
    std::string      url;                              // 下载链接
    std::string      fileName{kInvalidFileName};       // 文件名
    FileSize         fileSize{kInvalidFileSize};       // 文件大小 (byte)
    Category         category{kInvalidCategory};       // 分类
    TaskState        state{kInvalidTaskState};         // 任务状态
    BandLimit        bandLimit{kInvalidBandLimit};     // 带宽限制 (0: 未设置 | 单位 KB/s)
    int              threadCount{kInvalidThreadCount}; // 下载线程数 (0 为未设置)
    time_t           firstTry;                         // 首次尝试时间戳
    time_t           lastTry;                          // 最后一次尝试时间戳
    std::string      userAgent;                        // User-Agent
    Resumable        resumable{kInvalidResumable};     // 是否支持断点续传
    std::string      pageUrl;                          // 页面链接
    std::string      pageTitle;                        // 页面标题
    std::string      mimeType;                         // MIME 类型
    std::string      errorMsg;                         // 错误信息
    std::string      saveDir;                          // 保存目录

    [[nodiscard]] inline static std::shared_ptr<TaskModel> make() { return std::make_shared<TaskModel>(); }

    template <typename... Args>
        requires std::constructible_from<TaskModel, Args...>
    [[nodiscard]] inline static std::shared_ptr<TaskModel> make(Args&&... args) {
        return std::make_shared<TaskModel>(std::forward<Args>(args)...);
    }
};

struct TaskHeaderModel {
    TaskPrimaryKeyID id{kInvalidTaskID}; // 主键(与 TaskModel 一致)
    std::string      origin;             // Origin
    std::string      cookie;             // Cookie
    std::string      referer;            // Referer
};

} // namespace edm