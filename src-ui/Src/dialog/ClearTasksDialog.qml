import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../fluent"
import ".."

FluPopup {
    id: root

    width: 360
    height: contentColumn.implicitHeight + topPadding + bottomPadding
    closePolicy: Popup.CloseOnEscape

    signal confirmed(bool deleteFiles)

    padding: Constants.paddingStandard
    topPadding: Constants.paddingStandard
    bottomPadding: Constants.paddingStandard

    onClosed: deleteFilesCheckBox.checked = false

    ColumnLayout {
        id: contentColumn
        anchors.fill: parent
        spacing: Constants.itemSpacing

        RowLayout {
            spacing: Constants.itemSpacing
            FluIndicator {
                active: true
                animate: false
                activeColor: Constants.danger
            }
            FluText {
                isTitle: true
                text: qsTr("清理所有任务")
                font.bold: true
            }
        }

        FluText {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: qsTr("此操作将清空所有任务记录(不可逆)，请谨慎操作。")
        }

        Item {
            Layout.fillHeight: true
        }

        Item {
            Layout.fillWidth: true
            implicitHeight: 32

            FluCheckBox {
                id: deleteFilesCheckBox
                text: qsTr("删除已下载的文件")
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
            }

            Row {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                layoutDirection: Qt.RightToLeft
                spacing: Constants.itemSpacing

                FluButton {
                    text: qsTr("清理")
                    isDanger: true
                    onClicked: {
                        root.confirmed(deleteFilesCheckBox.checked);
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
