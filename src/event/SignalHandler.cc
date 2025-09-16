#include "SignalHandler.h"

#include "EasyDownloadManager.h"
#include "EventBus.h"
#include "downloader/TaskConfigure.h"
#include "ui/main/MainWindow.h"
#include "ui/new_task/NewTaskDialog.h"
#include "utils/StringUtils.h"

#include <QApplication>

namespace edm {

SignalHandler* SignalHandler::instance_ = new SignalHandler{};


SignalHandler::SignalHandler() {
    connect(
        EventBus::instance(),
        &EventBus::onRequestOpenNewTaskDialog,
        this,
        &SignalHandler::handleRequestOpenNewTaskDialog
    );

    connect(EventBus::instance(), &EventBus::onRequestCreateTask, this, &SignalHandler::handleRequestCreateTask);

    connect(
        EventBus::instance(),
        &EventBus::onRequestOpenSettingDialog,
        this,
        &SignalHandler::handleRequestOpenSettingDialog
    );
}


SignalHandler::~SignalHandler() = default;

SignalHandler* SignalHandler::instance() { return instance_; }

void SignalHandler::handleRequestOpenNewTaskDialog(QWidget* parent) const {
    if (!parent || !parent->isVisible()) {
        parent = EasyDownloadManager::getOrNewInstance().getMainWindow();
    }
    auto dialog = new NewTaskDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void SignalHandler::handleRequestCreateTask(QString const& url, QString const& saveDir, bool useProxy) const {
    qDebug() << "SignalHandler::handleRequestCreateTask" << url << saveDir << useProxy;
}
void SignalHandler::handleRequestOpenSettingDialog() const {
    EasyDownloadManager::getOrNewInstance().tryShowSettingDialog();
}


} // namespace edm