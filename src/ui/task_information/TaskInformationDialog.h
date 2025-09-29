#pragma once

#include <QDialog>


namespace edm {
struct TaskModel;
}
namespace Ui {
class TaskInformationDialog;
}

namespace edm {

class TaskInformationDialog : public QDialog {
    Q_OBJECT

public:
    explicit TaskInformationDialog(TaskModel const& model, QWidget* parent = nullptr);
    ~TaskInformationDialog();

private:
    Ui::TaskInformationDialog* ui;
};

} // namespace edm