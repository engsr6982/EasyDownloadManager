#include "TaskInformationDialog.h"
#include "model/TaskModel.h"
#include "ui_TaskInformationDialog.h"
#include "utils/IconUtils.h"
#include "utils/Utils.h"

#include <magic_enum/magic_enum.hpp>

namespace edm {


TaskInformationDialog::TaskInformationDialog(TaskModel const& model, QWidget* parent)
: QDialog(parent),
  ui(new Ui::TaskInformationDialog) {
    ui->setupUi(this);

    auto icon   = icon_utils::EdmIconProvider::getIconWithFileExtension(QString::fromStdString(model.fileName));
    auto pixmap = icon.pixmap({48, 48});
    ui->fileIcon_->setPixmap(pixmap);
    ui->fileName_->setText(QString::fromStdString(model.fileName));
    ui->fileSize_->setText(QString::fromStdString(utils::FileSize2String(model.fileSize)));
    ui->taskState_->setText(QString::fromStdString(magic_enum::enum_name(model.state).data()));
    ui->firstTry_->setText(QString::fromStdString(utils::TimeStamp2String(model.firstTry)));
    ui->lastTry_->setText(QString::fromStdString(utils::TimeStamp2String(model.lastTry)));
    ui->saveDirInput_->setText(QString::fromStdString(model.saveDir));
    ui->tempDirInput_->setText(QString::fromStdString(model.tempDir));
    ui->urlInput_->setText(QString::fromStdString(model.url));
    ui->pageUrlInput_->setText(QString::fromStdString(model.pageUrl));
    ui->pageTitle_->setText(QString::fromStdString(model.pageTitle));
}

TaskInformationDialog::~TaskInformationDialog() { delete ui; }

} // namespace edm