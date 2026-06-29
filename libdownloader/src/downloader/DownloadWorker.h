#pragma once
#include "Global.h"
#include "expected.h"

#include <atomic>
#include <memory>
#include <string>

namespace edm {
struct TaskConfigure;
}

namespace edm ::downloader {

struct DownloadRange;

class DownloadWorker final {
    std::shared_ptr<TaskConfigure>     config_;
    std::string                        outFilePath_;
    std::shared_ptr<std::atomic<bool>> isTaskRunning_; // 用于接收上层的暂停/取消指令

public:
    DownloadWorker(
        std::shared_ptr<TaskConfigure>     config,
        std::string                        outFilePath,
        std::shared_ptr<std::atomic<bool>> isTaskRunning,
        TaskId                             taskId = kInvalidTaskID
    );
    ~DownloadWorker();

    DownloadWorker(const DownloadWorker&)            = delete;
    DownloadWorker& operator=(const DownloadWorker&) = delete;
    DownloadWorker(DownloadWorker&&)                 = delete;
    DownloadWorker& operator=(DownloadWorker&&)      = delete;

    [[nodiscard]] Expected<> downloadRange(std::shared_ptr<DownloadRange> const& range) const;
    [[nodiscard]] Expected<> downloadWholeFile(std::shared_ptr<DownloadRange> const& range) const;

private:
    static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata);
};

} // namespace edm::downloader
