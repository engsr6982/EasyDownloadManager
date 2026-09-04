import QtQuick
import QtQuick.Layouts
import ".."

ColumnLayout {
    id: root
    spacing: Constants.itemSpacing
    Layout.fillWidth: true

    property string groupTitle: ""
    default property alias groupContent: cardsContainer.data

    Text {
        visible: root.groupTitle !== ""
        text: root.groupTitle
        font.family: Constants.fontFamily
        font.pixelSize: Constants.fontSizeTitle
        font.bold: true
        color: Constants.textPrimary

        Layout.topMargin: Constants.marginStandard
        Layout.leftMargin: 4
    }

    ColumnLayout {
        id: cardsContainer
        Layout.fillWidth: true
        spacing: 4
    }
}
