#pragma once
#include "expected.h"

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

namespace edm {
enum class TaskState;
struct TaskContext;
struct ITaskObserver;
} // namespace edm

namespace edm::downloader {

class DownloadState;
struct DownloadRange;

class DownloadTask final : public std::enable_shared_from_this<DownloadTask> {
private:
    std::shared_ptr<TaskContext>       configure_{nullptr};
    std::shared_ptr<DownloadState>     state_{nullptr};
    ITaskObserver*                     observer_{nullptr};
    mutable std::shared_mutex          mutex_;
    std::string                        outFilePath_;
    std::string                        metaFilePath_;
    std::shared_ptr<std::atomic<bool>> isRunningFlag_;
    std::atomic<bool>                  cancelRequested_{false};
    std::atomic<size_t>                nextRangeIndex_{0};

    std::atomic<int>         activeWorkers_{0};
    int64_t                  lastDownloadedBytes_{0};
    double                   currentSpeed_{0.0};
    std::vector<std::thread> workers_;

    [[nodiscard]] Expected<> prepareTask();
    void                     rebuildRanges();
    [[nodiscard]] bool       loadProgress();
    void                     saveProgress() const;
    void                     launchWorkers();
    void                     workerLoop();
    void                     joinWorkers();
    void                     finalizeTask();
    void                     setState(TaskState state);
    void                     notifyError(std::string const& message);

public:
    DownloadTask(const DownloadTask&)            = delete;
    DownloadTask& operator=(const DownloadTask&) = delete;
    DownloadTask(DownloadTask&&)                 = delete;
    DownloadTask& operator=(DownloadTask&&)      = delete;

    explicit DownloadTask(std::shared_ptr<edm::TaskContext> configure, ITaskObserver* observer);
    ~DownloadTask();

    [[nodiscard]] Expected<> start();
    [[nodiscard]] Expected<> pause();
    [[nodiscard]] Expected<> resume();
    [[nodiscard]] Expected<> cancel();

    [[nodiscard]] bool   isFinished() const;
    [[nodiscard]] bool   isPaused() const;
    [[nodiscard]] bool   isCanceled() const;
    [[nodiscard]] bool   isRunning() const;
    [[nodiscard]] double getProgress();

    [[nodiscard]] double getSpeed() const;
    void                 updateSpeed();

    [[nodiscard]] std::shared_ptr<edm::TaskContext>           getContext() const;
    [[nodiscard]] std::shared_ptr<DownloadState>              getStateObject() const;
    [[nodiscard]] int64_t                                     getDownloadedBytes() const;
    [[nodiscard]] std::vector<std::shared_ptr<DownloadRange>> getRanges() const;
    [[nodiscard]] TaskState                                   getState() const;
};

} // namespace edm::downloader
