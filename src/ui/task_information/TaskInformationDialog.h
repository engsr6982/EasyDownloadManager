#ifndef TASKINFORMATIONDIALOG_H
#define TASKINFORMATIONDIALOG_H

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

#endif // TASKINFORMATIONDIALOG_H
