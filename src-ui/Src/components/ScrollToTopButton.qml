pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import ".."
import "../fluent"

FluIconButton {
    id: control

    required property Flickable flickable

    anchors.right: parent.right
    anchors.bottom: parent.bottom
    anchors.rightMargin: 12
    anchors.bottomMargin: 12
    width: 36
    height: 36
    z: 10
    iconSize: 18
    text: "\uF19B" // arrow_up_20_regular
    visible: flickable.contentHeight > flickable.height
    enabled: flickable.contentY > 1
    opacity: flickable.contentY > 1 ? 1.0 : 0.0
    scale: flickable.contentY > 1 ? 1.0 : 0.85
    transformOrigin: Item.BottomRight

    Accessible.ignored: !enabled

    Behavior on opacity {
        NumberAnimation {
            duration: 180
            easing.type: Easing.OutCubic
        }
    }

    Behavior on scale {
        NumberAnimation {
            duration: 180
            easing.type: Easing.OutCubic
        }
    }

    background: Item {
        Rectangle {
            id: buttonPlate
            anchors.fill: parent
            color: control.hovered ? Constants.navHover : Constants.bgContent
            radius: Constants.radiusControl
            border.color: Constants.borderSubtle
            border.width: 1
        }

        MultiEffect {
            source: buttonPlate
            anchors.fill: buttonPlate
            autoPaddingEnabled: true
            shadowEnabled: true
            shadowColor: Constants.bgShadowColor
            shadowBlur: Constants.bgShadowBlur
            shadowVerticalOffset: Constants.bgShadowVerticalOffset
        }
    }

    ToolTip.visible: hovered && enabled
    ToolTip.text: qsTr("回到顶部")
    ToolTip.delay: 400

    Accessible.name: qsTr("回到顶部")

    onClicked: {
        flickable.cancelFlick();
        if (typeof flickable.scrollToTop === "function") {
            flickable.scrollToTop();
        } else {
            scrollAnimation.restart();
        }
    }

    NumberAnimation {
        id: scrollAnimation
        target: control.flickable
        property: "contentY"
        to: 0
        duration: 180
        easing.type: Easing.BezierCurve
        easing.bezierCurve: [0.16, 1, 0.3, 1]
    }
}
