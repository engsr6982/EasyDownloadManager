import QtQuick
import QtQuick.Templates as T
import ".."

T.Switch {
    id: control

    property bool showText: true
    property string textOn: qsTr("开")
    property string textOff: qsTr("关")

    spacing: Constants.itemSpacing
    padding: 0

    // 预先计算好紧凑排列时的理想宽高
    implicitWidth: {
        var finalWidth = 0;
        if (control.showText && control.contentItem) {
            finalWidth += control.contentItem.implicitWidth + control.spacing;
        }
        if (control.indicator) {
            finalWidth += control.indicator.width;
        } else {
            finalWidth += 40;
        }
        return finalWidth + control.leftPadding + control.rightPadding;
    }

    implicitHeight: {
        var textHeight = 0;
        if (control.showText && control.contentItem) {
            textHeight = control.contentItem.implicitHeight;
        }

        var indicatorHeight = 20;
        if (control.indicator) {
            indicatorHeight = control.indicator.height;
        }

        var maxHeight = Math.max(textHeight, indicatorHeight);
        return maxHeight + control.topPadding + control.bottomPadding;
    }

    // 文本组件
    contentItem: Text {
        text: {
            if (!control.showText) {
                return "";
            }
            if (control.checked) {
                return control.textOn;
            }
            return control.textOff;
        }

        font.family: Constants.fontFamily
        font.pixelSize: Constants.fontSizeBody
        color: Constants.textPrimary
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignLeft

        x: control.leftPadding
        y: control.topPadding + Math.round((control.availableHeight - height) / 2)

        width: control.contentItem ? control.contentItem.implicitWidth : 0
        height: control.availableHeight
        elide: Text.ElideRight
    }

    // 开关本体
    indicator: Rectangle {
        width: 40
        height: 20
        radius: 10
        opacity: control.enabled ? Constants.opacityStandard : Constants.opacityDisabled

        x: {
            if (control.showText && control.contentItem) {
                return control.leftPadding + control.contentItem.implicitWidth + control.spacing;
            }
            return control.leftPadding;
        }
        y: control.topPadding + Math.round((control.availableHeight - height) / 2)

        // 背景色
        color: {
            if (control.checked) {
                return Constants.accentPrimary;
            }
            return "transparent";
        }

        border.color: {
            if (control.enabled) {
                if (control.checked) {
                    return Constants.accentPrimary; // 与背景同色(隐藏边框)
                }
                return Constants.textSecondary;
            }
            return Constants.textDisabled;
        }
        border.width: 1

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

        // 内部滑块
        Rectangle {
            width: 12
            height: 12
            radius: 6
            anchors.verticalCenter: parent.verticalCenter

            x: {
                if (control.checked) {
                    return parent.width - width - 4;
                }
                return 4;
            }

            color: {
                if (control.enabled) {
                    if (control.checked) {
                        return Constants.bgContent;
                    }
                    return Constants.textSecondary;
                }
                return Constants.textDisabled;
            }

            Behavior on x {
                NumberAnimation {
                    duration: 150
                    easing.type: Easing.OutCubic
                }
            }
        }
    }
}
