#pragma once
#include <QMainWindow>
#include <QTimer>
#include <memory>
#include <unordered_map>

namespace edm {
class SettingsDialog;
}
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QStandardItemModel;

namespace edm {

namespace downloader {
class DownloadState;
}

struct TaskModel;
struct TaskContext;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void hideFileTree() const;
    void showFileTree() const;

    void initDataFromDB();

    void insertTask(std::shared_ptr<edm::TaskModel> task);

private slots:
    void handleTaskCreated(std::shared_ptr<edm::TaskContext> ctx);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void _setupLayout();
    void _buildToolBar();
    void _buildFileTree();
    void _buildTaskList();

    Ui::MainWindow*                                                     ui_{nullptr};
    SettingsDialog*                                                     settingsDialog_{nullptr};
    QTimer*                                                             uiUpdateTimer_{nullptr};
    std::unordered_map<int, std::shared_ptr<downloader::DownloadState>> taskStates_;
};

} // namespace edm
