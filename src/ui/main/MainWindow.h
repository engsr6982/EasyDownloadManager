#pragma once

#include <QMainWindow>


QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QStandardItemModel;

namespace edm {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    void hideFileTree() const;
    void showFileTree() const;

private:
    void _addDebugDatas() const;
    void _setupLayout();
    void _buildToolBar() const;
    void _buildFileTree();
    void _buildTaskList();

    Ui::MainWindow* ui_{nullptr};
};

} // namespace edm