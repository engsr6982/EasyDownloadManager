#ifndef TASKCOMPLETEDDIALOG_H
#define TASKCOMPLETEDDIALOG_H

#include <QDialog>

namespace Ui {
class TaskCompletedDialog;
}

class TaskCompletedDialog : public QDialog {
    Q_OBJECT

public:
    explicit TaskCompletedDialog(QWidget* parent = nullptr);
    ~TaskCompletedDialog();

private:
    Ui::TaskCompletedDialog* ui;
};

#endif // TASKCOMPLETEDDIALOG_H
