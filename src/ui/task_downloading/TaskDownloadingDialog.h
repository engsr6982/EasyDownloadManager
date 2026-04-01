#pragma once

#include <QDialog>

namespace Ui {
class TaskDownloadingDialog;
}

namespace edm {

class TaskDownloadingDialog : public QDialog {
    Q_OBJECT

public:
    explicit TaskDownloadingDialog(QWidget* parent = nullptr);
    ~TaskDownloadingDialog();

private:
    Ui::TaskDownloadingDialog* ui;
};

} // namespace edm
