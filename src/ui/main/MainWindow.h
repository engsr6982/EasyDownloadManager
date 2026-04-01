#pragma once

#include <QMainWindow>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QStandardItemModel;

namespace edm {

struct TaskModel;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    void hideFileTree() const;
    void showFileTree() const;

    void initDataFromDB();

    void insertTask(TaskModel const& task);

private slots:
    void handleTaskAddedToDatabase(edm::TaskModel const& task);

protected:
    void closeEvent(QCloseEvent* event) override;

    void onRequestOpenTaskInfoDialog(int row);

private:
    void _setupLayout();
    void _buildToolBar() const;
    void _buildFileTree();
    void _buildTaskList();

    Ui::MainWindow* ui_{nullptr};
    QTimer*         uiUpdateTimer_{nullptr};
};

} // namespace edm