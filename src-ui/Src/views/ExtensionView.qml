import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import ".."
import "../components"
import "../fluent"

import edm.qml.ui 1.0

/**
 * 插件中心：工具栏（导入 / 全部重载 / 打开脚本目录）+ 扩展卡片列表，
 * 每个扩展独立启停/重载/卸载；虚拟控制台以独立非模态窗口查看实时运行日志。
 */
Item {
    id: root
    anchors.fill: parent

    signal showUninstallPluginDialog(string name, string dirPath)

    // 导入脚本包（选择包含 manifest.json 的目录）
    FolderDialog {
        id: importPicker
        onAccepted: ScriptManager.importScript(AppController.removeFileUrlPrefix(selectedFolder))
    }

    // 扩展卡片更多操作菜单（视图层单实例，定位与防溢出由 FluMenu.popup 处理）
    FluMenu {
        id: scriptContextMenu
        property string targetName: ""
        property string targetVersion: ""
        property string targetDir: ""

        FluMenuItem {
            bindMenu: scriptContextMenu
            iconText: "\uEE6F"
            text: qsTr("虚拟控制台")
            onTriggered: root.openConsole(scriptContextMenu.targetName, scriptContextMenu.targetVersion)
        }

        FluMenuItem {
            bindMenu: scriptContextMenu
            iconText: "\uF42F"
            text: qsTr("打开文件夹")
            onTriggered: ScriptManager.openScriptDir(scriptContextMenu.targetName)
        }

        FluMenuSeparator {}

        FluMenuItem {
            bindMenu: scriptContextMenu
            iconText: "\uf34d"
            text: qsTr("卸载插件")
            isDanger: true
            onTriggered: root.showUninstallPluginDialog(scriptContextMenu.targetName, scriptContextMenu.targetDir)
        }
    }

    function openConsole(name, version) {
        AppController.openConsole(name, version);
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Constants.itemSpacing

        // 标题控件行
        RowLayout {
            id: toolbar
            Layout.fillWidth: true
            Layout.preferredHeight: Constants.viewTitlebarHeight

            Text {
                text: qsTr("插件中心")
                font.family: Constants.fontFamily
                font.pixelSize: Constants.fontSizeTitle
                font.bold: true
                color: Constants.textPrimary
            }

            Item {
                Layout.fillWidth: true
            }

            FluIconButton {
                text: "\uF137" // apps_add_in 导入脚本包
                onClicked: importPicker.open()

                ToolTip.visible: hovered
                ToolTip.text: qsTr("导入脚本")
                ToolTip.delay: 400
            }

            FluIconButton {
                text: "\uF13E" // arrow_clockwise 全部重载
                onClicked: ScriptManager.reloadAll()

                ToolTip.visible: hovered
                ToolTip.text: qsTr("全部重载")
                ToolTip.delay: 400
            }

            FluIconButton {
                text: "\uF583" // open 打开脚本目录
                onClicked: ScriptManager.openScriptsDir()

                ToolTip.visible: hovered
                ToolTip.text: qsTr("打开脚本目录")
                ToolTip.delay: 400
            }
        }

        // 视图内容区
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: cardList
                anchors.fill: parent
                clip: true
                model: ScriptManager
                spacing: Constants.itemSpacing
                cacheBuffer: 240
                boundsBehavior: Flickable.StopAtBounds
                // 不放可见滚动条：overlay 会遮挡卡片右缘；滚轮与惯性滚动不受影响

                delegate: ExtensionCard {
                    width: cardList.width
                    name: model.name
                    version: model.version
                    author: model.author
                    description: model.description
                    dirPath: model.dir
                    scriptState: model.state
                    errorText: model.errorText
                    iconUrl: model.icon

                    onToggleRequested: (name, enabled) => ScriptManager.setEnabled(name, enabled)
                    onReloadRequested: (name) => ScriptManager.reloadScript(name)
                    onConsoleRequested: (name) => root.openConsole(name, version)
                    onMoreRequested: (name, version, targetItem, x, y) => {
                        scriptContextMenu.targetName = name;
                        scriptContextMenu.targetVersion = version;
                        scriptContextMenu.targetDir = dirPath;
                        scriptContextMenu.popup(targetItem, x, y);
                    }
                }

                // 空状态
                Text {
                    anchors.centerIn: parent
                    visible: cardList.count === 0
                    text: qsTr("未发现脚本扩展\n\n将包含 manifest.json 的脚本文件夹放入脚本目录，或点击右上角导入")
                    horizontalAlignment: Text.AlignHCenter
                    font.family: Constants.fontFamily
                    font.pixelSize: Constants.fontSizeBody
                    color: Constants.textDisabled
                }
            }
        }
    }

    // 操作失败提示（转发应用日志）
    Connections {
        target: ScriptManager
        function onScriptActionError(action, name, message) {
            console.warn("[ScriptManager]", action, name, message);
        }
    }

    Component.onCompleted: ScriptManager.refresh()
}
