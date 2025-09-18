#include "TaskCompletedDialog.h"
#include "ui_TaskCompletedDialog.h"

TaskCompletedDialog::TaskCompletedDialog(QWidget* parent) : QDialog(parent), ui(new Ui::TaskCompletedDialog) {
    ui->setupUi(this);
}

TaskCompletedDialog::~TaskCompletedDialog() { delete ui; }
