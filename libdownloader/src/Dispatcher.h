#pragma once
#include "Global.h"
#include "expected.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>


namespace edm {

struct TaskConfigure;

namespace downloader {
class DownloadState;
class DownloadTask;
} // namespace downloader


class Dispatcher {
public:
    using TaskLoader           = std::function<std::shared_ptr<TaskConfigure>(TaskId)>;
    using TaskChangedCallback  = std::function<void(std::shared_ptr<TaskConfigure> const&)>;

    struct Options {
        TaskLoader           taskLoader;
        TaskChangedCallback  onTaskChanged;
    };

private:
    std::unordered_map<TaskId, std::shared_ptr<downloader::DownloadTask>> activeTasks_;
    std::mutex                                                            tasksMutex_;
    TaskLoader                                                            taskLoader_;
    TaskChangedCallback                                                   onTaskChanged_;

    void                     persistTask(std::shared_ptr<TaskConfigure> const& ctx) const;
    [[nodiscard]] Expected<> scheduleTask(std::shared_ptr<TaskConfigure> task);

public:
    explicit Dispatcher(Options options = {});
    ~Dispatcher();

    std::shared_ptr<downloader::DownloadState> getTaskState(TaskId id);
    std::shared_ptr<TaskConfigure>             getTaskConfigure(TaskId id);

    void updateAllSpeeds();

    void updateSpeed(TaskId id);

    Expected<> pauseTask(TaskId id);
    Expected<> resumeTask(TaskId id);
    Expected<> cancelTask(TaskId id);

    [[nodiscard]] Expected<> createTask(std::shared_ptr<TaskConfigure> configure);
};

} // namespace edm
