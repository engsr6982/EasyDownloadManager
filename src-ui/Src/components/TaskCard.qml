import QtQuick
import QtQuick.Layouts
import ".."
import "../fluent"

import edm.qml.ui 1.0

Item {
    id: cardRoot
    height: 72
    width: parent.width

    property int index
    property int taskId: 0
    property string fileName: "<filename>"
    property string fileSizeStr: "0.00 MB"
    property real progressValue: 0.0
    property string speedStr: "N/A"
    property string statusStr: "N/A"
    property string urlStr
    property string saveDir
    property string referrer
    property string timestamp

    signal cardClicked(int taskId)
    signal pauseClicked(int taskId)
    signal deleteClicked(int taskId)
    signal rightClicked(int taskId, Item targetItem, real mouseX, real mouseY)

    Rectangle {
        id: delegateCard
        anchors.fill: parent
        color: Constants.bgContent
        radius: Constants.radiusCard
        border.width: 1
        border.color: delegateMouseArea.containsMouse ? Constants.accentPrimary : Constants.borderCard

        Behavior on border.color {
            ColorAnimation {
                duration: 150
            }
        }

        MouseArea {
            id: delegateMouseArea
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: function (mouse) {
                if (mouse.button === Qt.RightButton) {
                    cardRoot.rightClicked(cardRoot.taskId, cardRoot, mouse.x, mouse.y);
                } else if (mouse.button === Qt.LeftButton) {
                    cardRoot.cardClicked(cardRoot.taskId);
                }
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: Constants.paddingStandard
            spacing: Constants.marginStandard

            // 左侧信息区
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                // 文件名
                Text {
                    // text: cardRoot.fileName ?? "<filename>"
                    text: {
                        var rawName = cardRoot.fileName ?? "<filename>";

                        var keepEndLength = 8; // 保留的末尾字符长度 .tar.gz / .7z
                        var maxLen = 50; // 最大长度，超出触发截断

                        if (rawName.length > maxLen) {
                            var startPart = rawName.substring(0, maxLen - keepEndLength - 3); // -3预留...长度
                            var endPart = rawName.substring(rawName.length - keepEndLength);
                            return startPart + "..." + endPart;
                        } else {
                            return rawName;
                        }
                    }
                    font.family: Constants.fontFamily
                    font.pixelSize: Constants.fontSizeBody
                    font.bold: true
                    color: Constants.textPrimary
                    // elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }

                // 进度条
                FluProgressBar {
                    Layout.fillWidth: true
                    value: cardRoot.progressValue
                    indeterminate: cardRoot.statusStr === "Pending"
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Constants.itemSpacing

                    // 文件大小
                    Text {
                        text: cardRoot.fileSizeStr ?? "0.00 MB"
                        font.family: Constants.fontFamily
                        font.pixelSize: Constants.fontSizeSecondary
                        color: Constants.textSecondary
                    }

                    // 分割竖线
                    Rectangle {
                        Layout.preferredWidth: 1
                        Layout.preferredHeight: 8
                        color: Constants.borderSubtle
                    }

                    // 状态
                    Text {
                        text: {
                            var s = cardRoot.statusStr ?? "N/A";
                            // C++ sends "Finished" — map locally for display readability
                            if (s === "Finished")
                                return qsTr("已完成");
                            if (s === "Running")
                                return qsTr("下载中");
                            if (s === "Paused")
                                return qsTr("已暂停");
                            if (s === "Failed")
                                return qsTr("失败");
                            if (s === "Canceled")
                                return qsTr("已取消");
                            if (s === "Pending")
                                return qsTr("等待中");
                            if (s === "Preparing")
                                return qsTr("准备中");
                            return s;
                        }
                        font.family: Constants.fontFamily
                        font.pixelSize: Constants.fontSizeSecondary
                        color: {
                            var s = cardRoot.statusStr;
                            if (s === "Finished")
                                return Constants.success;
                            if (s === "Running")
                                return Constants.accentPrimary;
                            if (s === "Failed")
                                return Constants.danger;
                            return Constants.textSecondary;
                        }
                    }
                }
            }

            // 右侧控制区
            RowLayout {
                spacing: Constants.marginStandard
                Layout.alignment: Qt.AlignVCenter

                // 速度
                Text {
                    text: cardRoot.speedStr ?? "N/A"
                    font.family: Constants.fontFamily
                    font.pixelSize: Constants.fontSizeBody
                    font.bold: true
                    color: Constants.textPrimary
                    Layout.preferredWidth: 80
                    horizontalAlignment: Text.AlignRight
                }

                Row {
                    spacing: 4

                    // 暂停 / 继续
                    FluIconButton {
                        text: {
                            var s = cardRoot.statusStr;
                            // 等待中(Pending)尚未开始 → 显示"继续"；准备中(Preparing)已在跑 → 显示"暂停"
                            if (s === "Running" || s === "Preparing")
                                return "\uf5a2";  // pause
                            return "\uf606"; // play
                        }
                        enabled: {
                            var s = cardRoot.statusStr;
                            return s === "Running" || s === "Paused" || s === "Pending" || s === "Failed"
                                || s === "Preparing";
                        }
                        onClicked: cardRoot.pauseClicked(cardRoot.taskId)
                    }

                    // 删除
                    FluIconButton {
                        text: "\uf34d"
                        iconColor: Constants.danger
                        onClicked: cardRoot.deleteClicked(cardRoot.taskId)
                    }

                    // 更多
                    FluIconButton {
                        text: "\uf557"
                        onClicked: cardRoot.rightClicked(cardRoot.taskId, this, 0, this.height)
                    }
                }
            }
        }
    }
}
