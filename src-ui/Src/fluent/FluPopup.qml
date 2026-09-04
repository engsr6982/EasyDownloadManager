pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Templates as T
import ".."

T.Popup {
    id: root

    implicitWidth: 180
    implicitHeight: 130

    visible: false
    modal: true
    focus: true

    // 动态居中定位
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0

    // 允许通过点击外部遮罩或按 ESC 键关闭弹窗
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    padding: Constants.paddingStandard
    background: Item {
        // 单层白底背板
        Rectangle {
            id: bgPlate
            width: root.width
            height: root.height
            radius: Constants.radiusCard
            color: Constants.bgContent
            border.color: Constants.borderCard
            border.width: 1
        }

        // 阴影组件
        MultiEffect {
            source: bgPlate
            anchors.fill: bgPlate
            autoPaddingEnabled: true
            shadowEnabled: true
            shadowColor: Constants.bgShadowColor
            shadowBlur: Constants.bgShadowBlur
            shadowVerticalOffset: Constants.bgShadowVerticalOffset

            // 保持阴影同步进入/退出淡出动画
            opacity: root.contentItem ? root.contentItem.opacity : 0.0
        }
    }

    Overlay.modal: Rectangle {
        color: Constants.dialogOverlay

        x: root.parent ? root.parent.mapToItem(Overlay.overlay, 0, 0).x : 0
        y: root.parent ? root.parent.mapToItem(Overlay.overlay, 0, 0).y : 0
        width: root.parent ? root.parent.width : 0
        height: root.parent ? root.parent.height : 0
    }

    // 动效
    enter: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0.0
            to: 1.0
            duration: 150
        }
        NumberAnimation {
            property: "scale"
            from: 1.05
            to: 1.0
            duration: 150
        }
    }
    exit: Transition {
        NumberAnimation {
            property: "opacity"
            from: 1.0
            to: 0.0
            duration: 100
        }
        NumberAnimation {
            property: "scale"
            from: 1.0
            to: 0.95
            duration: 100
        }
    }
}
