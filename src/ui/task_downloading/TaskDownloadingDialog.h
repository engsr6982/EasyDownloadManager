#ifndef TASKDOWNLOADINGDIALOG_H
#define TASKDOWNLOADINGDIALOG_H

#include <QDialog>

namespace Ui {
class TaskDownloadingDialog;
}

class TaskDownloadingDialog : public QDialog {
    Q_OBJECT

public:
    explicit TaskDownloadingDialog(QWidget* parent = nullptr);
    ~TaskDownloadingDialog();

private:
    Ui::TaskDownloadingDialog* ui;
};

#endif // TASKDOWNLOADINGDIALOG_H
