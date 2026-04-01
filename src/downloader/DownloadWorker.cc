#include "DownloadWorker.h"
#include "CurlEx.h"
#include "components/ThreadProgressBar.h"

#include <fmt/format.h>

namespace edm ::downloader {


DownloadWorker::DownloadWorker(
    TaskConfigure                              config,
    std::string                                outFilePath,
    std::shared_ptr<components::DownloadRange> range,
    std::shared_ptr<std::atomic<bool>>         isTaskRunning,
    std::function<void(bool)>                  onFinished
)
: config_(std::move(config)),
  outFilePath_(std::move(outFilePath)),
  range_(std::move(range)),
  isTaskRunning_(std::move(isTaskRunning)),
  onFinished_(std::move(onFinished)) {
    setAutoDelete(true); // 让 QThreadPool 自动清理
}
DownloadWorker::~DownloadWorker() = default;

struct WorkerContext {
    std::FILE*                 fp;
    components::DownloadRange* range;
    std::atomic<bool>*         isTaskRunning;
};

size_t DownloadWorker::writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto ctx = static_cast<WorkerContext*>(userdata);

    // 如果上层调用了 pause() 或 cancel()，isRunning 变成 false
    if (!ctx->isTaskRunning->load(std::memory_order_relaxed)) {
        return CURL_WRITEFUNC_PAUSE; // 中断 libcurl 传输
    }

    size_t totalBytes = size * nmemb;

    // 写入文件 (不需要加锁，因为每个 worker 有自己独立的 fp 和互不重叠的文件区块)
    if (std::fwrite(ptr, size, nmemb, ctx->fp) != nmemb) {
        return 0; // 写入磁盘失败，中断传输
    }

    // 更新进度条用的原子变量
    ctx->range->downloaded.fetch_add(totalBytes, std::memory_order_relaxed);

    return totalBytes;
}

void DownloadWorker::run() {
    qDebug() << fmt::format("Worker started for range: {}-{}", range_->start, range_->end).c_str();
    bool success = false;

    // 打开独立的文件句柄 (rb+ 允许随机读写，不截断文件)
    std::FILE* fp = nullptr;
#ifdef _WIN32
    fp = _fsopen(outFilePath_.c_str(), "rb+", _SH_DENYNO);
#else
    fp = std::fopen(outFilePath_.c_str(), "rb+");
#endif

    if (!fp) {
        qDebug() << "Failed to open file for writing:" << outFilePath_.c_str();
        if (onFinished_) onFinished_(false);
        return;
    }

    // 定位到当前应该写入的偏移量
    qint64 currentOffset = range_->start + range_->downloaded.load(std::memory_order_relaxed);

#ifdef _WIN32
    _fseeki64(fp, currentOffset, SEEK_SET); // Windows 支持 >2GB 文件的 fseek
#else
    fseeko(fp, currentOffset, SEEK_SET); // Linux/macOS 支持 >2GB 文件的 fseek
#endif

    // 配置 CURL
    auto curlExRes = config_.newCurl();
    if (!curlExRes) {
        std::fclose(fp);
        return;
    }
    CurlEx curl = std::move(curlExRes.value());

    // 告诉服务器我们要下载的精确范围
    std::string rangeStr = fmt::format("{}-{}", currentOffset, range_->end);
    curl.setOpt(CURLOPT_RANGE, rangeStr.c_str());

    // 设置写入回调
    WorkerContext ctx{fp, range_.get(), isTaskRunning_.get()};
    curl.setOpt(CURLOPT_WRITEFUNCTION, writeCallback);
    curl.setOpt(CURLOPT_WRITEDATA, &ctx);

    // 开始下载
    auto performRes = curl.perform();
    if (performRes) {
        // 判断这块是否真的下完了
        qint64 expected = range_->end - range_->start + 1;
        if (range_->downloaded.load() == expected) {
            success = true;
        }
    } else {
        qDebug() << "Worker perform error:" << performRes.error().message().c_str();
    }

    // 资源清理
    std::fclose(fp);
    qDebug() << fmt::format("Worker finished/stopped for range: {}-{}", range_->start, range_->end).c_str();

    if (onFinished_) onFinished_(success); // 通知上层该线程退出了
}


} // namespace edm::downloader