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

        RowLayout {
            spacing: Constants.itemSpacing
            FluIndicator {
                active: true
                animate: false
                activeColor: Constants.danger
            }
            FluText {
                isTitle: true
                text: qsTr("删除扩展")
                font.bold: true
            }
        }

        FluText {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: qsTr("确认删除扩展 %1？此操作不可逆。").arg(root.pluginName)
        }

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

        Item {
            Layout.fillWidth: true
            implicitHeight: 32

            Row {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                layoutDirection: Qt.RightToLeft
                spacing: Constants.itemSpacing

                FluButton {
                    text: qsTr("删除")
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
