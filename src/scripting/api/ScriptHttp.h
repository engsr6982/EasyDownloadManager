#pragma once

#include <functional>
#include <string>
#include <unordered_map>

namespace edm::script {

/// HTTP 响应结果（绑定层转换为脚本侧 HttpResponse 实例）
struct HttpResult {
    bool                                         ok{false}; // 传输层成功
    int                                          status{0}; // HTTP 状态码
    std::string                                  finalUrl;  // 重定向后的最终地址
    std::string                                  body;      // 响应体（HEAD 为空）
    std::string                                  error;     // 传输层错误（ok=false 时有效）
    std::unordered_map<std::string, std::string> headers;
};

/// 请求选项
struct HttpRequestOptions {
    std::unordered_map<std::string, std::string> headers;
    std::string                                  proxy;
    int                                          timeoutMs{30000};
};

/// 回调式 HTTP 客户端（curl；请求在工作线程执行，callback 在该线程被调用）
struct ScriptHttp {
    static void request(
        std::string const&              method, // "GET" / "HEAD"
        std::string const&              url,
        HttpRequestOptions const&       options,
        std::string const&              userAgent,
        std::function<void(HttpResult)> callback
    );
};

} // namespace edm::script
