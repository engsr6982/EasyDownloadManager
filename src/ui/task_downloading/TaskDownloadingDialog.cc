#include "TaskDownloadingDialog.h"

#include "Dispatcher.h"
#include "EdmApplication.h"
#include "downloader/DownloadState.h"
#include "model/TaskModel.h"
#include "ui_TaskDownloadingDialog.h"
#include "utils/Utils.h"

#include <QTimer>
#include <QMessageBox>
#include <magic_enum/magic_enum.hpp>

namespace edm {

TaskDownloadingDialog::TaskDownloadingDialog(int taskId, QWidget* parent)
: QDialog(parent),
  ui(new Ui::TaskDownloadingDialog),
  taskId_(taskId) {
    ui->setupUi(this);
    auto dispatcher = EdmApplication::getInstance().getDispatcher();
    state_          = dispatcher->getTaskState(taskId_);
    model_          = dispatcher->getTaskModel(taskId_);

    setupUI();

    // UI 只读取下载任务共享状态，Dispatcher 不再每秒组装快照。
    updateTimer_ = new QTimer(this);
    updateTimer_->setInterval(1000);
    connect(updateTimer_, &QTimer::timeout, this, &TaskDownloadingDialog::updateUI);

    // 初始化时立刻刷新一次
    updateUI();
    updateTimer_->start();
}

TaskDownloadingDialog::~TaskDownloadingDialog() {
    updateTimer_->stop();
    delete ui;
}

void TaskDownloadingDialog::setupUI() {
    // 2. 绑定按钮事件
    connect(ui->pauseOrResumeBtn_, &QPushButton::clicked, this, [this]() {
        auto dispatcher = EdmApplication::getInstance().getDispatcher();
        if (state_) {
            if (state_->state() == TaskState::Running) {
                auto paused = dispatcher->pauseTask(taskId_);
                if (!paused) {
                    QMessageBox::warning(this, "暂停失败", QString::fromStdString(paused.error().message()));
                    return;
                }
            } else if (state_->state() == TaskState::Paused) {
                auto resumed = dispatcher->resumeTask(taskId_);
                if (!resumed) {
                    QMessageBox::warning(this, "恢复失败", QString::fromStdString(resumed.error().message()));
                    return;
                }
                state_ = dispatcher->getTaskState(taskId_);
            }
        }
    });

    connect(ui->cancelBtn_, &QPushButton::clicked, this, [this]() {
        auto canceled = EdmApplication::getInstance().getDispatcher()->cancelTask(taskId_);
        if (!canceled) {
            QMessageBox::warning(this, "取消失败", QString::fromStdString(canceled.error().message()));
            return;
        }
        this->close();
    });
}

void TaskDownloadingDialog::updateUI() {
    if (!state_ || !model_) {
        // 任务可能已被清理
        ui->state_->setText("已移除");
        return;
    }

    auto& model = model_;

    // 1. 基础信息更新
    ui->url_->setText(QString::fromStdString(model->url));
    ui->size_->setText(QString::fromStdString(utils::FileSize2String(model->fileSize)));

    auto currentState = state_->state();
    ui->state_->setText(QString::fromStdString(magic_enum::enum_name(currentState).data()));
    if (currentState == TaskState::Finished || currentState == TaskState::Failed) {
        updateTimer_->stop();
        this->close(); // 任务完成或失败后自动关闭对话框
        // TODO: 改进窗口关闭逻辑，使用全局派发的信号
    }

    ui->resumable_->setText(model->resumable == Resumable::Yes ? "支持" : "不支持");

    // 2. 进度和速度更新
    QString speedStr = QString::fromStdString(utils::FileSize2String(state_->speed())) + "/s";
    ui->speed_->setText(speedStr);

    double progressRatio = 0.0;
    if (model->fileSize > 0) {
        progressRatio = static_cast<double>(state_->downloadedBytes()) / model->fileSize;
    }

    QString progressText = QString("%1 (%2%)")
                               .arg(QString::fromStdString(utils::FileSize2String(state_->downloadedBytes())))
                               .arg(progressRatio * 100, 0, 'f', 2);
    ui->progress_->setText(progressText);

    ui->downloadProgressBar_->setValue(static_cast<int>(progressRatio * 100));

    // 3. 计算剩余时间
    if (currentState == TaskState::Running && state_->speed() > 0) {
        qint64 remainBytes   = model->fileSize - state_->downloadedBytes();
        qint64 remainSeconds = remainBytes / static_cast<qint64>(state_->speed());
        ui->remTime_->setText(QString("%1s").arg(remainSeconds));
    } else {
        ui->remTime_->setText("--");
    }

    // 5. 同步按钮状态
    if (currentState == TaskState::Running) {
        ui->pauseOrResumeBtn_->setText("暂停");
        ui->pauseOrResumeBtn_->setEnabled(true);
    } else if (currentState == TaskState::Paused) {
        ui->pauseOrResumeBtn_->setText("恢复");
        ui->pauseOrResumeBtn_->setEnabled(model->resumable == Resumable::Yes); // 不支持断点续传的不能恢复
    } else {
        ui->pauseOrResumeBtn_->setEnabled(false);
    }

    // TODO: 如果你需要刷新 connects_ 表格（每个线程的独立信息），可以遍历 snapshot->ranges 在此更新表格行
}

} // namespace edm
