#include "ThreadProgressBar.h"
#include <QMutexLocker>
#include <QPainter>

namespace edm::components {

ThreadProgressBar::ThreadProgressBar(QWidget* parent) : QWidget(parent) { setMinimumHeight(20); }

ThreadProgressBar::~ThreadProgressBar() = default;

void ThreadProgressBar::setRanges(QVector<Range> const& ranges) {
    QMutexLocker locker(&mutex_);
    ranges_ = ranges;
    update();
}

void ThreadProgressBar::updateThread(int index, qint64 downloaded) {
    QMutexLocker locker(&mutex_);
    if (index >= 0 && index < ranges_.size()) {
        ranges_[index].downloaded = downloaded;
        update();
    }
}

void ThreadProgressBar::addThread(Range const& range) {
    QMutexLocker locker(&mutex_);
    ranges_.append(range);
    update();
}

void ThreadProgressBar::paintEvent(QPaintEvent* paintEvent) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QMutexLocker locker(&mutex_);

    if (ranges_.isEmpty()) return;

    int w = width();
    int h = height();

    // 计算总范围
    qint64 totalStart = ranges_.first().start;
    qint64 totalEnd   = ranges_.first().end;
    for (const auto& r : ranges_) {
        if (r.start < totalStart) totalStart = r.start;
        if (r.end > totalEnd) totalEnd = r.end;
    }
    qint64 totalSize = totalEnd - totalStart;

    for (const auto& r : ranges_) {
        double xStart = double(r.start - totalStart) / totalSize * w;
        double xEnd   = double(r.end - totalStart) / totalSize * w;

        // 绘制背景条
        QRectF bgRect(xStart, 0, xEnd - xStart, h);
        p.setBrush(QColor(200, 200, 200));
        p.setPen(Qt::NoPen);
        p.drawRect(bgRect);

        // 绘制下载进度
        double progressRatio = (r.end > r.start) ? double(r.downloaded) / (r.end - r.start) : 0.0;
        QRectF fgRect(xStart, 0, (xEnd - xStart) * progressRatio, h);
        p.setBrush(QColor(100, 200, 100));
        p.drawRect(fgRect);
    }
}


} // namespace edm::components