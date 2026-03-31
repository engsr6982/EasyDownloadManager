#pragma once
#include <optional>
#include <string>

#include "Global.h"

namespace edm::downloader {

struct FetchedMetaInfo {
    std::string finalUrl;            // 最终请求的 URL (经过重定向后)
    bool        supportRange{false}; // 是否支持断点续传

    std::optional<FileSize>    fileSize;           // 文件大小 (未知则为空)
    std::optional<std::string> contentType;        // MIME 类型
    std::optional<std::string> etag;               // ETag (校验文件是否被修改)
    std::optional<std::string> lastModified;       // 最后修改时间
    std::optional<std::string> contentDisposition; // 服务端的建议文件名
    std::optional<std::string> md5;                // 文件完整性校验 MD5 (Content-MD5 或 x-cos-meta-md5)
};


} // namespace edm::downloader