#pragma once

#include <QDialog>


namespace Ui {
class TaskInformationDialog;
}

class TaskInformationDialog : public QDialog {
    Q_OBJECT

public:
    explicit TaskInformationDialog(QWidget* parent = nullptr);
    ~TaskInformationDialog();

private:
    Ui::TaskInformationDialog* ui;
};
