#include "TaskDownloadingDialog.h"

#include "Dispatcher.h"
#include "EdmApplication.h"
#include "model/TaskModel.h"
#include "ui_TaskDownloadingDialog.h"
#include "utils/Utils.h"

#include <QTimer>
#include <magic_enum/magic_enum.hpp>

namespace edm {

TaskDownloadingDialog::TaskDownloadingDialog(int taskId, QWidget* parent)
: QDialog(parent),
  ui(new Ui::TaskDownloadingDialog),
  taskId_(taskId) {
    ui->setupUi(this);

    setupUI();

    // 内部起一个定时器，每 1 秒拉取一次快照刷新 UI
    updateTimer_ = new QTimer(this);
    updateTimer_->setInterval(1000);
    connect(updateTimer_, &QTimer::timeout, this, &TaskDownloadingDialog::updateUI);

    // 初始化时立刻刷新一次
    updateUI();
    updateTimer_->start();
}

TaskDownloadingDialog::~TaskDownloadingDialog() { delete ui; }

void TaskDownloadingDialog::setupUI() {
    // 2. 绑定按钮事件
    connect(ui->pauseOrResumeBtn_, &QPushButton::clicked, this, [this]() {
        auto dispatcher = EdmApplication::getInstance().getDispatcher();
        auto snapshot   = dispatcher->getTaskSnapshot(taskId_);
        if (snapshot) {
            if (snapshot->state == TaskState::Running) {
                dispatcher->pauseTask(taskId_);
                ui->pauseOrResumeBtn_->setText("恢复");
            } else if (snapshot->state == TaskState::Paused) {
                dispatcher->resumeTask(taskId_);
                ui->pauseOrResumeBtn_->setText("暂停");
            }
        }
    });

    connect(ui->cancelBtn_, &QPushButton::clicked, this, [this]() {
        EdmApplication::getInstance().getDispatcher()->cancelTask(taskId_);
        this->close();
    });
}

void TaskDownloadingDialog::updateUI() {
    auto dispatcher = EdmApplication::getInstance().getDispatcher();
    auto snapshot   = dispatcher->getTaskSnapshot(taskId_);

    if (!snapshot) {
        // 任务可能已被清理
        ui->state_->setText("已移除");
        return;
    }

    auto& model = snapshot->model;

    // 1. 基础信息更新
    ui->url_->setText(QString::fromStdString(model->url));
    ui->size_->setText(QString::fromStdString(utils::FileSize2String(model->fileSize)));
    ui->state_->setText(QString::fromStdString(magic_enum::enum_name(snapshot->state).data()));
    ui->resumable_->setText(model->resumable == Resumable::Yes ? "支持" : "不支持");

    // 2. 进度和速度更新
    QString speedStr = QString::fromStdString(utils::FileSize2String(snapshot->speed)) + "/s";
    ui->speed_->setText(speedStr);

    double progressRatio = 0.0;
    if (model->fileSize > 0) {
        progressRatio = static_cast<double>(snapshot->downloadedBytes) / model->fileSize;
    }

    QString progressText = QString("%1 (%2%)")
                               .arg(QString::fromStdString(utils::FileSize2String(snapshot->downloadedBytes)))
                               .arg(progressRatio * 100, 0, 'f', 2);
    ui->progress_->setText(progressText);

    ui->downloadProgressBar_->setValue(static_cast<int>(progressRatio * 100));

    // 3. 计算剩余时间
    if (snapshot->state == TaskState::Running && snapshot->speed > 0) {
        qint64 remainBytes   = model->fileSize - snapshot->downloadedBytes;
        qint64 remainSeconds = remainBytes / static_cast<qint64>(snapshot->speed);
        ui->remTime_->setText(QString("%1s").arg(remainSeconds));
    } else {
        ui->remTime_->setText("--");
    }

    // 5. 同步按钮状态
    if (snapshot->state == TaskState::Running) {
        ui->pauseOrResumeBtn_->setText("暂停");
        ui->pauseOrResumeBtn_->setEnabled(true);
    } else if (snapshot->state == TaskState::Paused) {
        ui->pauseOrResumeBtn_->setText("恢复");
        ui->pauseOrResumeBtn_->setEnabled(model->resumable == Resumable::Yes); // 不支持断点续传的不能恢复
    } else {
        ui->pauseOrResumeBtn_->setEnabled(false);
    }

    // TODO: 如果你需要刷新 connects_ 表格（每个线程的独立信息），可以遍历 snapshot->ranges 在此更新表格行
}

} // namespace edm