import QtQuick
import QtQuick.Controls
import ".."

Button {
    id: control

    property int iconSize: Constants.iconFontSize
    property color iconColor: Constants.textPrimary
    property color hoverColor: Constants.borderSubtle

    height: 28
    width: 28

    background: Rectangle {
        color: {
            if (!control.enabled) {
                return "transparent"; // 禁用后 hover 透明
            }
            return control.hovered ? Constants.borderSubtle : "transparent";
        }
        radius: Constants.radiusControl
    }
    contentItem: Text {
        text: control.text
        font.family: Constants.iconFontFamily
        font.pixelSize: Constants.iconFontSize

        color: control.enabled ? control.iconColor : Constants.textDisabled
        opacity: control.enabled ? Constants.opacityStandard : Constants.opacityDisabled

        // 水平垂直居中
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
