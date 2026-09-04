import QtQuick
import QtQuick.Templates as T
import ".."

T.ProgressBar {
    id: control

    implicitWidth: 200
    implicitHeight: 4

    padding: 0

    // 轨道背景
    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 4
        color: Constants.borderSubtle
        radius: 2
    }

    // 进度内容区
    contentItem: Item {
        implicitWidth: 200
        implicitHeight: 4
        clip: true

        // 常规模式
        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: 2
            color: control.value == 1.0 ? Constants.success : Constants.accentPrimary
            visible: !control.indeterminate
        }

        // 不确定模式
        Rectangle {
            width: parent.width * 0.3
            height: parent.height
            radius: 2
            color: Constants.accentPrimary
            visible: control.indeterminate

            NumberAnimation on x {
                from: -(control.availableWidth * 0.3)
                to: control.availableWidth
                duration: 1200
                loops: Animation.Infinite
                running: control.indeterminate && control.visible
            }
        }
    }
}
