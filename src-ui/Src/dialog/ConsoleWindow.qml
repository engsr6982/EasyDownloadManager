import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../fluent"

import edm.qml.ui 1.0

/**
 * 虚拟控制台窗口：与主窗口一致的无边框 Fluent 外观（DWM 阴影 + 边缘拖拽缩放），
 * 独立非模态窗口，实时查看某个脚本的运行时日志。
 * 标题栏承载全部工具（自动滚动/清空/置顶/关闭）；日志区为终端风格深色底，
 * 与标题栏以直线分割；关闭窗口后日志仍持续入缓冲，重开即见全量历史。
 */
Window {
    id: consoleRoot

    property string scriptName: ""
    property string scriptVersion: ""
    property var logModel: null // QmlScriptManager.consoleModel(name)
    property bool pinned: false

    // 终端配色（黑底上需比浅色主题常量更高的明度才够对比）
    readonly property color termBg: "#1F1F1F"
    readonly property color termTime: "#9A9A9A"
    readonly property color termDebug: "#8C8C8C"
    readonly property color termInfo: "#4DA3FF"
    readonly property color termWarn: Constants.warning
    readonly property color termError: "#F14C4C"

    width: 720
    height: 480
    minimumWidth: 520
    minimumHeight: 320
    visible: false
    modality: Qt.NonModal
    // 无 transientParent：不参与主窗口的置顶栈与激活联动；生命周期由 QML 引擎托管
    transientParent: null
    // 不提供最小化：无边框窗口的最小化动画由系统随意摆放
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowSystemMenuHint
    color: Constants.windowBase

    Component.onCompleted: {
        consoleRoot.transientParent = null; // 覆盖组件完成期 Qt 的自动父窗口指派
        AppController.installWindowFrameless(consoleRoot);
    }

    function openFor(name, version) {
        consoleRoot.scriptName = name;
        consoleRoot.scriptVersion = version;
        consoleRoot.logModel = ScriptManager.consoleModel(name);
        followSwitch.checked = true;
        consoleRoot.show();
        consoleRoot.raise();
        Qt.callLater(consoleList.positionViewAtEnd);
    }

    function togglePinned() {
        consoleRoot.pinned = !consoleRoot.pinned;
        // flags 变更需重建平台窗口才生效；chrome 经 visibleChanged 重新施加
        consoleRoot.hide();
        consoleRoot.flags = consoleRoot.pinned
                            ? (consoleRoot.flags | Qt.WindowStaysOnTopHint)
                            : (consoleRoot.flags & ~Qt.WindowStaysOnTopHint);
        consoleRoot.show();
        consoleRoot.raise();
    }

    // ---- 标题栏（拖拽移动 + 工具 + 关闭） ----
    Rectangle {
        id: titlebar
        width: parent.width
        height: 36
        color: "transparent"

        MouseArea {
            anchors.fill: parent
            onPressed: consoleRoot.startSystemMove()
        }

        Text {
            text: qsTr("虚拟控制台 - %1%2").arg(consoleRoot.scriptName)
                  .arg(consoleRoot.scriptVersion.length > 0 ? "  v" + consoleRoot.scriptVersion : "")
            font.family: Constants.fontFamily
            font.pixelSize: Constants.fontSizeBody
            font.bold: true
            color: Constants.textPrimary
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            elide: Text.ElideRight
            width: parent.width - 260
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                text: qsTr("自动滚动")
                font.family: Constants.fontFamily
                font.pixelSize: Constants.fontSizeSecondary
                color: Constants.textSecondary
                anchors.verticalCenter: parent.verticalCenter

                MouseArea {
                    id: followLabelArea
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: followSwitch.checked = !followSwitch.checked
                }
            }

            FluSwitch {
                id: followSwitch
                showText: false
                checked: true
                // Row 只管水平排布，垂直位置需自行对齐
                anchors.verticalCenter: parent.verticalCenter
            }

            FluIconButton {
                text: "\uF202" // broom 清空
                onClicked: {
                    if (consoleRoot.scriptName.length > 0) {
                        ScriptManager.clearConsole(consoleRoot.scriptName);
                    }
                }

                ToolTip.visible: hovered
                ToolTip.text: qsTr("清空日志")
                ToolTip.delay: 400
            }

            FluIconButton {
                text: consoleRoot.pinned ? "\uF604" /* pin_off */ : "\uF602" /* pin */
                iconColor: consoleRoot.pinned ? Constants.accentPrimary : Constants.textPrimary
                onClicked: consoleRoot.togglePinned()

                ToolTip.visible: hovered
                ToolTip.text: consoleRoot.pinned ? qsTr("取消置顶") : qsTr("置顶窗口")
                ToolTip.delay: 400
            }

            Button {
                id: btnClose
                width: 32
                height: 28
                flat: true
                background: Rectangle {
                    color: btnClose.hovered ? Constants.danger : "transparent"
                    radius: Constants.radiusControl
                }
                contentItem: Text {
                    text: "\uF36A" // dismiss
                    font.family: Constants.iconFontFamily
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: btnClose.hovered ? Constants.bgContent : Constants.textPrimary
                }
                onClicked: consoleRoot.close()
            }
        }
    }

    // ---- 内容区（与标题栏以直线分割；终端区铺满） ----
    Rectangle {
        id: contentCard
        anchors.top: titlebar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        color: Constants.windowSecondaryBase
        border.width: 1
        border.color: Constants.borderSubtle

        Rectangle {
            anchors.fill: parent
            anchors.margins: Constants.paddingStandard
            radius: Constants.radiusCard
            color: consoleRoot.termBg
            clip: true

            ListView {
                id: consoleList
                anchors.fill: parent
                anchors.margins: 8
                clip: true
                model: consoleRoot.logModel
                spacing: 2
                flickDeceleration: 4000

                ScrollBar.vertical: ScrollBar {}

                // 新日志到达时若在底部则自动跟随（实时会话体验）
                onCountChanged: {
                    if (followSwitch.checked) {
                        Qt.callLater(consoleList.positionViewAtEnd);
                    }
                }

                delegate: Item {
                    width: consoleList.width
                    height: line.implicitHeight + 2

                    RowLayout {
                        anchors.fill: parent
                        spacing: 4

                        Text {
                            text: model.time
                            font.family: "Consolas, monospace"
                            font.pixelSize: 11
                            color: consoleRoot.termTime
                            Layout.preferredWidth: 70
                        }

                        Text {
                            text: {
                                switch (model.level) {
                                case ScriptLogModel.Error: return "[error]";
                                case ScriptLogModel.Warn: return "[warn]";
                                case ScriptLogModel.Debug: return "[debug]";
                                default: return "[info]";
                                }
                            }
                            font.family: "Consolas, monospace"
                            font.pixelSize: 11
                            font.bold: model.level === ScriptLogModel.Error
                            color: {
                                switch (model.level) {
                                case ScriptLogModel.Error: return consoleRoot.termError;
                                case ScriptLogModel.Warn: return consoleRoot.termWarn;
                                case ScriptLogModel.Debug: return consoleRoot.termDebug;
                                default: return consoleRoot.termInfo;
                                }
                            }
                            Layout.preferredWidth: 52
                        }

                        Text {
                            id: line
                            text: model.message
                            font.family: "Consolas, 'Microsoft YaHei', monospace"
                            font.pixelSize: 11
                            color: {
                                switch (model.level) {
                                case ScriptLogModel.Error: return consoleRoot.termError;
                                case ScriptLogModel.Warn: return consoleRoot.termWarn;
                                case ScriptLogModel.Debug: return consoleRoot.termDebug;
                                default: return "#E8E8E8";
                                }
                            }
                            wrapMode: Text.WrapAnywhere
                            Layout.fillWidth: true
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: consoleList.count === 0
                    text: qsTr("暂无日志")
                    font.family: Constants.fontFamily
                    font.pixelSize: Constants.fontSizeBody
                    color: consoleRoot.termDebug
                }
            }
        }
    }
}
