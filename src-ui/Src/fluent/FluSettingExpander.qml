import QtQuick
import QtQuick.Layouts
import ".."

Rectangle {
    id: root
    property string iconText: ""
    property string title: ""
    property string description: ""
    property bool expanded: false

    default property alias contentData: contentContainer.data

    Layout.fillWidth: true
    implicitHeight: layout.implicitHeight

    radius: Constants.radiusCard
    color: Constants.bgContent
    border.color: Constants.borderCard
    border.width: 1

    ColumnLayout {
        id: layout
        width: parent.width
        spacing: 0

        // 主卡片
        Rectangle {
            id: headerItem
            Layout.fillWidth: true
            implicitHeight: root.description !== "" ? 64 : 48
            color: "transparent"

            // 点击卡片的淡灰色背景遮罩
            Rectangle {
                anchors.fill: parent
                color: headerArea.pressed ? Constants.borderSubtle : "transparent"
                radius: Constants.radiusCard

                // 展开状态下，按下时通过底部叠加微型矩形，抹平下方圆角，衔接内容流，且不破坏外部总圆角
                Rectangle {
                    visible: root.expanded && headerArea.pressed
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: Constants.radiusCard
                    color: Constants.borderSubtle
                }
            }

            MouseArea {
                id: headerArea
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.expanded = !root.expanded
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Constants.marginStandard
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

                    // 主文本
                    Text {
                        text: root.title
                        font.family: Constants.fontFamily
                        font.pixelSize: Constants.fontSizeBody
                        font.bold: true
                        color: Constants.textPrimary
                        elide: Text.ElideRight
                    }

                    // 描述
                    Text {
                        visible: root.description !== ""
                        text: root.description
                        font.family: Constants.fontFamily
                        font.pixelSize: Constants.fontSizeSecondary
                        color: Constants.textSecondary
                        elide: Text.ElideRight
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                // 展开指示器箭头
                Text {
                    text: "\uF2A4"
                    font.family: Constants.iconFontFamily
                    font.pixelSize: 12
                    color: Constants.textPrimary
                    Layout.alignment: Qt.AlignVCenter

                    rotation: root.expanded ? 180 : 0
                    Behavior on rotation {
                        NumberAnimation {
                            duration: 150
                            easing.type: Easing.OutCubic
                        }
                    }
                }
            }
        }

        // 展开状态下的卡片底部细线
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Constants.borderSubtle
            opacity: root.expanded ? 1.0 : 0.0
            Behavior on opacity {
                NumberAnimation {
                    duration: 150
                }
            }
        }

        // 动态拉伸内容包裹区
        Item {
            id: contentWrapper
            Layout.fillWidth: true
            Layout.preferredHeight: root.expanded ? contentContainer.implicitHeight : 0
            clip: true

            Behavior on Layout.preferredHeight {
                NumberAnimation {
                    duration: 220
                    easing.type: Easing.OutCubic
                }
            }

            ColumnLayout {
                id: contentContainer
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: 0
            }
        }
    }
}
