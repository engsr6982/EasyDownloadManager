import QtQuick

ListView {
    id: control

    interactive: false
    boundsBehavior: Flickable.StopAtBounds

    property real wheelTargetY: 0

    function scrollByWheel(delta) {
        topAnimation.stop();
        if (!wheelAnimation.running)
            wheelTargetY = contentY;

        var maxY = Math.max(0, contentHeight - height);
        var targetY = Math.max(0, Math.min(maxY, wheelTargetY - delta));
        if (targetY === wheelTargetY)
            return;

        wheelTargetY = targetY;
        wheelAnimation.restart();
    }

    function scrollToTop() {
        wheelAnimation.stop();
        wheelTargetY = 0;
        topAnimation.restart();
    }

    SmoothedAnimation {
        id: wheelAnimation
        target: control
        property: "contentY"
        to: control.wheelTargetY
        velocity: 1600
        maximumEasingTime: 120
        reversingMode: SmoothedAnimation.Sync
    }

    NumberAnimation {
        id: topAnimation
        target: control
        property: "contentY"
        to: 0
        duration: 180
        easing.type: Easing.OutCubic
    }

    WheelHandler {
        onWheel: function(event) {
            var delta = event.pixelDelta.y;
            if (event.phase === Qt.NoScrollPhase || delta === 0)
                delta = event.angleDelta.y * 0.5;
            control.scrollByWheel(delta);
            event.accepted = true;
        }
    }
}
