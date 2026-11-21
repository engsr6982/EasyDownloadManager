import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import ".."
import "../fluent"

import edm.qml.ui 1.0

/**
 * 扩展卡片：左侧图标块（manifest.icon 指定时显示自定义图标，否则默认图标），
 * 中部名称/版本/描述/来源，右侧启停开关与操作按钮。
 * 打开文件夹与虚拟控制台经 moreRequested 由视图层的公共 FluMenu 弹出。
 */
Item {
    id: cardRoot

    property string name: ""
    property string version: ""
    property string author: ""
    property string description: ""
    property string dirPath: ""
    property int scriptState: ScriptManager.Disabled // Disabled / Enabled / Error
    property string errorText: ""
    property string iconUrl: "" // manifest.icon 解析出的文件 URL，空则用默认图标

    signal toggleRequested(string name, bool enabled)
    signal reloadRequested(string name)
    signal consoleRequested(string name)
    signal moreRequested(string name, string version, Item targetItem, real x, real y)

    readonly property bool isError: scriptState === ScriptManager.Error
    readonly property bool isEnabled: scriptState === ScriptManager.Enabled

    width: parent.width
    height: cardLayout.implicitHeight + Constants.paddingStandard * 2

    Rectangle {
        id: card
        anchors.fill: parent
        radius: Constants.radiusCard
        border.width: 1
        color: cardMouseArea.containsMouse ? Constants.windowSecondaryBase : Constants.bgContent
        border.color: cardMouseArea.containsMouse ? Constants.borderSubtle : Constants.borderCard

        Behavior on color {
            ColorAnimation { duration: 150 }
        }
        Behavior on border.color {
            ColorAnimation { duration: 150 }
        }

        MouseArea {
            id: cardMouseArea
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
        }

        RowLayout {
            id: cardLayout
            anchors.fill: parent
            anchors.margins: Constants.paddingStandard
            spacing: Constants.marginStandard

            // ---- 图标块（自定义图标可用时原色展示，否则状态色贴片 + 默认图标） ----
            Rectangle {
                id: iconTile
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                Layout.alignment: Qt.AlignVCenter
                radius: Constants.radiusControl
                color: iconImage.visible ? "transparent"
                                         : (cardRoot.isError
                                            ? Qt.alpha(Constants.danger, 0.08)
                                            : Qt.alpha(Constants.accentPrimary, 0.08))

                Behavior on color {
                    ColorAnimation { duration: 150 }
                }

                Image {
                    id: iconImage
                    anchors.centerIn: parent
                    width: 36
                    height: 36
                    source: cardRoot.iconUrl
                    sourceSize.width: 72
                    sourceSize.height: 72
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                    asynchronous: true
                    visible: cardRoot.iconUrl.length > 0 && status === Image.Ready
                    // 图片与占位贴片统一圆角：经遮罩把图片四角裁圆
                    layer.enabled: true
                    layer.effect: MultiEffect {
                        maskEnabled: true
                        maskSource: iconMask
                        maskThresholdMin: 0.5
                        maskSpreadAtMin: 1.0
                    }
                }

                Rectangle {
                    id: iconMask
                    width: 36
                    height: 36
                    radius: Constants.radiusControl
                    visible: false
                    layer.enabled: true
                }

                Text {
                    visible: !iconImage.visible
                    anchors.centerIn: parent
                    text: cardRoot.isError ? "\uF3F2" /* error_circle */ : "\uF137" /* apps_add_in */
                    font.family: Constants.iconFontFamily
                    font.pixelSize: 20
                    color: cardRoot.isError ? Constants.danger : Constants.accentPrimary
                }
            }

            // ---- 文本区 ----
            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: 3

                opacity: cardRoot.isEnabled || cardRoot.isError ? 1.0 : Constants.opacityDisabled
                Behavior on opacity {
                    NumberAnimation { duration: 150 }
                }

                // 名称 + 版本
                Item {
                    Layout.fillWidth: true
                    implicitHeight: titleRow.implicitHeight

                    Row {
                        id: titleRow
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Constants.itemSpacing

                        Text {
                            id: nameText
                            width: Math.min(implicitWidth, titleRow.parent.width - versionPill.width - titleRow.spacing)
                            text: cardRoot.name
                            font.family: Constants.fontFamily
                            font.pixelSize: Constants.fontSizeBody
                            font.bold: true
                            color: Constants.textPrimary
                            elide: Text.ElideRight
                        }

                        Rectangle {
                            id: versionPill
                            implicitWidth: versionLabel.implicitWidth + 12
                            implicitHeight: 18
                            radius: 9
                            color: Constants.standardHover

                            Text {
                                id: versionLabel
                                anchors.centerIn: parent
                                text: "v" + cardRoot.version
                                font.family: Constants.fontFamily
                                font.pixelSize: Constants.fontSizeSecondary
                                color: Constants.textSecondary
                            }
                        }
                    }
                }

                // 描述
                Text {
                    text: cardRoot.description.length > 0
                          ? cardRoot.description : qsTr("暂无描述")
                    font.family: Constants.fontFamily
                    font.pixelSize: Constants.fontSizeSecondary
                    color: Constants.textSecondary
                    elide: Text.ElideRight
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    Layout.fillWidth: true
                }

                // 来源（正常态）/ 错误消息（错误态，点击打开控制台）
                Text {
                    visible: !cardRoot.isError
                    text: cardRoot.author.length > 0
                          ? cardRoot.author : qsTr("未知作者")
                    font.family: Constants.fontFamily
                    font.pixelSize: Constants.fontSizeSecondary
                    color: Constants.textDisabled
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Text {
                    visible: cardRoot.isError
                    text: cardRoot.errorText.length > 0
                          ? cardRoot.errorText : qsTr("加载失败")
                    font.family: Constants.fontFamily
                    font.pixelSize: Constants.fontSizeSecondary
                    color: errorArea.containsMouse ? Constants.dangerPressed : Constants.danger
                    elide: Text.ElideRight
                    Layout.fillWidth: true

                    MouseArea {
                        id: errorArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: cardRoot.consoleRequested(cardRoot.name)
                    }

                    ToolTip.visible: errorArea.containsMouse
                    ToolTip.text: qsTr("点击打开控制台查看详情")
                    ToolTip.delay: 400
                }
            }

            // ---- 控制区 ----
            RowLayout {
                Layout.alignment: Qt.AlignVCenter
                spacing: 4

                FluSwitch {
                    // checkable=false：点击不自行翻转，视觉状态由卡片数据驱动
                    showText: false
                    checkable: false
                    checked: cardRoot.isEnabled
                    onClicked: cardRoot.toggleRequested(cardRoot.name, !cardRoot.isEnabled)

                    ToolTip.visible: hovered
                    ToolTip.text: cardRoot.isEnabled ? qsTr("禁用") : qsTr("启用")
                    ToolTip.delay: 400
                }

                FluIconButton {
                    text: "\uF13E" // arrow_clockwise 重载
                    enabled: cardRoot.scriptState !== ScriptManager.Disabled
                    onClicked: cardRoot.reloadRequested(cardRoot.name)

                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("重载")
                    ToolTip.delay: 400
                }

                FluIconButton {
                    text: "\uF557" // more 更多操作
                    onClicked: cardRoot.moreRequested(cardRoot.name, cardRoot.version, this, 0, this.height)

                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("更多操作")
                    ToolTip.delay: 400
                }
            }
        }
    }
}
