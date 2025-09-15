#include "NewTaskDialog.h"
#include "config/EdmGlobalConfig.h"
#include "event/EventBus.h"
#include "ui_NewTaskDialog.h"

#include <qclipboard.h>
#include <qfiledialog.h>

namespace edm {


NewTaskDialog::NewTaskDialog(QWidget* parent) : QDialog(parent), ui(new Ui::NewTaskDialog) {
    ui->setupUi(this);

    setWindowTitle("新建任务");

    auto    clipboard = QApplication::clipboard();
    QString text      = clipboard->text();
    if (text.startsWith("http://") || text.startsWith("https://")) {
        ui->urlInput_->setText(text);
    }

    ui->dirInput_->setText(EdmGlobalConfig::instance().getSaveDir());
    ui->useProxyCheckBox_->setChecked(EdmGlobalConfig::instance().canUseProxy());
}

NewTaskDialog::~NewTaskDialog() { delete ui; }

void NewTaskDialog::on_pickDirButton__clicked() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择文件夹", QDir::homePath());
    if (dir.isEmpty()) {
        qDebug() << "没有选择文件夹";
        return;
    }
    ui->dirInput_->setText(dir);
}

void NewTaskDialog::on_dialogButtonBox__accepted() {
    emit EventBus::instance()
        -> onCreateTask(ui->urlInput_->text(), ui->dirInput_->text(), ui->useProxyCheckBox_->isChecked());
    this->accept();
}

void NewTaskDialog::on_dialogButtonBox__rejected() { this->reject(); }


} // namespace edm
