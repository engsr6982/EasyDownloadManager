pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import ".."

T.CheckBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset, implicitContentHeight + topPadding + bottomPadding, implicitIndicatorHeight + topPadding + bottomPadding)

    padding: 0
    spacing: 6
    hoverEnabled: true

    // 文本内容
    contentItem: Text {
        leftPadding: control.indicator.width + control.spacing
        rightPadding: control.rightPadding

        text: control.text
        font.family: Constants.fontFamily
        font.pixelSize: Constants.fontSizeBody
        color: Constants.textPrimary
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight

        width: control.availableWidth
        height: control.availableHeight
    }

    // 复选框本体
    indicator: Rectangle {
        implicitWidth: 20
        implicitHeight: 20
        radius: Constants.radiusControl

        x: control.leftPadding
        y: control.topPadding + Math.round((control.availableHeight - height) / 2)

        border.width: 1
        border.color: {
            if (!control.enabled)
                return Constants.borderDisabled;
            if (control.checkState === Qt.Checked)
                return Constants.accentPrimary;
            if (control.hovered)
                return Constants.textSecondary;
            return Constants.borderSubtle;
        }

        color: {
            if (!control.enabled)
                return Constants.bgDisabled;
            if (control.checkState === Qt.Checked) {
                if (control.pressed)
                    return Constants.accentPressed;
                if (control.hovered)
                    return Constants.accentHover;
                return Constants.accentPrimary;
            }
            return "transparent";
        }

        Behavior on color {
            ColorAnimation {
                duration: 100
            }
        }
        Behavior on border.color {
            ColorAnimation {
                duration: 100
            }
        }

        // 勾选标记
        Text {
            anchors.centerIn: parent
            text: "\uF295"
            font.family: Constants.iconFontFamily
            font.pixelSize: 12
            color: Constants.textOnAccent
            visible: control.checkState === Qt.Checked
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            z: 1
        }

        // 不确定状态的横线
        Rectangle {
            width: 10
            height: 2
            radius: 1
            anchors.centerIn: parent
            color: Constants.textOnAccent
            visible: control.checkState === Qt.PartiallyChecked
            z: 1
        }
    }
}
