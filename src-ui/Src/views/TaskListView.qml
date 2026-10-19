import QtQuick
import QtQuick.Layouts
import ".."
import "../fluent"
import "../components"

import edm.qml.ui 1.0

Item {
    id: root
    anchors.fill: parent

    signal taskSelected(int taskId)
    signal showNewTaskDialog
    signal showDeleteTaskDialog(int taskId)

    // 右键菜单
    FluMenu {
        id: taskContextMenu
        property int targetTaskId: 0
        property var targetDetail: null
        property string targetStatus: targetDetail ? (targetDetail.statusStr || "") : ""

        FluMenuItem {
            bindMenu: taskContextMenu
            text: (taskContextMenu.targetStatus === "Running" || taskContextMenu.targetStatus === "Preparing")
                    ? qsTr("暂停") : qsTr("继续")
            iconText: (taskContextMenu.targetStatus === "Running" || taskContextMenu.targetStatus === "Preparing") ? "\uf5a2" : "\uf606"
            enabled: {
                var s = taskContextMenu.targetStatus;
                return s === "Running" || s === "Paused" || s === "Pending" || s === "Failed"
                    || s === "Preparing";
            }
            onTriggered: {
                if (taskContextMenu.targetTaskId <= 0) return;
                var s = taskContextMenu.targetStatus;
                if (s === "Running" || s === "Preparing") {
                    TaskListModel.pauseTask(taskContextMenu.targetTaskId);
                } else {
                    TaskListModel.resumeTask(taskContextMenu.targetTaskId);
                }
            }
        }

        FluMenuItem {
            bindMenu: taskContextMenu
            iconText: "\uF13E"
            text: qsTr("重新下载")
            enabled: {
                var s = taskContextMenu.targetStatus;
                return s === "Finished" || s === "Failed" || s === "Paused" || s === "Canceled";
            }
            onTriggered: {
                if (taskContextMenu.targetTaskId > 0) {
                    TaskListModel.redownloadTask(taskContextMenu.targetTaskId);
                }
            }
        }

        FluMenuSeparator {}

        FluMenuItem {
            bindMenu: taskContextMenu
            iconText: "\uF583"
            text: qsTr("打开文件")
            enabled: taskContextMenu.targetStatus === "Finished"
                && taskContextMenu.targetDetail
                && taskContextMenu.targetDetail.saveDir !== ""
                && taskContextMenu.targetDetail.fileName !== ""
            onTriggered: {
                if (taskContextMenu.targetTaskId <= 0) return;
                var detail = taskContextMenu.targetDetail;
                if (detail && detail.saveDir !== "" && detail.fileName !== "") {
                    AppController.openFile(detail.saveDir + "/" + detail.fileName);
                }
            }
        }

        FluMenuItem {
            bindMenu: taskContextMenu
            iconText: "\uF42F"
            text: qsTr("打开文件夹")
            enabled: taskContextMenu.targetDetail && taskContextMenu.targetDetail.saveDir !== ""
            onTriggered: {
                if (taskContextMenu.targetTaskId <= 0) return;
                var detail = taskContextMenu.targetDetail;
                if (detail && detail.saveDir !== "") {
                    AppController.openFolder(detail.saveDir);
                }
            }
        }

        FluMenuSeparator {}

        FluMenuItem {
            bindMenu: taskContextMenu
            iconText: "\uf34d"
            text: qsTr("删除任务")
            isDanger: true
            onTriggered: root.showDeleteTaskDialog(taskContextMenu.targetTaskId)
        }
    }

    // 列布局
    ColumnLayout {
        anchors.fill: parent
        spacing: Constants.itemSpacing

        // 标题控件行
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Constants.viewTitlebarHeight

            Text {
                text: qsTr("下载任务")
                font.family: Constants.fontFamily
                font.pixelSize: Constants.fontSizeTitle
                font.bold: true
                color: Constants.textPrimary
            }

            Item {
                Layout.fillWidth: true
            }

            // TODO: 基于 Dropdown 的 tag 过滤器
            // 下载任务<-----------> [过滤器] [+新建任务]
            //                      | 准备中/下载中/暂停/取消/失败/完成
            //                      | 视频/音频/文档/程序/压缩文件/其它

            FluButton {
                isAccent: true
                text: qsTr("新建任务")
                iconText: "\uF151"
                onClicked: root.showNewTaskDialog()
            }
        }

        // 任务列表
        ListView {
            id: taskListView
            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: Constants.itemSpacing
            clip: true
            reuseItems: true

            model: TaskListModel

            delegate: TaskCard {
                index: model.index
                taskId: model.taskId
                fileName: model.fileName
                fileSizeStr: model.fileSizeStr
                progressValue: model.progressValue
                speedStr: model.speedStr
                statusStr: model.statusStr
                urlStr: model.urlStr
                saveDir: model.saveDir
                referrer: model.referrer
                timestamp: model.timestamp

                onPauseClicked: function (id) {
                    var status = model.statusStr;
                    // Running/Preparing 才是在跑（可暂停）；Pending/Paused/Failed 都是"没在跑"（可继续）

                    if (status === "Running" || status === "Preparing") {
                        TaskListModel.pauseTask(id);
                    } else if (status === "Pending" || status === "Paused" || status === "Failed") {
                        TaskListModel.resumeTask(id);
                    }
                }

                onDeleteClicked: function (id) {
                    root.showDeleteTaskDialog(id);
                }

                onRightClicked: function (taskId, targetItem, mouseX, mouseY) {
                    taskContextMenu.targetTaskId = taskId;
                    taskContextMenu.targetDetail = TaskListModel.getTaskDetail(taskId);
                    taskContextMenu.popup(targetItem, mouseX, mouseY);
                }

                onCardClicked: {
                    root.taskSelected(model.taskId);
                }
            }
        }
    }
}
