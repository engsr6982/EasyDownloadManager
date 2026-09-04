pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import ".."

T.Button {
    id: control

    property bool isAccent: false
    property bool isDanger: false

    // 图标配置
    property string iconText: "" // 图标 glyph（留空则不显示图标）
    property color iconColor: (control.isAccent || control.isDanger) ? Constants.textOnAccent : Constants.textPrimary
    property int iconSize: Constants.iconFontSize
    property string iconFontFamily: Constants.iconFontFamily
    property bool iconRight: false // 图标是否显示在文本右侧（默认在左侧）

    hoverEnabled: true

    // ================== 动态间距与内边距自适应 ==================
    // 如果是纯图标扁平按钮（没有文本），则缩小内边距以实现完美的正方形效果
    padding: 0
    topPadding: (control.text === "") ? 2 : 4
    bottomPadding: (control.text === "") ? 2 : 4
    leftPadding: (control.text === "") ? 2 : Constants.paddingStandard
    rightPadding: (control.text === "") ? 2 : Constants.paddingStandard

    implicitWidth: Math.max(implicitBackgroundWidth, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)

    // ================== 背景板样式 ==================
    background: Rectangle {
        // 如果是纯图标扁平按钮，隐式尺寸收窄为 28x28
        implicitWidth: (control.text === "" && control.flat) ? 28 : 60
        implicitHeight: (control.text === "" && control.flat) ? 28 : 32
        radius: Constants.radiusControl

        color: {
            if (control.flat) {
                if (!control.enabled)
                    return "transparent";
                if (control.pressed)
                    return Constants.standardPressed;
                return control.hovered ? Constants.borderSubtle : "transparent";
            }
            if (control.pressed) {
                if (control.isDanger) return Constants.dangerPressed;
                return control.isAccent ? Constants.accentPressed : Constants.standardPressed;
            }
            if (control.hovered) {
                if (control.isDanger) return Constants.dangerHover;
                return control.isAccent ? Constants.accentHover : Constants.standardHover;
            }
            if (control.isDanger) return Constants.danger;
            return control.isAccent ? Constants.accentPrimary : Constants.standardPrimary;
        }

        border.color: {
            if (control.flat && !control.hovered && !control.pressed)
                return "transparent";
            return control.pressed ? Constants.borderSubtle : Constants.borderCard;
        }
        border.width: 1

        // 底部高程沉淀线（flat 按钮不显示沉淀线）
        Rectangle {
            height: 1
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: Constants.radiusControl
            anchors.rightMargin: Constants.radiusControl
            anchors.bottomMargin: 1
            color: control.isDanger ? Constants.dangerShadowBottom : (control.isAccent ? Constants.shadowAccentBottom : Constants.shadowStandardBottom)
            visible: !control.flat
        }
    }

    // ================== 内容排版 ==================
    contentItem: Item {
        implicitWidth: contentRow.implicitWidth
        implicitHeight: contentRow.implicitHeight

        Row {
            id: contentRow
            anchors.centerIn: parent
            spacing: (control.iconText !== "" && control.text !== "") ? 6 : 0
            layoutDirection: control.iconRight ? Qt.RightToLeft : Qt.LeftToRight
            opacity: control.enabled ? Constants.opacityStandard : Constants.opacityDisabled

            // 图标元素
            Text {
                id: iconItem
                text: control.iconText
                font.family: control.iconFontFamily
                font.pixelSize: control.iconSize
                color: control.iconColor
                visible: control.iconText !== ""
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                anchors.verticalCenter: parent.verticalCenter
            }

            // 文本元素
            Text {
                id: textItem
                text: control.text
                font.family: Constants.fontFamily
                font.pixelSize: Constants.fontSizeBody
                color: (control.isAccent || control.isDanger) ? Constants.textOnAccent : Constants.textPrimary
                visible: control.text !== ""
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
}