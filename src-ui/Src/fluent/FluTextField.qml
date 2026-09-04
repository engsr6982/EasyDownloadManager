pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import ".."

T.TextField {
    id: control

    property bool isPassword: false
    property bool passwordVisible: false

    hoverEnabled: true

    implicitWidth: implicitBackgroundWidth
    implicitHeight: implicitBackgroundHeight

    padding: 0
    leftPadding: 10
    topPadding: 0
    bottomPadding: 0

    // 动态调整右边距：如果是密码框，给右侧的眼睛图标留出安全的独立空间
    rightPadding: isPassword ? 36 : 10
    font.family: Constants.fontFamily
    font.pixelSize: Constants.fontSizeBody

    color: control.enabled ? Constants.textPrimary : Constants.textDisabled
    selectionColor: Constants.accentPrimary
    selectedTextColor: Constants.textOnAccent
    placeholderTextColor: Constants.textSecondary
    verticalAlignment: TextInput.AlignVCenter

    // 根据密码模式和显隐状态，切换输入框的遮罩行为
    echoMode: {
        if (isPassword && !passwordVisible) {
            return TextInput.Password;
        }
        return TextInput.Normal;
    }

    // 占位符(占位文本)
    Text {
        text: control.placeholderText
        font: control.font
        color: control.placeholderTextColor
        verticalAlignment: control.verticalAlignment
        visible: control.text.length === 0

        x: control.leftPadding
        y: control.topPadding
        width: control.width - control.leftPadding - control.rightPadding
        height: control.height - control.topPadding - control.bottomPadding
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 32

        color: {
            if (!control.enabled) {
                return Constants.bgDisabled;
            }
            if (control.activeFocus) {
                return Constants.bgContent;
            }
            if (control.hovered) {
                return Constants.windowSecondaryBase;
            }
            return Constants.bgContent;
        }
        border.color: {
            if (!control.enabled)
                return Constants.borderDisabled;
            if (control.activeFocus)
                return Constants.accentPrimary;
            if (control.hovered)
                return Constants.textSecondary;
            return Constants.borderCard;
        }

        border.width: 1
        radius: Constants.radiusControl
        opacity: control.enabled ? Constants.opacityStandard : Constants.opacityDisabled

        // 平滑渐变
        Behavior on color {
            ColorAnimation {
                duration: 150
            }
        }
        Behavior on border.color {
            ColorAnimation {
                duration: 150
            }
        }

        // “中心向两端展延”聚焦高亮线
        Rectangle {
            height: 2
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            color: Constants.accentPrimary

            width: control.activeFocus ? (parent.width - Constants.radiusControl * 2) : 0

            Behavior on width {
                NumberAnimation {
                    duration: 250
                    easing.type: Easing.OutQuint
                }
            }
        }
    }

    // 密码显隐切换图标部件
    Text {
        id: eyeIcon
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter

        // 只有开启密码模式且未被禁用时才显示
        visible: control.isPassword && control.enabled

        font.family: Constants.iconFontFamily
        font.pixelSize: Constants.iconFontSize

        // 依据密码状态瞬间切换码位（默认睁眼，隐藏时闭眼）
        text: control.passwordVisible ? "\u{F06D3}" : "\uEFCA"

        color: {
            if (iconMouseArea.containsPress) {
                return Constants.accentPrimary;
            }
            if (iconMouseArea.containsMouse) {
                return Constants.textPrimary;
            }
            return Constants.textSecondary;
        }

        MouseArea {
            id: iconMouseArea
            anchors.fill: parent
            anchors.margins: -4 // 物理向外扩 4px 边缘判定，提升高分屏下的点击手感
            hoverEnabled: true
            onClicked: {
                control.passwordVisible = !control.passwordVisible;
            }
        }
    }
}
