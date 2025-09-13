#pragma once
#include <QDialog>

namespace Ui {
class NewTaskDialog;
}

namespace edm {

class NewTaskDialog : public QDialog {
    Q_OBJECT

public:
    explicit NewTaskDialog(QWidget* parent = nullptr);
    ~NewTaskDialog();

private slots:
    void on_pickDirButton__clicked();

    void on_dialogButtonBox__accepted();

    void on_dialogButtonBox__rejected();

private:
    Ui::NewTaskDialog* ui;
};

} // namespace edm
