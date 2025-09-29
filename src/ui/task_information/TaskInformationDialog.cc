#include "TaskInformationDialog.h"
#include "model/TaskModel.h"
#include "ui_TaskInformationDialog.h"
#include "utils/IconUtils.h"
#include "utils/StringUtils.h"
#include "utils/Utils.h"

#include <magic_enum/magic_enum.hpp>

namespace edm {


TaskInformationDialog::TaskInformationDialog(TaskModel const& model, QWidget* parent)
: QDialog(parent),
  ui(new Ui::TaskInformationDialog) {
    ui->setupUi(this);

    auto icon   = icon_utils::EdmIconProvider::getIconWithFileExtension(string_utils::string2qstring(model.fileName));
    auto pixmap = icon.pixmap({48, 48});
    ui->fileIcon_->setPixmap(pixmap);
    ui->fileName_->setText(string_utils::string2qstring(model.fileName));
    ui->fileSize_->setText(string_utils::string2qstring(utils::FileSize2String(model.fileSize)));
    ui->taskState_->setText(string_utils::stringview2qstring(magic_enum::enum_name(model.state)));
    ui->firstTry_->setText(string_utils::string2qstring(utils::TimeStamp2String(model.firstTry)));
    ui->lastTry_->setText(string_utils::string2qstring(utils::TimeStamp2String(model.lastTry)));
    ui->saveDirInput_->setText(string_utils::string2qstring(model.saveDir));
    ui->tempDirInput_->setText(string_utils::string2qstring(model.tempDir));
    ui->urlInput_->setText(string_utils::string2qstring(model.url));
    ui->pageUrlInput_->setText(string_utils::string2qstring(model.pageUrl));
    ui->pageTitle_->setText(string_utils::string2qstring(model.pageTitle));
}

TaskInformationDialog::~TaskInformationDialog() { delete ui; }

} // namespace edm