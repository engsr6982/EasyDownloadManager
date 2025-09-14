#include "Dispatcher.h"

#include "event/EventBus.h"
#include "event/SignalHandler.h"

#include <qobject.h>

namespace edm {


Dispatcher::Dispatcher() { linkMainWindowSignals(); }

Dispatcher::~Dispatcher() {}

void Dispatcher::linkMainWindowSignals() {
    QObject::connect(
        EventBus::instance(),
        &EventBus::onRequestCreateTask,
        SignalHandler::instance(),
        &SignalHandler::handleRequestCreateTask
    );

    QObject::connect(
        EventBus::instance(),
        &EventBus::onCreateTask,
        SignalHandler::instance(),
        &SignalHandler::handleCreateTask
    );

    QObject::connect(
        EventBus::instance(),
        &EventBus::onRequestOpenSettingDialog,
        SignalHandler::instance(),
        &SignalHandler::handleRequestOpenSettingDialog
    );
}


} // namespace edm