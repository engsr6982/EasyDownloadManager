#pragma once
#include "CurlEx.h"
#include "Global.h"
#include "expected.h"

#include <atomic>
#include <memory>
#include <string>

namespace edm {
struct TaskContext;
}

namespace edm::downloader {

struct DownloadRange;
struct WorkerSlot;

class DownloadWorker final {
    std::shared_ptr<TaskContext>       config_;
    std::string                        outFilePath_;
    std::shared_ptr<std::atomic<bool>> isTaskRunning_; // 用于接收上层的暂停/取消指令
    WorkerSlot*                        slot_{nullptr}; // 本线程的状态槽位（可为空）
    curl::CurlEx                       curl_;          // 线程内持久句柄：跨分片复用连接缓存

public:
    DownloadWorker(
        std::shared_ptr<TaskContext>       config,
        std::string                        outFilePath,
        std::shared_ptr<std::atomic<bool>> isTaskRunning,
        WorkerSlot*                        slot = nullptr
    );
    ~DownloadWorker();

    DownloadWorker(DownloadWorker const&)            = delete;
    DownloadWorker& operator=(DownloadWorker const&) = delete;
    DownloadWorker(DownloadWorker&&)                 = delete;
    DownloadWorker& operator=(DownloadWorker&&)      = delete;

    [[nodiscard]] Expected<> downloadRange(std::shared_ptr<DownloadRange> const& range);
    [[nodiscard]] Expected<> downloadWholeFile(std::shared_ptr<DownloadRange> const& range);

private:
    static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata);
    static size_t headerCallback(char* ptr, size_t size, size_t nmemb, void* userdata);
};

} // namespace edm::downloader
