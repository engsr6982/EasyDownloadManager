#pragma once
#include <QObject>

namespace edm {

class SignalHandler : public QObject {
    Q_OBJECT;

public:
    Q_DISABLE_COPY_MOVE(SignalHandler);
    explicit SignalHandler();
    ~SignalHandler() override;

    [[nodiscard]] static SignalHandler* instance();

public slots:
    void handleShowNewTaskDialog(bool checked = false) const;
    void handleRequestCreateTask(QString const& url, QString const& saveDir, bool useProxy) const;
    void handleShowSettingDialog(bool checked = false) const;
    void handleShowTaskInfoDialog(int id) const;

    void handleShowDownloadingDialog(int id) const;
};

} // namespace edm
