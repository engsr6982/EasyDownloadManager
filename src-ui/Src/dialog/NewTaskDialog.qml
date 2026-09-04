import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../fluent"
import ".."

import edm.qml.ui 1.0

FluPopup {
    id: root

    width: 360
    height: 260
    closePolicy: Popup.CloseOnEscape // 仅 ESC 关闭

    padding: Constants.paddingStandard

    onOpened: {
        urlInput.forceActiveFocus(); // 自动聚焦输入框

        // Auto-paste URL from clipboard if it looks like http/https
        var clip = AppController.clipboardText().trim();
        var httpRegex = /^https?:\/\/.+/i;
        if (httpRegex.test(clip)) {
            urlInput.text = clip;
            urlInput.selectAll();
        }

        // force sync settings
        downloadDir.text = SettingsModel.saveDir;
        proxySw.checked = SettingsModel.proxyEnabled;
        proxySw.enabled = SettingsModel.proxyEnabled;
    }
    onClosed: {
        urlInput.text = ""; // Clear it immediately to avoid caching super long strings
        downloadDir.text = SettingsModel.saveDir;
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Constants.itemSpacing

        // 标题栏
        RowLayout {
            spacing: 8
            FluIndicator {
                active: true
                animate: false
            }
            FluText {
                isTitle: true
                text: qsTr("新建下载任务")
                font.bold: true
            }
        }

        // URL
        FluText {
            isSecondary: true
            text: qsTr("粘贴链接地址 (HTTP, HTTPS):")
        }
        ScrollView {
            id: urlScrollView
            Layout.fillWidth: true
            implicitHeight: 80
            clip: true

            FluTextArea {
                id: urlInput
                placeholderText: qsTr("输入下载链接，仅支持一条链接")
            }
        }

        // 存储目录
        FluText {
            text: qsTr("存储目录:")
            isSecondary: true
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Constants.itemSpacing

            FluTextField {
                id: downloadDir
                text: "<download dir>"
                Layout.fillWidth: true
            }
            FluIconButton {
                text: "\uF3DD"
                onClicked: downloadDirPicker.open()
            }
            FolderDialog {
                id: downloadDirPicker
                onAccepted: {
                    // sync to downloadDir input
                    var path = selectedFolder;
                    var localUrl = AppController.removeFileUrlPrefix(path);
                    downloadDir.text = localUrl;
                }
            }
        }

        Item {
            Layout.fillHeight: true
        }

        // 底部操作栏
        Item {
            id: bottomBar
            Layout.fillWidth: true
            implicitHeight: 32

            // 左对齐代理隧道
            Row {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                FluSwitch {
                    id: proxySw
                    showText: false
                    checked: false
                    checkable: true
                }
                FluText {
                    text: qsTr("使用代理下载")
                    anchors.verticalCenter: proxySw.verticalCenter
                }
            }

            // 右对齐操作控件
            Row {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                layoutDirection: Qt.RightToLeft
                spacing: Constants.itemSpacing

                FluButton {
                    text: qsTr("下载")
                    isAccent: true
                    onClicked: {
                        var url = urlInput.text.trim();
                        if (url === "") {
                            return;
                        }
                        var ok = TaskListModel.createTask(url, downloadDir.text, proxySw.checked);
                        if (ok) {
                            root.close();
                        } else {
                            // TODO: show error toast/dialog
                            console.log("创建任务失败");
                        }
                    }
                }
                FluButton {
                    text: qsTr("取消")
                    onClicked: root.close()
                }
            }
        }
    }
}
