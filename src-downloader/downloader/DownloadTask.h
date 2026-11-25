#pragma once
#include "Global.h"
#include "expected.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
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

/// 删除任务的临时文件族（<name>.edm / <stem>(<n>)<ext>.edm 及对应 .meta）。
/// .meta 记录归属 taskId：匹配或缺失的删除，属于其他同名任务的跳过。
/// 供宿主在"重新下载/删除任务"前清理孤儿临时文件使用。
void removeTaskTempFiles(std::string const& saveDir, std::string const& fileName, TaskId taskId);

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
    /// 准备阶段（元信息探测）请求中止：由 pause()/cancel() 置位。
    /// 探测过程不持有 mutex_，所以这条指令必须走原子量，不能再抢锁。
    std::atomic<bool> prepareAbort_{false};
    /// 是否正处于 start() 的准备阶段（网络探测 + 磁盘预分配，未持锁）
    std::atomic<bool>   preparing_{false};
    std::atomic<size_t> nextRangeIndex_{0};

    // ---- 持久 worker 协议：线程随任务常驻（任务级复用），世代号驱动重入 ----
    // - start/rearm：世代号自增唤醒驻留线程，重新领取分片；不重新创建线程
    // - pause：isRunningFlag 置 false，worker 完成当前分片后驻留（parked）而非退出
    // - cancel/析构：exitWorkers 置位，worker 线程退出并由调用方 join
    // - 最后一个驻留的 worker 触发世代收尾（finalizeTask，一次性 guard）
    std::vector<std::thread> workers_;
    std::mutex               wakeMutex_;
    std::condition_variable  wakeCv_;
    std::atomic<uint64_t>    workGeneration_{0};  // 世代号：每次 start/rearm 自增
    std::atomic<bool>        exitWorkers_{false}; // 置位后 worker 线程退出
    std::atomic<int>         parkedCount_{0};     // 驻留中的 worker 数
    std::atomic<int>         workerTarget_{0};    // 本任务 worker 数（首次启动确定）
    std::atomic<bool>        workersSpawned_{false};
    std::atomic<bool>        finalizeDone_{false};  // 世代收尾一次性 guard
    std::atomic<uint32_t>    workerLaunchCount_{0}; // 线程创建批次计数（观测/测试用）

    std::atomic<int64_t> lastDownloadedBytes_{0};
    std::atomic<double>  currentSpeed_{0.0};

    [[nodiscard]] Expected<> prepareTask();
    void                     rebuildRanges();
    [[nodiscard]] bool       loadProgress();
    void                     saveProgress() const;
    void                     rearmWorkers();
    void                     waitAllParked();
    void                     workerLoop(size_t slot);
    void                     removeTempFiles();
    /// 是否收到暂停/取消请求（准备阶段的错误不应被当作失败上报）
    [[nodiscard]] bool isAbortRequested() const noexcept {
        return prepareAbort_.load(std::memory_order_acquire) || cancelRequested_.load(std::memory_order_acquire)
            || !isRunningFlag_->load(std::memory_order_acquire);
    }
    void joinWorkers();
    void finalizeTask();
    void setState(TaskState state);
    void notifyError(std::string const& message);

public:
    DownloadTask(DownloadTask const&)            = delete;
    DownloadTask& operator=(DownloadTask const&) = delete;
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

    [[nodiscard]] uint32_t                                    workerLaunchCount() const;
    [[nodiscard]] std::shared_ptr<edm::TaskContext>           getContext() const;
    [[nodiscard]] std::shared_ptr<DownloadState>              getStateObject() const;
    [[nodiscard]] int64_t                                     getDownloadedBytes() const;
    [[nodiscard]] std::vector<std::shared_ptr<DownloadRange>> getRanges() const;
    [[nodiscard]] TaskState                                   getState() const;
};

} // namespace edm::downloader
