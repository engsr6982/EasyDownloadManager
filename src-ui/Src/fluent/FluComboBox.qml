pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T
import QtQuick.Effects
import ".."

T.ComboBox {
    id: control

    implicitWidth: 140
    implicitHeight: 32

    property int popupPadding: 4   // 弹出框内边距
    property int shadowBleed: 32   // 阴影出血区安全边距
    readonly property real delegateHeight: 32

    // 动态计算目标高度：包含项高度、间隙高度以及上下 padding 和边框
    readonly property real targetHeight: Math.min(control.count * delegateHeight + Math.max(0, control.count - 1) * popupPadding + popupPadding * 2 + 2, 320)

    property int activeIndex: 0
    property real calculatedY: 0
    property real animHeight: 32
    property real animY: 0

    // 主体背景
    background: Rectangle {
        color: {
            if (!control.enabled)
                return Constants.bgDisabled; // 禁用
            if (control.pressed)
                return Constants.borderSubtle;
            return control.hovered ? Constants.windowBase : "transparent";
        }
        border.color: control.enabled ? Constants.borderCard : Constants.borderDisabled
        border.width: 1
        radius: Constants.radiusControl
        opacity: control.enabled ? Constants.opacityStandard : Constants.opacityDisabled

        // 底部高程沉淀线
        Rectangle {
            height: 1
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: Constants.radiusControl
            anchors.rightMargin: Constants.radiusControl
            anchors.bottomMargin: 1
            color: Constants.shadowStandardBottom
        }
    }

    // 显示文本
    contentItem: Text {
        text: control.displayText
        font.family: Constants.fontFamily
        font.pixelSize: Constants.fontSizeBody
        color: control.enabled ? Constants.textPrimary : Constants.textDisabled
        verticalAlignment: Text.AlignVCenter
        leftPadding: Constants.paddingStandard
    }

    // 右侧箭头
    indicator: Text {
        text: "\uF2A4"
        font.family: Constants.iconFontFamily
        font.pixelSize: 10
        color: Constants.textSecondary
        x: control.width - width - Constants.paddingStandard
        y: (control.height - height) / 2
        rotation: control.popup.visible ? 180 : 0 // 默认朝下，点击后朝上
        Behavior on rotation {
            NumberAnimation {
                duration: 300
                easing.type: Easing.OutExpo
            }
        }
    }

    // 下拉浮窗
    popup: Popup {
        // 撑大物理窗口，解决操作系统层面对越界阴影的裁切
        width: control.width + control.shadowBleed * 2
        padding: 0
        margins: 0

        // 物理窗口高度同步撑大
        implicitHeight: control.targetHeight + control.shadowBleed * 2

        // 物理窗口反向偏移补偿，保证视觉主体依旧对齐 ComboBox
        x: -control.shadowBleed
        y: control.calculatedY - control.shadowBleed

        onAboutToShow: {
            control.activeIndex = Math.max(0, control.currentIndex);
            // 计算当前选中项在带 padding/gap 布局下的偏移量
            var itemTopOffset = control.activeIndex * (control.delegateHeight + control.popupPadding) + control.popupPadding;
            var rawY = -(itemTopOffset);

            // 边缘避让计算
            if (control.Window && control.Window.window) {
                var globalComboY = control.mapToItem(null, 0, 0).y;
                var windowHeight = control.Window.window.height;
                var safetyMargin = 8;

                if (globalComboY + rawY < safetyMargin) {
                    rawY = safetyMargin - globalComboY;
                } else if (globalComboY + rawY + control.targetHeight > windowHeight - safetyMargin) {
                    rawY = windowHeight - safetyMargin - control.targetHeight - globalComboY;
                }
            }
            control.calculatedY = rawY;

            // 滚动条视口偏移量计算
            var totalContentHeight = control.count * control.delegateHeight + Math.max(0, control.count - 1) * control.popupPadding + control.popupPadding * 2;
            var maxContentY = Math.max(0, totalContentHeight - control.targetHeight);
            var desiredContentY = control.activeIndex * (control.delegateHeight + control.popupPadding);
            listView.contentY = Math.min(desiredContentY, maxContentY);

            // 打开前复位动画参数
            control.animHeight = control.height;
            control.animY = -control.calculatedY;
        }

        enter: Transition {
            NumberAnimation {
                target: control.popup.contentItem
                property: "opacity"
                to: 1.0
                duration: 150
                easing.type: Easing.OutQuad
            }
            NumberAnimation {
                target: control
                property: "animHeight"
                to: control.targetHeight
                duration: 250
                easing.type: Easing.OutExpo
            }
            NumberAnimation {
                target: control
                property: "animY"
                to: 0
                duration: 250
                easing.type: Easing.OutExpo
            }
        }

        exit: Transition {
            NumberAnimation {
                target: control.popup.contentItem
                property: "opacity"
                to: 0.0
                duration: 120
                easing.type: Easing.OutQuad
            }
        }

        // 视觉与效果背景层
        background: Item {
            // 单层白底背板
            Rectangle {
                id: bgPlate
                x: control.shadowBleed
                y: control.shadowBleed + control.animY
                width: control.width
                height: control.animHeight
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
                opacity: control.popup.contentItem ? control.popup.contentItem.opacity : 0.0
            }
        }

        // 纯交互与内容层
        contentItem: Item {
            Item {
                x: control.shadowBleed
                y: control.shadowBleed + control.animY
                width: control.width
                height: control.animHeight
                clip: true

                Item {
                    y: -control.animY
                    width: parent.width
                    height: control.targetHeight

                    ListView {
                        id: listView
                        anchors.fill: parent
                        anchors.margins: control.popupPadding // 左右以及上下 Padding 边界
                        spacing: control.popupPadding         // 元素之间的间隔
                        clip: true
                        model: control.delegateModel
                        boundsBehavior: Flickable.StopAtBounds

                        ScrollBar.vertical: ScrollBar {
                            // 计算是否需要显示滚动条
                            policy: (control.count * control.delegateHeight + Math.max(0, control.count - 1) * control.popupPadding > control.targetHeight - control.popupPadding * 2) ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                        }
                    }
                }
            }
        }
    }

    delegate: ItemDelegate {
        id: delegateItem
        width: listView.width
        height: control.delegateHeight
        highlighted: control.highlightedIndex === index

        required property int index
        required property var modelData

        contentItem: Text {
            text: delegateItem.modelData
            color: Constants.textPrimary
            font.family: Constants.fontFamily
            font.pixelSize: Constants.fontSizeBody
            verticalAlignment: Text.AlignVCenter
            leftPadding: 10
        }

        background: Rectangle {
            color: {
                if (control.activeIndex === delegateItem.index || delegateItem.highlighted)
                    return Constants.standardHover; // 已选中项背景
                return "transparent";
            }
            radius: Constants.radiusControl

            FluIndicator {
                active: true
                animate: false
                anchors.left: parent.left
                anchors.leftMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                visible: (control.activeIndex === delegateItem.index) && control.popup.visible
            }
        }
    }
}
