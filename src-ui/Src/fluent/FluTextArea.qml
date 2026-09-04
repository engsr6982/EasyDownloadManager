import QtQuick
import QtQuick.Templates as T
import ".."

T.TextArea {
    id: control

    font.family: Constants.fontFamily
    font.pixelSize: Constants.fontSizeBody
    color: control.enabled ? Constants.textPrimary : Constants.textDisabled

    implicitWidth: Math.max(background ? background.implicitWidth : 0, contentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(background ? background.implicitHeight : 0, contentHeight + topPadding + bottomPadding)

    // 选区颜色高亮
    selectionColor: Qt.rgba(Constants.accentPrimary.r, Constants.accentPrimary.g, Constants.accentPrimary.b, 0.3)
    selectedTextColor: control.color

    padding: Constants.paddingStandard
    selectByMouse: true
    wrapMode: TextEdit.WrapAtWordBoundaryOrAnywhere

    // 占位符渲染
    Text {
        x: control.leftPadding
        y: control.topPadding
        width: control.width - control.leftPadding - control.rightPadding

        text: control.placeholderText
        font: control.font
        color: Constants.textSecondary

        visible: !control.text
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitWidth: 260
        implicitHeight: 80

        // 背景状态机
        color: control.enabled ? Constants.bgContent : Constants.bgDisabled
        radius: Constants.radiusControl

        border.color: control.enabled ? Constants.borderSubtle : Constants.borderDisabled
        border.width: 1

        // 底部高亮指示线
        Rectangle {
            id: focusIndicator
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 2
            radius: Constants.radiusControl
            color: Constants.accentPrimary

            // 仅在激活聚焦且可用时展现
            visible: control.activeFocus && control.enabled

            // 左右内缩 1px，完美贴合外框圆角，绝不溢出
            anchors.leftMargin: 1
            anchors.rightMargin: 1
        }
    }
}
