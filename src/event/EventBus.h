#pragma once
#include <qobject.h>


namespace edm {
enum class TaskState;
}
namespace edm {
struct TaskContext;

struct MetaInfoResultEvent;

class EventBus : public QObject {
    Q_OBJECT

    EventBus()           = default;
    ~EventBus() override = default;

public:
    EventBus(const EventBus&)                          = delete;
    EventBus&               operator=(const EventBus&) = delete;
    inline static EventBus* instance() {
        static EventBus instance;
        return &instance;
    }

signals:
    /**
     * 请求创建任务
     * @param url 任务地址
     * @param saveDir 保存路径
     * @param useProxy 是否需要代理
     */
    void onRequestCreateTask(QString const& url, QString const& saveDir, bool useProxy) const;

    /**
     * 任务已创建
     * @param ctx 任务上下文
     */
    void onTaskCreated(std::shared_ptr<edm::TaskContext> ctx);

    // TODO: impl
    void onTaskStateChanged(std::shared_ptr<edm::TaskContext> ctx, TaskState oldState, TaskState newState);
};

} // namespace edm
