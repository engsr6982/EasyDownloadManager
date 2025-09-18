#include "TaskInformationDialog.h"
#include "ui_TaskInformationDialog.h"

TaskInformationDialog::TaskInformationDialog(QWidget* parent) : QDialog(parent), ui(new Ui::TaskInformationDialog) {
    ui->setupUi(this);
}

TaskInformationDialog::~TaskInformationDialog() { delete ui; }
