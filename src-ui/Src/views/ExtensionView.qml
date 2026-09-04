import QtQuick
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: root
    anchors.fill: parent

    // 列布局
    ColumnLayout {
        anchors.fill: parent
        spacing: Constants.itemSpacing

        // 标题控件行
        RowLayout {
            id: toolbar
            Layout.fillWidth: true
            Layout.preferredHeight: Constants.viewTitlebarHeight

            Text {
                text: qsTr("插件中心")
                font.family: Constants.fontFamily
                font.pixelSize: Constants.fontSizeTitle
                font.bold: true
                color: Constants.textPrimary
            }

            Item {
                Layout.fillWidth: true
            }

            FluIconButton {
                text: "\uF137"
            }
        }

        // 视图内容区
        Rectangle {
            width: parent.width
            height: root.height - toolbar.height
            color: "transparent"

            FluText {
                text: qsTr("模块开发中...")
                anchors.centerIn: parent
            }
        }
    }
}
