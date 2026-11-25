import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../fluent"
import ".."

FluPopup {
    id: root

    property string pluginName: ""
    property string dirPath: ""

    width: 360
    height: contentColumn.implicitHeight + topPadding + bottomPadding
    closePolicy: Popup.CloseOnEscape

    signal confirmed(string name)

    padding: Constants.paddingStandard
    topPadding: Constants.paddingStandard
    bottomPadding: Constants.paddingStandard

    ColumnLayout {
        id: contentColumn
        anchors.fill: parent
        spacing: Constants.itemSpacing

        // 标题栏
        RowLayout {
            spacing: Constants.itemSpacing
            FluIndicator {
                active: true
                animate: false
                activeColor: Constants.danger
            }
            FluText {
                isTitle: true
                text: qsTr("卸载插件")
                font.bold: true
            }
        }

        // 提示文本：占满宽度换行，不随字数横向撑开挤走底部按钮
        FluText {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: qsTr("将先禁用「%1」并移除其命令与事件，随后物理删除插件目录，此操作不可恢复。").arg(root.pluginName)
        }

        // 插件目录路径：无空格串按任意位置折行
        FluText {
            Layout.fillWidth: true
            visible: root.dirPath.length > 0
            color: Constants.textDisabled
            font.pixelSize: Constants.fontSizeSecondary
            wrapMode: Text.WrapAnywhere
            text: root.dirPath
        }

        Item {
            Layout.fillHeight: true
        }

        // 底部操作栏：Item 定高承载宽度，Row 右锚，与正文分处不同纵向段，正文再长也顶不出去
        Item {
            Layout.fillWidth: true
            implicitHeight: 32

            Row {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                layoutDirection: Qt.RightToLeft
                spacing: Constants.itemSpacing

                FluButton {
                    text: qsTr("卸载")
                    isDanger: true
                    onClicked: {
                        root.confirmed(root.pluginName);
                        root.close();
                    }
                }

                FluButton {
                    text: qsTr("取消")
                    onClicked: root.close()
                }
            }
        }
    }
}
