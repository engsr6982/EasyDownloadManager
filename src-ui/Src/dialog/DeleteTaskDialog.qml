import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../fluent"
import ".."

FluPopup {
    id: root

    property int taskId: -1

    width: 340
    height: contentColumn.implicitHeight + topPadding + bottomPadding
    closePolicy: Popup.CloseOnEscape

    signal confirmed(int taskId, bool deleteFiles)

    padding: Constants.paddingStandard
    topPadding: Constants.paddingStandard
    bottomPadding: Constants.paddingStandard

    onClosed: deleteFilesCheckBox.checked = false

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
                text: qsTr("删除任务")
                font.bold: true
            }
        }

        // 提示文本
        FluText {
            text: qsTr("是否确认删除此任务？")
        }

        Item {
            Layout.fillHeight: true
        }

        // 底部操作栏
        Item {
            Layout.fillWidth: true
            implicitHeight: 32

            // 左侧：删除已下载文件复选框
            FluCheckBox {
                id: deleteFilesCheckBox
                text: qsTr("删除已下载的文件")
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
            }

            // 右侧：操作按钮
            Row {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                layoutDirection: Qt.RightToLeft
                spacing: Constants.itemSpacing

                FluButton {
                    text: qsTr("删除")
                    isDanger: true
                    onClicked: {
                        root.confirmed(root.taskId, deleteFilesCheckBox.checked);
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
