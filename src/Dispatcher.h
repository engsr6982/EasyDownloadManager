#pragma once
#include "model/TaskModel.h"

#include <QObject>
#include <mutex>

namespace edm {

enum class TaskState;
struct TaskModel;
namespace downloader {
struct DownloadRange;
class DownloadTask;
} // namespace downloader

class Dispatcher : public QObject {
    Q_OBJECT

    std::unordered_map<int, std::shared_ptr<downloader::DownloadTask>> activeTasks_;
    std::mutex                                                         tasksMutex_;

public:
    explicit Dispatcher();
    ~Dispatcher() override;

    struct TaskSnapshot {
        std::shared_ptr<edm::TaskModel>                         model;
        TaskState                                               state;
        qint64                                                  downloadedBytes;
        double                                                  speed; // byte/s
        std::vector<std::shared_ptr<downloader::DownloadRange>> ranges;
    };

    // 获取正在运行任务的快照
    std::optional<TaskSnapshot> getTaskSnapshot(int id);

    // 供 Timer 定时调用，让所有的 Task 更新一次速度计算
    void updateAllSpeeds();

    void pauseTask(int id);
    void resumeTask(int id);
    void cancelTask(int id);

public slots:
    void handleDispatchTask(std::shared_ptr<edm::TaskModel> task);
};

} // namespace edm
