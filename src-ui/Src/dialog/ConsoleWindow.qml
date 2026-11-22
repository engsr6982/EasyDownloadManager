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
 * 整块富文本按行渲染（等级着色在 ScriptLogModel 统一定义），支持选择与 Ctrl+C 复制；
 * 关闭窗口后日志仍持续入缓冲，重开即见全量历史。
 */
Window {
    id: consoleRoot

    property string scriptName: ""
    property string scriptVersion: ""
    property var logModel: null // QmlScriptManager.consoleModel(name)
    property bool pinned: false

    // 终端配色（等级色板定义在 ScriptLogModel::logHtml）
    readonly property color termBg: "#1F1F1F"

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
    // 任务栏标题：随脚本切换同步更新
    title: scriptName.length > 0
           ? qsTr("虚拟控制台 - %1%2").arg(scriptName)
                 .arg(scriptVersion.length > 0 ? "  v" + scriptVersion : "")
           : qsTr("虚拟控制台")

    Component.onCompleted: {
        consoleRoot.transientParent = null; // 覆盖组件完成期 Qt 的自动父窗口指派
        AppController.installWindowFrameless(consoleRoot);
        // C++ 侧 QIcon/QFile 按文件名处理，资源路径须用 ":/" 短形式而非 "qrc:/" URL 形式
        AppController.setWindowIcon(consoleRoot, ":/assets/icons/console.png");
    }

    // 供外部验证渲染结果（logEdit 在组件内部作用域）
    readonly property int renderedLength: logEdit.length

    function openFor(name, version) {
        consoleRoot.scriptName = name;
        consoleRoot.scriptVersion = version;
        consoleRoot.logModel = ScriptManager.consoleModel(name);
        // 打开与切换脚本都立即渲染当前缓冲；contentsChanged 只负责后续增量
        logEdit.text = consoleRoot.logModel ? consoleRoot.logModel.logHtml() : "";
        followSwitch.checked = true;
        consoleRoot.show();
        consoleRoot.raise();
        Qt.callLater(consoleRoot.scrollToEnd);
    }

    function scrollToEnd() {
        logFlick.contentY = Math.max(0, logEdit.contentHeight - logFlick.height);
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

    // 模型变更：重建富文本并按需跟随滚动
    Connections {
        target: consoleRoot.logModel
        function onContentsChanged() {
            logEdit.text = consoleRoot.logModel ? consoleRoot.logModel.logHtml() : "";
            if (followSwitch.checked) {
                Qt.callLater(consoleRoot.scrollToEnd);
            }
        }
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

            Flickable {
                id: logFlick
                anchors.fill: parent
                anchors.margins: 8
                contentWidth: width
                contentHeight: logEdit.contentHeight
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar {}

                TextEdit {
                    id: logEdit
                    width: logFlick.width
                    readOnly: true
                    selectByMouse: true
                    activeFocusOnPress: false
                    textFormat: TextEdit.RichText
                    wrapMode: TextEdit.WrapAnywhere
                    color: "#E8E8E8"
                    // 选中高亮沿用终端信息色
                    selectionColor: "#4DA3FF"
                    selectedTextColor: consoleRoot.termBg
                    font.family: "Consolas, 'Microsoft YaHei', monospace"
                    font.pixelSize: 11
                }
            }

            Text {
                anchors.centerIn: parent
                visible: logEdit.length === 0
                text: qsTr("暂无日志")
                font.family: Constants.fontFamily
                font.pixelSize: Constants.fontSizeBody
                color: "#8C8C8C"
            }
        }
    }
}
