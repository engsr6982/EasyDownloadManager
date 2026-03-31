#include "SettingsDialog.h"
#include "EdmConfig.h"
#include "ui_SettingsDialog.h"
#include "utils/EnumUtils.h"

#include <magic_enum/magic_enum.hpp>
#include <qdir.h>
#include <qfiledialog.h>
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

    connect(ui->saveDirChooseBtn_, &QToolButton::clicked, this, [this]() { chooseDir(ui->saveDirInput_); });
    connect(ui->tempDirChooseBtn_, &QToolButton::clicked, this, [this]() { chooseDir(ui->tempDirInput_); });
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
    setUserAndPasswordWidgetEnabled(e);
}
void SettingsDialog::setUserAndPasswordWidgetEnabled(bool e) const {
    ui->proxyUserInput_->setEnabled(e);
    ui->proxyPwdInput_->setEnabled(e);
}
void SettingsDialog::chooseDir(QLineEdit* input) {
    auto legacy = input->text();

    QString dir{};
    if (QDir{}.exists(legacy)) {
        dir = QFileDialog::getExistingDirectory(this, "选择文件夹", legacy);
    } else {
        dir = QFileDialog::getExistingDirectory(this);
    }

    if (!dir.isEmpty()) {
        input->setText(dir);
    }
}

void SettingsDialog::initWidgets() {
    for (auto val : magic_enum::enum_values<AvailableThreads>()) {
        ui->threadCountComboBox_->addItem(QString::fromStdString(std::to_string(static_cast<int>(val))));
    }
    for (auto type : magic_enum::enum_names<ProxyType>()) {
        ui->proxyTypeComboBox_->addItem(QString::fromStdString(type.data()));
    }
    ui->bandWidthLimitInput_->setValidator(new QIntValidator(0, INT64_MAX, this));
    ui->proxyHostInput_->setText(GlobalDefaults::kDefaultProxyHost);
    ui->proxyPortSpinBox_->setValue(GlobalDefaults::kDefaultProxyPort);
    syncWidgetStateFromConfig();
}

void SettingsDialog::syncWidgetStateFromConfig() const {
    auto const& config = EdmConfig::getInstance();

    // basic
    ui->threadCountComboBox_->setCurrentIndex(
        enum_utils::getIndexFromRawValue<AvailableThreads>(config.getThreadCount()).value_or(0)
    );
    ui->bandWidthLimitInput_->setText(QString::fromStdString(std::to_string(config.getBandwidthLimit())));
    ui->userAgentInput_->setText(config.getUserAgent());
    ui->saveDirInput_->setText(config.getSaveDir());
    ui->tempDirInput_->setText(config.getTempDir());
    ui->autoStartCheckBox_->setChecked(config.canAutoStart());
    ui->showCompleteCheckBox_->setChecked(config.canShowDownloadCompleteDialog());

    // proxy
    auto proxy = config.getProxyConfig();
    qDebug() << "[SettingsDialog] loading proxy.type raw:" << static_cast<int>(proxy.type_);
    ui->proxyTypeComboBox_->setCurrentIndex(enum_utils::getIndexFromEnumValue(proxy.type_).value_or(0));
    if (proxy.type_ == ProxyType::None) {
        setProxySubWidgetEnabled(false); // 未启用代理，禁用控件
        return;
    }
    setProxySubWidgetEnabled(true); // 注意：type 不为 none 时，值不应该为 nullopt
    ui->proxyHostInput_->setText(proxy.host_.value());
    ui->proxyPortSpinBox_->setValue(proxy.port_.value());
    ui->proxyUserInput_->setText(proxy.user_.value_or(""));
    ui->proxyPwdInput_->setText(proxy.password_.value_or(""));
    if (proxy.isSocks4Series()) {
        setUserAndPasswordWidgetEnabled(false); // 特殊情况：socks4、socks4a 不支持账户密码
    }
}
void SettingsDialog::saveWidgetStateToConfig() {
    auto& config = EdmConfig::getInstance();

    config.setThreadCount(enum_utils::getRawValueFromIndex<AvailableThreads>(ui->threadCountComboBox_->currentIndex()));

    {
        auto limit = std::stoi(ui->bandWidthLimitInput_->text().toStdString());
        qDebug() << "[SettingsDialog] saving bandwidth limit (stoi):" << limit;
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
        ProxyConfig cfg{};
        cfg.type_     = enum_utils::getEnumFromIndex<ProxyType>(ui->proxyTypeComboBox_->currentIndex());
        cfg.host_     = ui->proxyHostInput_->text();
        cfg.port_     = ui->proxyPortSpinBox_->value();
        cfg.user_     = ui->proxyUserInput_->text();
        cfg.password_ = ui->proxyPwdInput_->text();
        qDebug() << "[SettingsDialog] saving proxy.type:" << static_cast<int>(cfg.type_);
        qDebug() << "[SettingsDialog] saving proxy.host:" << cfg.host_;
        qDebug() << "[SettingsDialog] saving proxy.port:" << cfg.port_;
        qDebug() << "[SettingsDialog] saving proxy.user:" << cfg.user_;
        qDebug() << "[SettingsDialog] saving proxy.password:" << cfg.password_;
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
    auto type = enum_utils::getEnumFromIndex<ProxyType>(index);
    setProxySubWidgetEnabled(type != ProxyType::None);

    if (type == ProxyType::Socks4 || type == ProxyType::Socks4a) {
        setUserAndPasswordWidgetEnabled(false); // 特殊情况：socks4、socks4a 不支持账户密码
    }
}
void SettingsDialog::onResetUserAgentButtonClicked() const {
    ui->userAgentInput_->setText(GlobalDefaults::kDefaultUserAgent);
}


} // namespace edm