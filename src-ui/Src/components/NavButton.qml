import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../fluent"

Button {
    id: navBtn
    width: parent.width
    height: 36
    checkable: true
    checked: false

    required property ButtonGroup targetGroup // 关联的按钮组
    property string viewSource: "" // 视图源文件
    property string iconText: "" // 文本图标

    ButtonGroup.group: targetGroup

    // 背景层
    background: Rectangle {
        id: bgRect

        anchors.fill: parent
        anchors.leftMargin: 1
        anchors.rightMargin: 1

        radius: Constants.radiusControl
        color: (navBtn.checked || navBtn.hovered) ? Constants.navHover : "transparent"

        // 状态指示器
        FluIndicator {
            active: navBtn.checked
            anchors.left: parent.left
            anchors.leftMargin: 2
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    // 文本层
    contentItem: RowLayout {
        spacing: Constants.itemSpacing
        Layout.fillWidth: true
        Layout.fillHeight: true

        // 图标
        Text {
            // visible: navBtn.iconText !== ""
            text: navBtn.iconText
            font.family: Constants.iconFontFamily
            font.pixelSize: Constants.iconFontSize
            color: navBtn.checked ? Constants.accentPrimary : Constants.textSecondary

            leftPadding: Constants.paddingStandard // 左间距，离状态条远点
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
        }

        // 文本
        Text {
            text: navBtn.text
            font.family: Constants.fontFamily
            font.pixelSize: Constants.fontSizeBody
            color: navBtn.checked ? Constants.textPrimary : Constants.textSecondary

            Layout.fillWidth: true
            verticalAlignment: Text.AlignVCenter
        }
    }
}
