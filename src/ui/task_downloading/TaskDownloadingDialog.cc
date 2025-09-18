#include "TaskDownloadingDialog.h"
#include "ui_TaskDownloadingDialog.h"

TaskDownloadingDialog::TaskDownloadingDialog(QWidget* parent) : QDialog(parent), ui(new Ui::TaskDownloadingDialog) {
    ui->setupUi(this);
}

TaskDownloadingDialog::~TaskDownloadingDialog() { delete ui; }
