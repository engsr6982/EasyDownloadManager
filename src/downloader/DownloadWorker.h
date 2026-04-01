#pragma once
#include "TaskConfigure.h"

#include <QRunnable>
#include <memory>
#include <qtclasshelpermacros.h>

namespace edm::components {
struct DownloadRange;
}

namespace edm ::downloader {

class DownloadWorker final : public QRunnable {
    TaskConfigure                              config_;
    std::string                                outFilePath_;
    std::shared_ptr<components::DownloadRange> range_;
    std::shared_ptr<std::atomic<bool>>         isTaskRunning_; // 用于接收上层的暂停/取消指令

    std::function<void(bool success)> onFinished_;

public:
    Q_DISABLE_COPY_MOVE(DownloadWorker)

    DownloadWorker(
        TaskConfigure                              config,
        std::string                                outFilePath,
        std::shared_ptr<components::DownloadRange> range,
        std::shared_ptr<std::atomic<bool>>         isTaskRunning,
        std::function<void(bool)>                  onFinished
    );
    ~DownloadWorker() override;

    void run() override;

private:
    static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata);
};

} // namespace edm::downloader
