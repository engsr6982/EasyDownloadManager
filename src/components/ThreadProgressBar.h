#pragma once
#include <QMutex>
#include <QWidget>

namespace edm::components {

class ThreadProgressBar : public QWidget {
public:
    struct Range {
        qint64 start;
        qint64 end;
        qint64 downloaded;
    };

    explicit ThreadProgressBar(QWidget* parent = nullptr);
    ~ThreadProgressBar() override;

    // 设置所有线程分段
    void setRanges(QVector<Range> const& ranges);

    // 更新单个线程分段进度（动态更新）
    void updateThread(int index, qint64 downloaded);

    // 动态添加新的线程分段
    void addThread(Range const& range);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QVector<Range> ranges_;
    QMutex         mutex_;
};

} // namespace edm::components
