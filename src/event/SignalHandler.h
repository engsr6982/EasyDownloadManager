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
    void handleRequestCreateTask(QString const& url, QString const& saveDir, bool useProxy) const;
};

} // namespace edm
