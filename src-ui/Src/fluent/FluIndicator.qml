import QtQuick
import ".."

Rectangle {
    id: root

    property bool active: false          // 是否激活
    property bool horizontal: false      // 是否为横向模式（默认竖向）
    property bool animate: true          // 是否开启尺寸伸缩动画
    property color activeColor: Constants.indicatorSelected// 激活颜色

    readonly property real thick: 3      // 指示器厚度
    readonly property real longSize: 16  // 激活时的长度

    color: activeColor
    radius: thick / 2 // 自动圆角

    // 根据横竖模式，动态交换宽高
    width: horizontal ? (active ? longSize : 0) : thick
    height: horizontal ? thick : (active ? longSize : 0)

    // 动画
    Behavior on width {
        enabled: root.animate && root.horizontal
        NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
    }

    Behavior on height {
        enabled: root.animate && !root.horizontal
        NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
    }
}