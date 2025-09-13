#include "SignalHandler.h"

#include "EasyDownloadManager.h"
#include "windows/main/MainWindow.h"
#include "windows/new_task/NewTaskDialog.h"

#include <QApplication>

namespace edm {

SignalHandler::SignalHandler() {}

SignalHandler::~SignalHandler() {}

SignalHandler* SignalHandler::instance() {
    static SignalHandler instance;
    return &instance;
}

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
}


} // namespace edm