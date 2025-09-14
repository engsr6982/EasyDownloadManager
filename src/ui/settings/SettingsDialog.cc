#include "SettingsDialog.h"
#include "config/EdmGlobalConfig.h"
#include "ui_SettingsDialog.h"
#include "utils/EnumUtils.h"
#include "utils/StringUtils.h"

#include <magic_enum/magic_enum.hpp>

namespace edm {


SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::SettingsDialog) {
    ui->setupUi(this);
    initWidgets(); // 初始化控件

    connect(
        ui->proxyTypeComboBox_,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this,
        &SettingsDialog::onSwitchProxyType
    );
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
    ui->proxyHostInput_->setText(EdmGlobalConfig::kDefaultProxyHost);
    ui->proxyPortSpinBox_->setValue(EdmGlobalConfig::kDefaultProxyPort);
    syncWidgetStateFromConfig();
}

void SettingsDialog::syncWidgetStateFromConfig() const {
    auto const& config = EdmGlobalConfig::instance();

    // basic
    ui->threadCountComboBox_->setCurrentIndex(
        enum_utils::enum_value_index<EdmGlobalConfig::AvailableThreads>(config.getThreadCount()).value_or(0)
    );
    ui->bandWidthLimitInput_->setText(string_utils::string2qstring(std::to_string(config.getBandwidthLimit())));
    ui->userAgentInput_->setText(config.getUserAgent());
    ui->saveDirInput_->setText(config.getSaveDir());
    ui->tempDirInput_->setText(config.getTempDir());
    ui->autoStartCheckBox_->setChecked(config.canAutoStart());
    ui->showCompleteCheckBox_->setChecked(config.canShowDownloadCompleteDialog());

    // proxy
    auto proxy = config.getProxyConfig();
    ui->proxyTypeComboBox_->setCurrentIndex(enum_utils::enum_index(proxy.type_).value_or(0));
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
    // TODO: impl
}
void SettingsDialog::switchTabToFirst() const { ui->tabWidget_->setCurrentIndex(0); }

void SettingsDialog::onSwitchProxyType(int index) const {
    auto type = enum_utils::enum_from_index<EdmGlobalConfig::ProxyType>(index);
    setProxySubWidgetEnabled(type != EdmGlobalConfig::ProxyType::None);
}


} // namespace edm