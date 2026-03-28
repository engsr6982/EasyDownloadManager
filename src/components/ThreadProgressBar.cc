#include "ThreadProgressBar.h"

#include <QPainter>
#include <QTimer>

namespace edm::components {

ThreadProgressBar::ThreadProgressBar(QWidget* parent) : QWidget(parent), updateTimer_(new QTimer(this)) {
    setMinimumHeight(20);

    updateTimer_->setInterval(33);
    connect(updateTimer_, &QTimer::timeout, this, [this]() {
        this->update(); // 仅触发重绘，不阻塞
    });
}

ThreadProgressBar::~ThreadProgressBar() { stopUpdating(); }

void ThreadProgressBar::setRanges(std::vector<std::shared_ptr<DownloadRange>> const& ranges) {
    ranges_ = ranges;
    update(); // 初始化时重绘一次
}

void ThreadProgressBar::startUpdating() {
    if (!updateTimer_->isActive()) {
        updateTimer_->start();
    }
}
void ThreadProgressBar::stopUpdating() {
    if (updateTimer_->isActive()) {
        updateTimer_->stop();
    }
    update(); // 停止时确保画出最终状态
}

void ThreadProgressBar::paintEvent(QPaintEvent* paintEvent) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制背景
    QRect rect = this->rect();
    painter.fillRect(rect, QColor(240, 240, 240));

    if (ranges_.empty()) return;

    qint64 totalStart = ranges_.front()->start;
    qint64 totalEnd   = ranges_.back()->end;
    qint64 totalSize  = totalEnd - totalStart;

    // 防止除以 0 的致命错误
    if (totalSize <= 0) return;

    int w = rect.width();
    int h = rect.height();

    // 绘制每个线程的进度块
    painter.setPen(Qt::NoPen);
    for (const auto& r : ranges_) {
        // 原子读取已下载量
        qint64 downloaded = r->downloaded.load(std::memory_order_relaxed);

        // 计算绘制坐标
        double xStart = double(r->start - totalStart) / totalSize * w;
        double width  = double(downloaded) / totalSize * w;

        QRectF blockRect(xStart, 0, width, h);
        painter.fillRect(blockRect, QColor(100, 149, 237)); // 柔和的蓝色
    }

    // 绘制边框
    painter.setPen(QPen(Qt::gray, 1));
    painter.drawRect(0, 0, w - 1, h - 1);
}


} // namespace edm::components