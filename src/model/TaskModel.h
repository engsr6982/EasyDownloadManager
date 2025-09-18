#pragma once
#include "Definition.h"

#include <string>


namespace edm {

enum class RequestType {
    GET, // GET 请求
    POST // POST 请求
};

enum class Category {
    Video,       // 视频
    Audio,       // 音频
    Compressed,  // 压缩包
    Document,    // 文档
    Application, // 应用程序
    Other        // 其它
};

enum class TaskState {
    Pending = 0, // 等待开始
    Running,     // 正在运行
    Paused,      // 暂停
    Canceled,    // 取消
    Finished,    // 完成
    Failed,      // 失败(异常)
};

enum class Resumable {
    Unknown = -1, // 未知
    No      = 0,  // 不支持断点续传
    Yes     = 1   // 支持断点续传
};

struct TaskModel {
    int            id;             // 主键
    std::string    url;            // 下载链接
    RequestType    method;         // 请求类型 (默认 GET)
    std::string    fileName;       // 文件名
    FileSize       fileSize;       // 文件大小 (byte)
    Category       category;       // 分类
    TaskState      state;          // 任务状态
    BandWidthLimit bandWidthLimit; // 带宽限制 (0: 未设置 | 单位 KB/s)
    int            threadCount;    // 下载线程数 (0 为未设置)
    int64_t        firstTry;       // 首次尝试时间戳
    int64_t        lastTry;        // 最后一次尝试时间戳
    std::string    userAgent;      // User-Agent
    Resumable      resumable;      // 是否支持断点续传
    std::string    pageUrl;        // 页面链接
    std::string    pageTitle;      // 页面标题
    std::string    mimeType;       // MIME 类型
    std::string    errorMsg;       // 错误信息
    std::string    postBody;       // POST 请求的 Body
    std::string    saveDir;        // 保存目录
    std::string    tempDir;        // 临时目录
};

struct TaskHeaderModel {
    int         id;      // 主键(与 TaskModel 一致)
    std::string origin;  // Origin
    std::string cookie;  // Cookie
    std::string referer; // Referer
};

} // namespace edm