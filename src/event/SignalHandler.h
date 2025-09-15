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
    void handleRequestCreateTask(QWidget* parent) const;
    void handleCreateTask(QString const& url, QString const& dir, bool useProxy) const;
    void handleRequestOpenSettingDialog() const;
};

} // namespace edm
