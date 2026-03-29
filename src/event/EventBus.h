#pragma once
#include <qobject.h>

namespace edm {

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
     * 请求创建任务对话框 (当对话框接受后，触发 onRequestCreateTask 信号)
     */
    void onRequestOpenNewTaskDialog(bool checked = false) const;

    /**
     * 请求创建任务 (NewTaskDialog::accept 时发出信号)
     * @param url 任务地址
     * @param saveDir 保存路径
     * @param useProxy 是否需要代理
     */
    void onRequestCreateTask(QString const& url, QString const& saveDir, bool useProxy) const;

    void onRequestOpenSettingDialog(bool checked = false) const; // 请求打开设置对话框

    void onRequestOpenTaskInfoDialog(int id) const; // 请求打开任务信息对话框

    /**
     * 任务元信息获取完毕
     * @param result 结果
     */
    void onTaskMetaInfoFetched(edm::MetaInfoResultEvent const& result) const;
};

} // namespace edm
