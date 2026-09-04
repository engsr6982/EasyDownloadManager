#pragma once

#include <QObject>

namespace edm {

/**
 * @brief Bridges EventBus signals to C++ backend actions.
 *
 * In the Widget era this was the primary seam between UI and engine.
 * After migration to QML, most actions go through QmlTaskListModel directly,
 * but this handler is kept for any code that still emits EventBus signals.
 */
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
