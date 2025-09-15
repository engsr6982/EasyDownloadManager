#include "SettingsDialog.h"
#include "config/EdmGlobalConfig.h"
#include "ui_SettingsDialog.h"
#include "utils/EnumUtils.h"
#include "utils/StringUtils.h"

#include <magic_enum/magic_enum.hpp>
#include <qdir.h>
#include <qmessagebox.h>

namespace edm {


SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::SettingsDialog) {
    ui->setupUi(this);
    initWidgets(); // 初始化控件

    connect(
        ui->proxyTypeComboBox_,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this,
        &SettingsDialog::onProxyTypeSwitched
    );

    connect(ui->userAgentResetBtn_, &QToolButton::clicked, this, &SettingsDialog::onResetUserAgentButtonClicked);
}

SettingsDialog::~SettingsDialog() { delete ui; }

void SettingsDialog::showEvent(QShowEvent* event) {
    switchTabToFirst();
    syncWidgetStateFromConfig(); // 从配置同步控件状态
    QDialog::showEvent(event);
}
void SettingsDialog::setProxySubWidgetEnabled(bool e) const {
    ui->proxyHostInput_->setEnabled(e);
    ui->proxyPortSpinBox_->setEnabled(e);
    ui->proxyUserInput_->setEnabled(e);
    ui->proxyPwdInput_->setEnabled(e);
}

void SettingsDialog::initWidgets() {
    for (auto val : magic_enum::enum_values<EdmGlobalConfig::AvailableThreads>()) {
        ui->threadCountComboBox_->addItem(string_utils::string2qstring(std::to_string(static_cast<int>(val))));
    }
    for (auto type : magic_enum::enum_names<EdmGlobalConfig::ProxyType>()) {
        ui->proxyTypeComboBox_->addItem(string_utils::stringview2qstring(type));
    }
    ui->bandWidthLimitInput_->setValidator(new QIntValidator(0, INT_MAX, this));
    ui->proxyHostInput_->setText(EdmGlobalConfig::kDefaultProxyHost);
    ui->proxyPortSpinBox_->setValue(EdmGlobalConfig::kDefaultProxyPort);
    syncWidgetStateFromConfig();
}

void SettingsDialog::syncWidgetStateFromConfig() const {
    auto const& config = EdmGlobalConfig::instance();

    // basic
    ui->threadCountComboBox_->setCurrentIndex(
        enum_utils::getIndexFromRawValue<EdmGlobalConfig::AvailableThreads>(config.getThreadCount()).value_or(0)
    );
    ui->bandWidthLimitInput_->setText(string_utils::string2qstring(std::to_string(config.getBandwidthLimit())));
    ui->userAgentInput_->setText(config.getUserAgent());
    ui->saveDirInput_->setText(config.getSaveDir());
    ui->tempDirInput_->setText(config.getTempDir());
    ui->autoStartCheckBox_->setChecked(config.canAutoStart());
    ui->showCompleteCheckBox_->setChecked(config.canShowDownloadCompleteDialog());

    // proxy
    auto proxy = config.getProxyConfig();
    ui->proxyTypeComboBox_->setCurrentIndex(enum_utils::getIndexFromEnumValue(proxy.type_).value_or(0));
    if (proxy.type_ == EdmGlobalConfig::ProxyType::None) {
        setProxySubWidgetEnabled(false); // 未启用代理，禁用控件
        return;
    }
    setProxySubWidgetEnabled(true); // 注意：type 不为 none 时，值不应该为 nullopt
    ui->proxyHostInput_->setText(proxy.host_.value());
    ui->proxyPortSpinBox_->setValue(proxy.port_.value());
    ui->proxyUserInput_->setText(proxy.user_.value());
    ui->proxyPwdInput_->setText(proxy.password_.value());
}
void SettingsDialog::saveWidgetStateToConfig() {
    auto& config = EdmGlobalConfig::instance();

    config.setThreadCount(
        enum_utils::getRawValueFromIndex<EdmGlobalConfig::AvailableThreads>(ui->threadCountComboBox_->currentIndex())
    );

    {
        auto limit = std::stoi(string_utils::qstring2string(ui->bandWidthLimitInput_->text()));
        qDebug() << "[SettingsDialog] bandwidth limit (stoi):" << limit;
        config.setBandwidthLimit(limit);
    }
    config.setUserAgent(ui->userAgentInput_->text());

    {
        QDir dir{};
        auto saveDir = ui->saveDirInput_->text();
        if (!dir.exists(saveDir)) {
            QMessageBox::warning(this, "Error", "保存目录不存在");
            return;
        }
        config.setSaveDir(saveDir);

        auto tempDir = ui->tempDirInput_->text();
        if (!dir.exists(tempDir)) {
            QMessageBox::warning(this, "Error", "临时目录不存在");
            return;
        }
        config.setTempDir(tempDir);
    }
    config.setAutoStart(ui->autoStartCheckBox_->isChecked());
    config.setShowDownloadCompleteDialog(ui->showCompleteCheckBox_->isChecked());

    // proxy
    {
        EdmGlobalConfig::ProxyConfig cfg{};
        cfg.type_ = enum_utils::getEnumFromIndex<EdmGlobalConfig::ProxyType>(ui->proxyTypeComboBox_->currentIndex());
        cfg.host_ = ui->proxyHostInput_->text();
        cfg.port_ = ui->proxyPortSpinBox_->value();
        cfg.user_ = ui->proxyUserInput_->text();
        cfg.password_ = ui->proxyPwdInput_->text();
        config.setProxyConfig(cfg);
    }
}
void SettingsDialog::switchTabToFirst() const { ui->tabWidget_->setCurrentIndex(0); }

void SettingsDialog::accept() {
    qDebug() << "[SettingsDialog] accept";
    saveWidgetStateToConfig();
    QDialog::accept();
}

void SettingsDialog::reject() {
    qDebug() << "[SettingsDialog] reject";
    QDialog::reject();
}

void SettingsDialog::onProxyTypeSwitched(int index) const {
    auto type = enum_utils::getEnumFromIndex<EdmGlobalConfig::ProxyType>(index);
    setProxySubWidgetEnabled(type != EdmGlobalConfig::ProxyType::None);
}
void SettingsDialog::onResetUserAgentButtonClicked() const {
    ui->userAgentInput_->setText(EdmGlobalConfig::kDefaultUserAgent);
}


} // namespace edm