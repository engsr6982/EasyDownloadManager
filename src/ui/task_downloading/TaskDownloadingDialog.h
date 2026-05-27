#pragma once

#include <QDialog>
#include <memory>

namespace Ui {
class TaskDownloadingDialog;
}

namespace edm {

struct TaskModel;

namespace downloader {
class DownloadState;
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
    Ui::TaskDownloadingDialog*                 ui;
    int                                        taskId_;
    QTimer*                                    updateTimer_;
    std::shared_ptr<TaskModel>                 model_;
    std::shared_ptr<downloader::DownloadState> state_;
};

} // namespace edm
