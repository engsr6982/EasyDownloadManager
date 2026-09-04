import QtQuick
import QtQuick.Templates as T
import ".."

T.MenuSeparator {
    id: control
    padding: 0
    topPadding: 2
    bottomPadding: 2

    implicitWidth: parent.width
    implicitHeight: control.contentItem.implicitHeight + topPadding + bottomPadding

    contentItem: Rectangle {
        implicitHeight: 1
        color: Constants.borderSubtle
    }
}
