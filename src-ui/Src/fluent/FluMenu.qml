pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Templates as T
import ".."

T.Popup {
    id: root

    implicitWidth: 120
    implicitHeight: contentItem ? contentItem.implicitHeight + topPadding + bottomPadding : 0

    visible: false
    modal: false
    focus: true

    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 6

    property real animYOffset: -12.0

    // 强类型安全函数，处理边缘碰撞
    function popup(targetItem: Item, xOffset: real, yOffset: real): void {
        if (!targetItem || !root.parent)
            return;

        var globalPos = targetItem.mapToItem(root.parent, xOffset, yOffset);
        var targetX = globalPos.x;
        var targetY = globalPos.y;

        var windowWidth = root.parent.width;
        var windowHeight = root.parent.height;

        var margin = 6;

        // 右边距溢出修正：如果往右画会出界，就往左挪，并保留安全边距
        if (targetX + root.implicitWidth > windowWidth - margin) {
            targetX = windowWidth - root.implicitWidth - margin;
        }

        // 上下边距溢出修正：
        if (targetY + root.implicitHeight > windowHeight - margin) {
            // 直接从鼠标点击的绝对高度（globalPos.y）往上减去菜单自身高度
            // - 2 是为了往上提 2 个像素，防止鼠标直接压在菜单项上发生误触
            targetY = globalPos.y - root.implicitHeight - 2;

            // 兜底：如果菜单太长，往上翻之后把大顶给顶穿了，强行卡在顶部边界内
            if (targetY < margin) {
                targetY = margin;
            }
        }

        root.x = Math.max(margin, targetX);
        root.y = Math.max(margin, targetY);
        root.open();
    }

    background: Item {
        transform: [
            Translate {
                y: root.animYOffset
            }
        ]

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

    // 条目布局容器
    contentItem: ColumnLayout {
        spacing: 2

        // 文字和图标同步进行平移
        transform: [
            Translate {
                y: root.animYOffset
            }
        ]
    }

    // 入场向下弹出，出场原地点击渐隐
    enter: Transition {
        // 透明度淡入
        NumberAnimation {
            property: "opacity"
            from: 0.0
            to: 1.0
            duration: 120
            easing.type: Easing.OutCubic
        }
        // 纵向滑落弹出（从上方 -12px 的地方丝滑坠落到 0px）
        NumberAnimation {
            property: "animYOffset"
            from: -12.0
            to: 0.0
            duration: 150
            easing.type: Easing.OutQuint
        }
    }

    // 点击菜单项后，原地直接轻柔淡出
    exit: Transition {
        NumberAnimation {
            property: "opacity"
            from: 1.0
            to: 0.0
            duration: 100
            easing.type: Easing.OutCubic
        }
    }
}
