#pragma once
#include "expected.h"
#include "model/TaskModel.h"

#include <QObject>
#include <memory>
#include <mutex>
#include <unordered_map>


namespace edm {

struct TaskContext;
struct TaskModel;

namespace downloader {
class DownloadState;
class DownloadTask;
} // namespace downloader


class Dispatcher : public QObject {
    Q_OBJECT

    std::unordered_map<TaskId, std::shared_ptr<downloader::DownloadTask>> activeTasks_;
    std::mutex                                                            tasksMutex_;

public:
    explicit Dispatcher();
    ~Dispatcher() override;

    std::shared_ptr<downloader::DownloadState> getTaskState(TaskId id);
    std::shared_ptr<TaskModel> getTaskModel(TaskId id);

    void updateAllSpeeds();

    void updateSpeed(TaskId id);

    Expected<> pauseTask(TaskId id);
    Expected<> resumeTask(TaskId id);
    Expected<> cancelTask(TaskId id);

public slots:
    void handleTaskCreated(std::shared_ptr<edm::TaskContext> task);
};

} // namespace edm
