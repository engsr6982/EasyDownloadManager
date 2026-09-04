import QtQuick
import QtQuick.Layouts
import ".."

Rectangle {
    id: root

    property string iconText: ""
    property string title: ""
    property string description: ""
    property bool isSubRow: false
    property bool transparentBg: false

    default property alias contentData: rightContainer.data

    Layout.fillWidth: true
    implicitHeight: description !== "" ? 64 : 48

    // 如果是内嵌子行，背景透明，否则会遮住父级 Expander 的底部圆角
    color: (isSubRow || transparentBg) ? "transparent" : Constants.bgContent
    radius: isSubRow ? 0 : Constants.radiusCard

    border.width: (isSubRow || transparentBg) ? 0 : 1
    border.color: Constants.borderCard

    // 子卡片底部分割线
    Rectangle {
        visible: root.isSubRow
        width: parent.width - 48
        height: 1
        color: Constants.borderSubtle
        anchors.bottom: parent.bottom
        anchors.right: parent.right
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: root.isSubRow ? 48 : Constants.marginStandard
        anchors.rightMargin: Constants.marginStandard
        spacing: Constants.marginStandard

        // 图标
        Text {
            visible: root.iconText !== ""
            text: root.iconText
            font.family: Constants.iconFontFamily
            font.pixelSize: 20
            color: Constants.textPrimary
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: 24
            horizontalAlignment: Text.AlignHCenter
        }

        // 文字区域
        ColumnLayout {
            spacing: 2
            Layout.alignment: Qt.AlignVCenter

            // 让文字区域接管中间所有的弹性剩余空间
            Layout.fillWidth: true

            // 限制最大宽度, 总宽减去右侧控件预留空间和图标间距，防止无限撑大
            Layout.maximumWidth: parent.width - (root.iconText !== "" ? 48 : 0) - rightContainer.implicitWidth - 48

            Text {
                text: root.title
                font.family: Constants.fontFamily
                font.pixelSize: Constants.fontSizeBody
                font.bold: true
                color: Constants.textPrimary

                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            Text {
                visible: root.description !== ""
                text: root.description
                font.family: Constants.fontFamily
                font.pixelSize: Constants.fontSizeSecondary
                color: Constants.textSecondary

                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }

        // 右侧操作区
        RowLayout {
            id: rightContainer
            spacing: Constants.itemSpacing
            Layout.fillWidth: false // 以元素自己的宽度为准
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        }
    }
}
