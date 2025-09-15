#pragma once

#include <QDialog>


class QLineEdit;
namespace Ui {
class SettingsDialog;
}


namespace edm {

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog();

    void showEvent(QShowEvent* event) override;

    void setProxySubWidgetEnabled(bool e) const;

    void chooseDir(QLineEdit* input);

private:
    void initWidgets();

    void syncWidgetStateFromConfig() const;

    void saveWidgetStateToConfig();

    void switchTabToFirst() const;

    void accept() override;
    void reject() override;

private slots:
    void onProxyTypeSwitched(int index) const;

    void onResetUserAgentButtonClicked() const;

private:
    Ui::SettingsDialog* ui;
};

} // namespace edm