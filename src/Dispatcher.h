#pragma once
#include <QObject>
#include <mutex>

namespace edm {
enum class TaskState;

struct TaskModel;
namespace downloader {
class DownloadTask;
}

class Dispatcher : public QObject {
    Q_OBJECT

    std::unordered_map<int, std::shared_ptr<downloader::DownloadTask>> activeTasks_;
    std::mutex                                                         tasksMutex_;

public:
    explicit Dispatcher();
    ~Dispatcher() override;

    struct TaskSnapshot {
        double    progress;
        double    speed;
        TaskState state;
    };

    // 获取正在运行任务的快照
    std::optional<TaskSnapshot> getTaskSnapshot(int id);

    // 供 Timer 定时调用，让所有的 Task 更新一次速度计算
    void updateAllSpeeds();

public slots:
    void handleDispatchTask(edm::TaskModel const& task);
};

} // namespace edm
