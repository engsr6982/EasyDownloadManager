#pragma once
#include <qobject.h>

namespace edm {

class SignalHandler : public QObject {
    Q_OBJECT;

    static SignalHandler* instance_;

public:
    Q_DISABLE_COPY_MOVE(SignalHandler);
    explicit SignalHandler();
    ~SignalHandler() override;

    static SignalHandler* instance();

public slots:
    void handleRequestOpenNewTaskDialog(bool checked = false) const;
    void handleRequestCreateTask(QString const& url, QString const& saveDir, bool useProxy) const;
    void handleRequestOpenSettingDialog(bool checked = false) const;
    void handleRequestOpenTaskInfoDialog(int id) const;
};

} // namespace edm
