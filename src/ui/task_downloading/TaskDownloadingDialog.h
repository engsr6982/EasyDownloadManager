#pragma once

#include <QDialog>

namespace Ui {
class TaskDownloadingDialog;
}

namespace edm {
namespace components {
class ThreadProgressBar;
}

class TaskDownloadingDialog : public QDialog {
    Q_OBJECT

public:
    explicit TaskDownloadingDialog(int taskId, QWidget* parent = nullptr);
    ~TaskDownloadingDialog();

private:
    void setupUI();
    void updateUI();

private:
    Ui::TaskDownloadingDialog* ui;
    int                        taskId_;
    QTimer*                    updateTimer_;
};

} // namespace edm
