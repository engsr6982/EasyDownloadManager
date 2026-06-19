#pragma once
#include "DownloadState.h"
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
struct TaskContext;
enum class TaskState;
} // namespace edm

namespace edm::downloader {

class DownloadTask final : public std::enable_shared_from_this<DownloadTask> {
public:
    using StateChangedCallback = std::function<void(std::shared_ptr<edm::TaskContext> const&)>;

private:
    std::shared_ptr<TaskContext>       context_{nullptr};
    std::shared_ptr<DownloadState>     state_{std::make_shared<DownloadState>()};
    StateChangedCallback               onStateChanged_;
    mutable std::shared_mutex          mutex_;
    std::string                        outFilePath_;
    std::string                        metaFilePath_;
    std::shared_ptr<std::atomic<bool>> isRunningFlag_;
    std::atomic<bool>                  cancelRequested_{false};
    std::atomic<size_t>                nextRangeIndex_{0};

    std::atomic<int> activeWorkers_{0};
    int64_t          lastDownloadedBytes_{0};
    double           currentSpeed_{0.0};
    std::vector<std::thread> workers_;

    [[nodiscard]] Expected<> prepareTask();
    void               rebuildRanges();
    [[nodiscard]] bool loadProgress();
    void               saveProgress() const;
    void               launchWorkers();
    void               workerLoop();
    void               joinWorkers();
    void               finalizeTask();
    void               setState(TaskState state);
    void               setError(std::string message);
    void               notifyStateChanged() const;

public:
    DownloadTask(const DownloadTask&)            = delete;
    DownloadTask& operator=(const DownloadTask&) = delete;
    DownloadTask(DownloadTask&&)                 = delete;
    DownloadTask& operator=(DownloadTask&&)      = delete;

    explicit DownloadTask(std::shared_ptr<edm::TaskContext> ctx, StateChangedCallback onStateChanged = {});
    ~DownloadTask();

    [[nodiscard]] Expected<> start();
    [[nodiscard]] Expected<> pause();
    [[nodiscard]] Expected<> resume();
    [[nodiscard]] Expected<> cancel();

    [[nodiscard]] bool isFinished() const;
    [[nodiscard]] bool isPaused() const;
    [[nodiscard]] bool isCanceled() const;
    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] double getProgress();

    [[nodiscard]] double getSpeed() const;
    void                 updateSpeed();

    [[nodiscard]] std::shared_ptr<edm::TaskContext> getTaskContext() const;
    [[nodiscard]] std::shared_ptr<DownloadState> getStateObject() const;
    [[nodiscard]] int64_t getDownloadedBytes() const;
    [[nodiscard]] std::vector<std::shared_ptr<DownloadRange>> getRanges() const { return state_->ranges; }
    [[nodiscard]] std::optional<std::string> getLastError();
    [[nodiscard]] TaskState getState() const;
};

} // namespace edm::downloader
