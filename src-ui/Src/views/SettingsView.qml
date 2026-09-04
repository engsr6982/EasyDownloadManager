import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Dialogs
import "../fluent"
import ".."

import edm.qml.ui 1.0

Item {
    id: root
    anchors.fill: parent

    Component.onCompleted: {
        SettingsModel.reload();
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ScrollBar.vertical: ScrollBar {
            id: vbar
            policy: ScrollBar.AsNeeded
            width: hovered || pressed ? 6 : 2
            anchors.right: parent.right
            anchors.rightMargin: 2
            contentItem: Rectangle {
                radius: width / 2
                color: (vbar.hovered || vbar.pressed) ? Constants.textSecondary : Constants.borderSubtle
            }
        }

        ColumnLayout {
            width: parent.width
            spacing: Constants.itemSpacing

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: Constants.viewTitlebarHeight

                Text {
                    text: qsTr("下载设置")
                    font.family: Constants.fontFamily
                    font.pixelSize: Constants.fontSizeTitle
                    font.bold: true
                    color: Constants.textPrimary
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            // 自启动
            FluSettingRow {
                iconText: "\uE713"
                title: qsTr("自启动")
                description: qsTr("开机后自动启动下载器")

                FluSwitch {
                    checked: SettingsModel.autoStart
                    onClicked: SettingsModel.autoStart = checked
                }
            }

            // 状态通知
            FluSettingRow {
                iconText: "\uE015"
                title: qsTr("状态通知")
                description: qsTr("当任务失败或完成时，弹出系统通知")

                FluSwitch {
                    checked: SettingsModel.showStatusNotification
                    onClicked: SettingsModel.showStatusNotification = checked
                }
            }

            // 下载路径
            FluSettingRow {
                id: saveDirRow
                iconText: "\uE57C"
                title: qsTr("下载路径")
                description: SettingsModel.saveDir

                FolderDialog {
                    id: folderPick
                    title: qsTr("请选择文件保存路径")
                    onAccepted: {
                        var path = selectedFolder;
                        var localUrl = AppController.removeFileUrlPrefix(path);
                        SettingsModel.saveDir = localUrl;
                    }
                }

                FluIconButton {
                    text: "\uF3DD" // edit pen
                    onClicked: folderPick.open()
                }
                FluIconButton {
                    text: "\uF419" // open folder
                    onClicked: AppController.openFolder(SettingsModel.saveDir)
                }
            }

            // 线程数量
            FluSettingRow {
                iconText: "\u{F0235}"
                title: qsTr("线程数量")
                description: qsTr("单个任务的线程数量(连接数), 默认为 8")

                FluComboBox {
                    model: SettingsModel.threadCounts
                    currentIndex: SettingsModel.threadCountIndex
                    onActivated: function (index) {
                        SettingsModel.threadCountIndex = index;
                    }
                }
            }

            // 带宽限制
            FluSettingRow {
                iconText: "\uF831"
                title: qsTr("带宽限制")
                description: qsTr("限制单个任务的下载速度, 默认为 0 (无限制), 单位 kb")

                Row {
                    spacing: Constants.itemSpacing

                    FluTextField {
                        id: bandLimit
                        width: 140
                        text: SettingsModel.bandwidthLimit.toString()
                        onEditingFinished: SettingsModel.bandwidthLimit = parseInt(text) || 0
                    }
                    FluIconButton {
                        text: "\uF1A0" // 重置
                        onClicked: {
                            bandLimit.text = "0";
                            SettingsModel.bandwidthLimit = 0;
                        }
                    }
                }
            }

            // User Agent
            FluSettingRow {
                iconText: "\u{F0132}"
                title: qsTr("User-Agent")
                description: qsTr("更改下载器使用的 User-Agent")

                Row {
                    spacing: Constants.itemSpacing

                    FluTextField {
                        id: userAgentField
                        width: 140
                        text: SettingsModel.userAgent
                        onEditingFinished: SettingsModel.userAgent = text
                    }
                    FluIconButton {
                        text: "\uF1A0" // 重置
                        onClicked: {
                            userAgentField.text = SettingsModel.defaultUserAgent();
                            SettingsModel.userAgent = userAgentField.text;
                        }
                    }
                }
            }

            // 代理设置
            FluSettingExpander {
                iconText: "\uF45E"
                title: qsTr("代理设置")
                description: qsTr("更改下载器使用的代理服务器")
                expanded: true

                FluSettingRow {
                    isSubRow: true
                    title: qsTr("启用代理")
                    description: qsTr("启用或禁用代理服务器")

                    FluSwitch {
                        id: proxyState
                        checked: SettingsModel.proxyEnabled
                        onClicked: SettingsModel.proxyEnabled = checked
                    }
                }

                FluSettingRow {
                    isSubRow: true
                    title: qsTr("代理类型")
                    description: qsTr("选择目标服务器支持的代理协议")

                    FluComboBox {
                        id: proxyProtocolType
                        model: SettingsModel.proxyTypes
                        currentIndex: SettingsModel.proxyTypeIndex > 0 ? SettingsModel.proxyTypeIndex - 1 : 0
                        enabled: proxyState.checked
                        onActivated: function (index) {
                            SettingsModel.proxyTypeIndex = index + 1; // offset: 0=None not in combo
                        }
                    }
                }

                FluSettingRow {
                    isSubRow: true
                    title: qsTr("主机地址")
                    description: qsTr("代理服务器的主机地址")

                    Row {
                        spacing: Constants.itemSpacing

                        FluTextField {
                            width: 120
                            text: SettingsModel.proxyHost
                            placeholderText: "localhost"
                            enabled: proxyState.checked
                            onEditingFinished: SettingsModel.proxyHost = text
                        }
                        FluText {
                            text: ":"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        FluTextField {
                            width: 60
                            text: SettingsModel.proxyPort.toString()
                            placeholderText: "7890"
                            enabled: proxyState.checked
                            validator: IntValidator {
                                bottom: 0
                                top: 65535
                            }
                            onEditingFinished: SettingsModel.proxyPort = parseInt(text) || 0
                        }
                    }
                }

                FluSettingRow {
                    isSubRow: true
                    title: qsTr("用户名")
                    description: qsTr("登录代理服务器时使用的用户名")

                    FluTextField {
                        text: SettingsModel.proxyUser
                        enabled: proxyState.checked && proxyProtocolType.currentIndex !== 2 && proxyProtocolType.currentIndex !== 3
                        onEditingFinished: SettingsModel.proxyUser = text
                    }
                }

                FluSettingRow {
                    isSubRow: true
                    title: qsTr("密码")
                    description: qsTr("登录代理服务器时使用的密码")

                    FluTextField {
                        isPassword: true
                        text: SettingsModel.proxyPassword
                        enabled: proxyState.checked && proxyProtocolType.currentIndex !== 2 && proxyProtocolType.currentIndex !== 3
                        onEditingFinished: SettingsModel.proxyPassword = text
                    }
                }
            }

            // 其他
            FluSettingGroup {
                groupTitle: "关于"

                FluSettingExpander {
                    iconText: "\uF4A4"
                    title: Constants.projectName
                    description: qsTr("关于此项目和第三方代码库信息")

                    FluSettingRow {
                        isSubRow: true
                        title: qsTr("链接")

                        // 两列布局
                        Row {
                            Column {
                                Text {
                                    text: qsTr("项目地址")
                                }
                            }
                            Column {
                                Text {
                                    text: "aa"
                                }
                            }
                        }
                    }

                    Row {
                        Layout.fillWidth: true

                        Column {
                            Text {
                                text: qsTr("开源代码库")
                            }
                        }
                        Column {
                            Text {
                                text: "Qt6"
                            }
                        }
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 24
            }
        }
    }
}
