#pragma once
#include <qobject.h>

namespace edm {

class EventBus : public QObject {
    Q_OBJECT

    EventBus()  = default;
    ~EventBus() = default;

public:
    EventBus(const EventBus&)                          = delete;
    EventBus&               operator=(const EventBus&) = delete;
    inline static EventBus* instance() {
        static EventBus instance;
        return &instance;
    }

signals:
    void onRequestCreateTask(QWidget* parent = nullptr) const; // 请求创建任务

    void onCreateTask(QString const& url, QString const& dir, bool useProxy) const; // 创建任务
};

} // namespace edm
