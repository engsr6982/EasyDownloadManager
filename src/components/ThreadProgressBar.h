#pragma once
#include <QWidget>

namespace edm::components {

struct DownloadRange {
    qint64              start{0};
    qint64              end{0};
    std::atomic<qint64> downloaded{0};

    DownloadRange(qint64 s, qint64 e) : start(s), end(e), downloaded(0) {}
};

class ThreadProgressBar : public QWidget {
public:
    explicit ThreadProgressBar(QWidget* parent = nullptr);
    ~ThreadProgressBar() override;

    // 设置所有线程分段
    void setRanges(std::vector<std::shared_ptr<DownloadRange>> const& ranges);

    // 启动/停止UI刷新
    void startUpdating();
    void stopUpdating();

protected:
    void paintEvent(QPaintEvent*) override;

private:
    std::vector<std::shared_ptr<DownloadRange>> ranges_;
    QTimer*                                     updateTimer_; // 用于定时刷新UI
};

} // namespace edm::components
