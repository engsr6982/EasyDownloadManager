import QtQuick
import ".."

Text {
    id: control

    property bool isTitle: false
    property bool isSecondary: false
    property bool isAccent: false
    property bool textOnAccent: false

    font.family: Constants.fontFamily
    font.pixelSize: {
        if (isTitle)
            return Constants.fontSizeTitle;
        return isSecondary ? Constants.fontSizeSecondary : Constants.fontSizeBody;
    }

    color: {
        if (textOnAccent)
            return Constants.textOnAccent;
        if (isAccent)
            return Constants.textAccent;
        return isSecondary ? Constants.textSecondary : Constants.textPrimary;
    }

    elide: Text.ElideRight
}
