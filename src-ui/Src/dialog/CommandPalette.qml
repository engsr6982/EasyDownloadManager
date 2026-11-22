import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import ".."
import "../fluent"

import edm.qml.ui 1.0

/**
 * 命令面板：窗口级遮罩加居中面板，模糊搜索命令，键盘导航执行。
 * Ctrl+Shift+P 或标题栏按钮呼出，Esc 或点击遮罩关闭。
 */
Item {
    id: paletteRoot
    anchors.fill: parent
    visible: false

    function open() {
        paletteRoot.visible = true;
        CommandRegistry.clearFilter();
        searchBox.clear();
        commandList.currentIndex = 0;
        searchBox.forceActiveFocus();
    }

    function close() {
        paletteRoot.visible = false;
    }

    function toggle() {
        paletteRoot.visible ? paletteRoot.close() : paletteRoot.open();
    }

    // ---- 遮罩 ----
    Rectangle {
        anchors.fill: parent
        color: Constants.dialogOverlay

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            onClicked: paletteRoot.close()
            // 遮罩消费全部滚轮事件，避免滚动穿透到底层视图
            onWheel: (wheel) => wheel.accepted = true
        }
    }

    // ---- 面板（背景与阴影分离，MultiEffect 仅 source 纯背景矩形）----
    Item {
        id: panel
        width: Math.min(600, paletteRoot.width - 64)
        height: Math.min(440, paletteRoot.height - 96)
        x: (paletteRoot.width - width) / 2
        y: Math.max(40, (paletteRoot.height - height) / 3)

        Rectangle {
            id: bgPlate
            anchors.fill: parent
            radius: Constants.radiusCard
            color: Constants.bgContent
            border.width: 1
            border.color: Constants.borderCard
        }

        MultiEffect {
            source: bgPlate
            anchors.fill: bgPlate
            autoPaddingEnabled: true
            shadowEnabled: true
            shadowColor: Constants.bgShadowColor
            shadowBlur: Constants.bgShadowBlur
            shadowVerticalOffset: Constants.bgShadowVerticalOffset
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Constants.paddingStandard
            spacing: Constants.itemSpacing

            FluTextField {
                id: searchBox
                Layout.fillWidth: true
                placeholderText: qsTr("输入命令名称或描述")

                Keys.onUpPressed: commandList.decrementCurrentIndexWrap()
                Keys.onDownPressed: commandList.incrementCurrentIndexWrap()
                Keys.onReturnPressed: commandList.executeCurrent()
                Keys.onEnterPressed: commandList.executeCurrent()
                // 拦截 Tab/Shift+Tab，焦点不离开命令面板
                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Tab || event.key === Qt.Key_Backtab) event.accepted = true;
                }

                onTextChanged: {
                    CommandRegistry.setFilter(text);
                    commandList.currentIndex = 0;
                }
            }

            ListView {
                id: commandList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: CommandRegistry
                spacing: 2
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar {}

                function incrementCurrentIndexWrap() {
                    if (count === 0) return;
                    currentIndex = currentIndex + 1 >= count ? 0 : currentIndex + 1;
                }
                function decrementCurrentIndexWrap() {
                    if (count === 0) return;
                    currentIndex = currentIndex - 1 < 0 ? count - 1 : currentIndex - 1;
                }
                function executeCurrent() {
                    if (count === 0) return;
                    CommandRegistry.executeRow(currentIndex);
                    paletteRoot.close();
                }

                onCountChanged: {
                    if (currentIndex >= count) currentIndex = Math.max(0, count - 1);
                }

                delegate: Item {
                    required property int index
                    required property string title
                    required property string description
                    required property string source

                    width: commandList.width
                    height: 46

                    Rectangle {
                        id: itemBg
                        anchors.fill: parent
                        radius: Constants.radiusControl
                        // 键盘选中（accent 条）与指针 hover 为两个独立状态，hover 不改写选区
                        color: commandList.currentIndex === index
                               ? Constants.standardHover
                               : (itemArea.containsMouse ? Constants.windowSecondaryBase : "transparent")
                        border.width: commandList.currentIndex === index ? 1 : 0
                        border.color: Constants.borderCard

                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.leftMargin: 2
                        anchors.verticalCenter: parent.verticalCenter
                        width: 3
                        height: 20
                        radius: 2
                        color: Constants.accentPrimary
                        visible: commandList.currentIndex === index
                    }

                    MouseArea {
                        id: itemArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            CommandRegistry.executeRow(index);
                            paletteRoot.close();
                        }
                    }

                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: Constants.paddingStandard
                        anchors.rightMargin: Constants.paddingStandard
                        spacing: 1

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            spacing: Constants.itemSpacing

                            Text {
                                text: title
                                font.family: Constants.fontFamily
                                font.pixelSize: Constants.fontSizeBody
                                font.bold: true
                                color: Constants.textPrimary
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Text {
                                text: source
                                font.family: Constants.fontFamily
                                font.pixelSize: Constants.fontSizeSecondary
                                color: Constants.textDisabled
                            }
                        }

                        Text {
                            text: description
                            font.family: Constants.fontFamily
                            font.pixelSize: Constants.fontSizeSecondary
                            color: Constants.textSecondary
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: commandList.count === 0
                    text: qsTr("无匹配命令")
                    font.family: Constants.fontFamily
                    font.pixelSize: Constants.fontSizeBody
                    color: Constants.textDisabled
                }
            }

            // ---- 快捷键提示 ----
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Enter 执行 / Up Down 切换 / Esc 关闭")
                font.family: Constants.fontFamily
                font.pixelSize: Constants.fontSizeSecondary
                color: Constants.textDisabled
            }
        }
    }
}
