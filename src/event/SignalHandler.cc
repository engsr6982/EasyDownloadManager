#include "SignalHandler.h"

#include "EasyDownloadManager.h"
#include "EventBus.h"
#include "ui/main/MainWindow.h"
#include "ui/new_task/NewTaskDialog.h"

#include <QApplication>

namespace edm {

SignalHandler* SignalHandler::instance_ = new SignalHandler{};


SignalHandler::SignalHandler() {
    connect(EventBus::instance(), &EventBus::onRequestCreateTask, this, &SignalHandler::handleRequestCreateTask);

    connect(EventBus::instance(), &EventBus::onCreateTask, this, &SignalHandler::handleCreateTask);

    connect(
        EventBus::instance(),
        &EventBus::onRequestOpenSettingDialog,
        this,
        &SignalHandler::handleRequestOpenSettingDialog
    );
}


SignalHandler::~SignalHandler() = default;

SignalHandler* SignalHandler::instance() { return instance_; }

void SignalHandler::handleRequestCreateTask(QWidget* parent) const {
    if (!parent || !parent->isVisible()) {
        parent = EasyDownloadManager::getOrNewInstance().getMainWindow();
    }

    auto dialog = new NewTaskDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void SignalHandler::handleCreateTask(QString const& url, QString const& dir, bool useProxy) const {
    qDebug() << "SignalHandler::handleCreateTask" << url << dir << useProxy;
    // TODO: impl
}
void SignalHandler::handleRequestOpenSettingDialog() const {
    EasyDownloadManager::getOrNewInstance().tryShowSettingDialog();
}


} // namespace edm