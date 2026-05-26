#pragma once
#include <memory>
#include <string>

#include <QRunnable>
#include <QtClassHelperMacros>

namespace edm {
struct TaskConfigure;
}

namespace edm ::downloader {

struct DownloadRange {
    int64_t              start{0};
    int64_t              end{0};
    std::atomic<int64_t> downloaded{0};

    DownloadRange(int64_t s, int64_t e) : start(s), end(e), downloaded(0) {}
};

class DownloadWorker final : public QRunnable {
    std::shared_ptr<TaskConfigure>     config_;
    std::string                        outFilePath_;
    std::shared_ptr<DownloadRange>     range_;
    std::shared_ptr<std::atomic<bool>> isTaskRunning_; // 用于接收上层的暂停/取消指令

    std::function<void(bool success)> onFinished_;

public:
    Q_DISABLE_COPY_MOVE(DownloadWorker)

    DownloadWorker(
        std::shared_ptr<TaskConfigure>     config,
        std::string                        outFilePath,
        std::shared_ptr<DownloadRange>     range,
        std::shared_ptr<std::atomic<bool>> isTaskRunning,
        std::function<void(bool)>          onFinished
    );
    ~DownloadWorker() override;

    void run() override;

private:
    static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata);
};

} // namespace edm::downloader
