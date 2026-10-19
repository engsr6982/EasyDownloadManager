pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import ".."

Rectangle {
    id: root

    property string iconText: ""
    property string text: ""
    property bool isDanger: false
    property var bindMenu: null
    signal triggered

    Layout.fillWidth: true
    height: 30
    radius: Constants.radiusControl

    // 依靠 MouseArea.containsMouse 实现平滑 Hover 变色；禁用时无反馈
    color: root.enabled && itemMouseArea.containsMouse
        ? (isDanger ? Qt.alpha(Constants.danger, 0.08) : Constants.standardHover)
        : "transparent"

    MouseArea {
        id: itemMouseArea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
            if (bindMenu && bindMenu.close) {
                bindMenu.close(); // 调用绑定的父菜单关闭函数
            }
            root.triggered(); // 执行原本的业务逻辑
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 10

        // 禁用态整体压暗：仅靠文字色（textDisabled 与 textSecondary 几乎同色）看不出禁用
        opacity: root.enabled ? 1.0 : 0.38
        Behavior on opacity {
            NumberAnimation {
                duration: 120
            }
        }

        // 图标
        Text {
            text: root.iconText
            font.family: Constants.iconFontFamily
            font.pixelSize: Constants.iconFontSize
            color: root.enabled
                ? (root.isDanger ? Constants.danger
                                 : (itemMouseArea.containsMouse ? Constants.accentPrimary : Constants.textSecondary))
                : Constants.textDisabled
            Layout.alignment: Qt.AlignVCenter
        }

        // 文本
        Text {
            text: root.text
            font.family: Constants.fontFamily
            font.pixelSize: Constants.fontSizeBody
            color: root.isDanger ? Constants.danger : (root.enabled ? Constants.textPrimary : Constants.textDisabled)
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
        }
    }
}
