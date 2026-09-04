pragma Singleton
import QtQuick

Item {
    readonly property string projectName: "EasyDownloadManager"

    // load icons from FluentSystemIcons-Regular.ttf
    FontLoader {
        id: fluentIconLoader
        source: Qt.resolvedUrl("fonts/FluentSystemIcons-Regular.ttf")
    }
    readonly property string iconFontFamily: fluentIconLoader.name
    readonly property int iconFontSize: 16

    // Background Colors
    readonly property color windowBase: "#F0F5F8"
    readonly property color windowSecondaryBase: "#F8FBFD"
    readonly property color sidebar: "transparent"
    readonly property color bgContent: "#FFFFFF"
    readonly property color dialogOverlay: "#80000000"

    readonly property color bgDisabled: "#F3F2F1"

    readonly property real opacityStandard: 1.0
    readonly property real opacityDisabled: 0.45

    readonly property color navHover: "#E2E7E9"

    // Accent Colors
    readonly property color accentPrimary: "#005A9E" //  "#0078D4"
    readonly property color accentHover: "#1A6BA8"// "#006CC1"
    readonly property color accentPressed: "#337BB1" // "#005499"
    readonly property color textAccent: "#005A9E"

    readonly property color indicatorSelected: Constants.accentPrimary // 状态指示器(蓝条)

    // Standard Colors
    readonly property color standardPrimary: "#FEFEFE"
    readonly property color standardHover: "#F0F0F0" // #FBFBFB
    readonly property color standardPressed: "#FCFCFC"

    // Text & Border Colors
    readonly property color textPrimary: "#323130"
    readonly property color textSecondary: "#5F5F5F"
    readonly property color textOnAccent: "#FFFFFF"
    readonly property color borderSubtle: "#E4E9EB" // #E5E5E5
    readonly property color borderCard: "#EBEBEB"

    readonly property color textDisabled: '#605E5C'
    readonly property color borderDisabled: "#E1DFDD"

    // Typography
    FontLoader {
        id: interFontLoader
        source: Qt.resolvedUrl("fonts/SourceHanSansCN-Regular.otf")
    }
    readonly property string fontFamily: interFontLoader.name + ", Microsoft YaHei, sans-serif"
    readonly property int fontSizeTitle: 16
    readonly property int fontSizeBody: 12
    readonly property int fontSizeSecondary: 11

    // Geometry
    readonly property real radiusWindow: 8.0
    readonly property real radiusCard: 8.0
    readonly property real radiusControl: 4.0

    // Border & Shadow
    readonly property color shadowAccentBottom: "#00365F" // Accent 按钮的沉淀线
    readonly property color shadowStandardBottom: "#EDEDED" // 普通按钮的沉淀线

    readonly property color bgShadowColor: "#28000000" // 背景阴影
    readonly property real bgShadowBlur: 0.8 // 背景阴影模糊度
    readonly property real bgShadowVerticalOffset: 6 // 背景阴影垂直偏移

    // Layout Spacing
    readonly property real marginStandard: 16.0
    readonly property real paddingStandard: 12.0
    readonly property real itemSpacing: 8.0

    readonly property real viewTitlebarHeight: 32.0

    // Animations
    readonly property int animDurationNormal: 250

    // Status Colors
    readonly property color success: "#107C41"
    readonly property color warning: "#FFB900"
    readonly property color danger: "#E81123"
    readonly property color dangerHover: "#EC2F3F"
    readonly property color dangerPressed: "#C50F1F"
    readonly property color dangerShadowBottom: "#8B0A15"
}
