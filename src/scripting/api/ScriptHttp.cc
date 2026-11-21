#include "ScriptHttp.h"

#include "CurlEx.h"

#include <thread>

namespace edm::script {

namespace {

constexpr std::size_t kMaxBodyBytes = 16 * 1024 * 1024;

std::once_flag& curlInitOnce() {
    static std::once_flag flag;
    return flag;
}

std::size_t writeBodyCb(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
    auto*             body  = static_cast<std::string*>(userdata);
    std::size_t const total = size * nmemb;
    if (body->size() + total > kMaxBodyBytes) return 0; // 大小与请求数不符时 curl 报错中止
    body->append(ptr, total);
    return total;
}

std::size_t writeHeaderCb(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
    auto*             headers = static_cast<std::unordered_map<std::string, std::string>*>(userdata);
    std::string const line(ptr, size * nmemb);
    auto const        sep = line.find(':');
    if (sep != std::string::npos) {
        auto key = line.substr(0, sep);
        auto val = line.substr(sep + 1);
        while (!key.empty() && (key.back() == '\r' || key.back() == '\n')) key.pop_back();
        while (!val.empty() && (val.back() == '\r' || val.back() == '\n')) val.pop_back();
        if (!val.empty() && val.front() == ' ') val.erase(0, 1);
        if (!key.empty()) (*headers)[std::move(key)] = std::move(val);
    }
    return size * nmemb;
}

HttpResult performCurl(
    std::string const&        method,
    std::string const&        url,
    HttpRequestOptions const& options,
    std::string const&        userAgent
) {
    std::call_once(curlInitOnce(), [] { curl_global_init(CURL_GLOBAL_DEFAULT); });

    HttpResult   result;
    curl::CurlEx curl;
    if (!curl) {
        result.error = "curl_easy_init failed";
        return result;
    }

    curl.setOpt(CURLOPT_URL, url.c_str())
        .setOpt(CURLOPT_FOLLOWLOCATION, 1L)
        .setOpt(CURLOPT_MAXREDIRS, 10L)
        .setOpt(CURLOPT_TIMEOUT_MS, static_cast<long>(options.timeoutMs > 0 ? options.timeoutMs : 30000))
        .setOpt(CURLOPT_NOSIGNAL, 1L)
        .setOpt(CURLOPT_ACCEPT_ENCODING, "");
    if (!userAgent.empty()) curl.setOpt(CURLOPT_USERAGENT, userAgent.c_str());
    for (auto const& [key, value] : options.headers) {
        curl.append(key + ": " + value);
    }
    if (!options.proxy.empty()) curl.setOpt(CURLOPT_PROXY, options.proxy.c_str());
    if (method == "HEAD") curl.setOpt(CURLOPT_NOBODY, 1L);
    curl.setOpt(CURLOPT_WRITEFUNCTION, writeBodyCb)
        .setOpt(CURLOPT_WRITEDATA, &result.body)
        .setOpt(CURLOPT_HEADERFUNCTION, writeHeaderCb)
        .setOpt(CURLOPT_HEADERDATA, &result.headers);

    auto const performed = curl.perform();
    if (performed) {
        result.ok   = true;
        long status = 0;
        curl.getInfo(CURLINFO_RESPONSE_CODE, &status);
        result.status  = static_cast<int>(status);
        char* finalUrl = nullptr;
        curl.getInfo(CURLINFO_EFFECTIVE_URL, &finalUrl);
        if (finalUrl) result.finalUrl = finalUrl;
    } else {
        result.error = performed.error().message();
    }
    return result;
}

} // namespace

void ScriptHttp::request(
    std::string const&              method,
    std::string const&              url,
    HttpRequestOptions const&       options,
    std::string const&              userAgent,
    std::function<void(HttpResult)> callback
) {
    std::thread([method, url, options, userAgent, callback = std::move(callback)] {
        callback(performCurl(method, url, options, userAgent));
    }).detach();
}

} // namespace edm::script
