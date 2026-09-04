import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../fluent"

import edm.qml.ui 1.0

Item {
    id: root

    // 闭合时 x 处于父容器的最右侧（隐藏），展开时向左偏移出自身宽度
    x: parent ? (parent.width - (opened ? width : 0)) : 0
    y: 0
    height: parent ? parent.height : 0

    Behavior on x {
        NumberAnimation {
            duration: 250
            easing.type: Easing.OutQuad
        }
    }

    property int currentTaskId: 0       // 当前选中的任务 ID（基本类型，不受 ListView 回收影响）
    property var taskDetail: null       // QmlTaskDetail live object from C++
    property bool opened: false

    function open() {
        opened = true;
    }

    function close() {
        opened = false;
    }

    // 当 currentTaskId 变化时，直接从 C++ 获取最新的 QmlTaskDetail
    onCurrentTaskIdChanged: {
        if (currentTaskId > 0) {
            taskDetail = TaskListModel.getTaskDetail(currentTaskId);
        } else {
            taskDetail = null;
        }
    }

    // 动态计算坐标，使其完全覆盖 contentContainer 区域
    MouseArea {
        id: clickOutsideDismisser
        x: root.parent && root.parent.parent ? (-root.parent.x - root.x) : 0
        y: root.parent && root.parent.parent ? (-root.parent.y - root.y) : 0
        width: root.parent && root.parent.parent ? root.parent.parent.width : 0
        height: root.parent && root.parent.parent ? root.parent.parent.height : 0

        enabled: root.opened // 仅在抽屉打开时激活，平时完全处于停用状态，避免不必要的性能消耗
        propagateComposedEvents: true

        onPressed: mouse => {
            root.close();
            mouse.accepted = false; // 拒绝该事件，让点击无损向下传递并直接触发底层按钮或项
        }
    }

    // 左侧渐变阴影（仅在左边缘外侧呈现）
    Rectangle {
        id: leftShadow
        anchors.right: backgroundRect.left
        anchors.top: backgroundRect.top
        anchors.bottom: backgroundRect.bottom
        width: 16 // 阴影的渲染宽度

        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop {
                position: 0.0
                color: "transparent"
            }
            GradientStop {
                position: 0.7
                color: "#06000000"
            } // 模拟自然的光强指数级衰减
            GradientStop {
                position: 1.0
                color: "#1E000000"
            } // 贴近抽屉边缘的最深投影区
        }

        // 阴影随抽屉开关同步进行渐变淡入淡出
        opacity: root.opened ? 1.0 : 0.0
        Behavior on opacity {
            NumberAnimation {
                duration: 200
                easing.type: Easing.OutQuad
            }
        }
    }

    // 抽屉主体背板
    Rectangle {
        id: backgroundRect
        anchors.fill: parent
        color: Constants.bgContent
        border.color: Constants.borderSubtle
        border.width: 1
        clip: true // 对溢出元素进行裁剪

        // 拦截面板内部的所有点击，防止点击面板内部时事件意外触发外部关闭逻辑
        MouseArea {
            anchors.fill: parent
            propagateComposedEvents: false
            onClicked: mouse => mouse.accepted = true
        }

        // ==================== 整体纵向布局容器 ====================
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12 // 统一由最外层控制 padding
            spacing: 12

            // 标题栏：固定占位，不参与弹性缩放
            RowLayout {
                id: subtitlebar
                Layout.fillWidth: true
                spacing: 8

                FluIconButton {
                    id: closeBtn
                    text: "\uF36A"
                    onClicked: root.close()
                }
                FluText {
                    text: qsTr("任务详情")
                    font.bold: true
                    isTitle: true
                    Layout.alignment: Qt.AlignVCenter
                }

                // 弹簧
                Item {
                    Layout.fillWidth: true
                    Layout.columnSpan: 1 // 占位
                }

                Row {
                    // 暂停 / 继续
                    FluIconButton {
                        text: {
                            var s = taskDetail ? taskDetail.statusStr : "N/A";
                            if (s === "Running")  return "\uf5a2";
                            return "\uf606";
                        }
                        enabled: {
                            var s = taskDetail ? taskDetail.statusStr : "N/A";
                            return s === "Running" || s === "Paused" || s === "Pending" || s === "Failed";
                        }
                        onClicked: {
                            if (currentTaskId <= 0) return;
                            var id = currentTaskId;
                            var s = taskDetail ? taskDetail.statusStr : "N/A";
                            if (s === "Running") {
                                TaskListModel.pauseTask(id);
                            } else {
                                TaskListModel.resumeTask(id);
                            }
                        }
                    }
                    // 打开文件夹
                    FluIconButton {
                        text: "\uF583"
                        onClicked: {
                            if (taskDetail && taskDetail.saveDir !== "") {
                                AppController.openFolder(taskDetail.saveDir);
                            }
                        }
                    }
                }
            }

            // 滚动内容区
            ScrollView {
                id: contentScrollView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ScrollBar.vertical: ScrollBar {
                    parent: contentScrollView
                    policy: ScrollBar.AlwaysOff
                    visible: false
                }

                // 真正的内部扁平容器
                ColumnLayout {
                    width: contentScrollView.availableWidth
                    spacing: 20

                    // 下载状态板
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        RowLayout {
                            spacing: 6
                            FluIndicator {
                                active: true
                                animate: false
                            }
                            FluText {
                                text: qsTr("下载状态")
                                font.bold: true
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: statusGrid.implicitHeight + 24
                            color: Qt.alpha(Constants.bgContent, 0.4)
                            border.color: Constants.borderSubtle
                            border.width: 1
                            radius: Constants.radiusCard

                            GridLayout {
                                id: statusGrid
                                anchors.fill: parent
                                anchors.margins: 12
                                columns: 2
                                columnSpacing: 0

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: parent.width / 2
                                    spacing: 4

                                    FluText {
                                        text: qsTr("当前速率")
                                        color: "#808080"
                                    }
                                    FluText {
                                        text: taskDetail ? taskDetail.speedStr : "N/A"
                                        font.pixelSize: 18
                                        font.bold: true
                                        isAccent: true
                                        elide: Text.ElideRight
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: parent.width / 2
                                    spacing: 4

                                    FluText {
                                        text: qsTr("剩余时间")
                                        color: "#808080"
                                    }
                                    FluText {
                                        text: taskDetail ? taskDetail.remainingTimeStr : "---"
                                        font.pixelSize: 16
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }

                        // 线程状态子卡片
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 150
                            border.color: Constants.borderSubtle
                            border.width: 1
                            radius: Constants.radiusCard
                            color: "transparent"

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 0

                                    FluText {
                                        text: qsTr("线程")
                                        font.bold: true
                                        Layout.preferredWidth: 48
                                    }
                                    FluText {
                                        text: qsTr("状态")
                                        font.bold: true
                                        Layout.fillWidth: true
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: Constants.borderSubtle
                                }

                                ListView {
                                    id: threadListView
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    clip: true

                                    ScrollBar.vertical: ScrollBar {
                                        policy: ScrollBar.AlwaysOff
                                        visible: false
                                    }

                                    model: taskDetail ? taskDetail.rangesModel : null

                                    delegate: Rectangle {
                                        id: threadStateElement
                                        width: threadListView.width
                                        height: 24
                                        radius: Constants.radiusCard

                                        MouseArea {
                                            id: threadStateElementMouseArea
                                            anchors.fill: parent
                                            hoverEnabled: true
                                        }

                                        color: threadStateElementMouseArea.containsMouse ? Constants.standardHover : "transparent"

                                        RowLayout {
                                            anchors.fill: parent
                                            spacing: 0
                                            anchors.leftMargin: 6
                                            anchors.rightMargin: 6

                                            FluText {
                                                text: "# " + (rangeIndex !== undefined ? rangeIndex : (index + 1))
                                                Layout.preferredWidth: 42
                                            }
                                            FluText {
                                                text: rangeStatus !== undefined ? rangeStatus : ""
                                                Layout.fillWidth: true
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // 任务信息板
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        RowLayout {
                            spacing: 6
                            FluIndicator {
                                active: true
                                animate: false
                            }
                            FluText {
                                text: qsTr("任务信息")
                                font.bold: true
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: infoGrid.implicitHeight + 24
                            color: Qt.alpha(Constants.bgContent, 0.4)
                            border.color: Constants.borderSubtle
                            border.width: 1
                            radius: Constants.radiusCard

                            GridLayout {
                                id: infoGrid
                                anchors.fill: parent
                                anchors.margins: 12
                                columns: 2
                                rowSpacing: 12
                                columnSpacing: 8

                                FluText {
                                    text: qsTr("文件名：")
                                    color: "#808080"
                                    Layout.alignment: Qt.AlignTop
                                }
                                FluText {
                                    text: taskDetail ? taskDetail.fileName : "N/A"
                                    Layout.fillWidth: true
                                    wrapMode: Text.WrapAnywhere
                                }

                                FluText {
                                    text: qsTr("文件体积：")
                                    color: "#808080"
                                }
                                FluText {
                                    text: taskDetail ? taskDetail.fileSizeStr : "N/A"
                                }

                                FluText {
                                    text: qsTr("存储路径：")
                                    color: "#808080"
                                    Layout.alignment: Qt.AlignTop
                                }
                                FluText {
                                    text: taskDetail ? taskDetail.saveDir : "N/A"
                                    Layout.fillWidth: true
                                    wrapMode: Text.WrapAnywhere
                                }

                                FluText {
                                    text: qsTr("首次操作：")
                                    color: "#808080"
                                }
                                FluText {
                                    text: taskDetail && taskDetail.firstTryStr !== "" ? taskDetail.firstTryStr : "N/A"
                                }

                                FluText {
                                    text: qsTr("最近操作：")
                                    color: "#808080"
                                }
                                FluText {
                                    text: taskDetail && taskDetail.lastTryStr !== "" ? taskDetail.lastTryStr : "N/A"
                                }

                                FluText {
                                    text: qsTr("下载链接：")
                                    color: "#808080"
                                    Layout.alignment: Qt.AlignTop
                                }
                                FluText {
                                    text: taskDetail ? taskDetail.urlStr : "N/A"
                                    Layout.fillWidth: true
                                    wrapMode: Text.WrapAnywhere
                                    elide: Text.ElideRight
                                    maximumLineCount: 3
                                }

                                FluText {
                                    text: qsTr("分类：")
                                    color: "#808080"
                                }
                                FluText {
                                    text: taskDetail ? taskDetail.categoryStr : "N/A"
                                }

                                FluText {
                                    text: qsTr("MIME 类型：")
                                    color: "#808080"
                                }
                                FluText {
                                    text: taskDetail ? taskDetail.mimeType : "N/A"
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                FluText {
                                    text: qsTr("来源页面：")
                                    color: "#808080"
                                    Layout.alignment: Qt.AlignTop
                                }
                                FluText {
                                    text: taskDetail ? taskDetail.referrer : "N/A"
                                    Layout.fillWidth: true
                                    wrapMode: Text.WrapAnywhere
                                    elide: Text.ElideRight
                                    maximumLineCount: 2
                                }

                                FluText {
                                    text: qsTr("断点续传：")
                                    color: "#808080"
                                }
                                FluText {
                                    text: taskDetail ? (taskDetail.resumable ? qsTr("支持") : qsTr("不支持")) : qsTr("未知")
                                }

                                FluText {
                                    text: qsTr("错误信息：")
                                    color: taskDetail && taskDetail.errorMsg !== "" ? Constants.danger : "#808080"
                                    Layout.alignment: Qt.AlignTop
                                    visible: taskDetail && taskDetail.errorMsg !== ""
                                }
                                FluText {
                                    text: taskDetail ? taskDetail.errorMsg : ""
                                    color: Constants.danger
                                    Layout.fillWidth: true
                                    wrapMode: Text.WrapAnywhere
                                    visible: taskDetail && taskDetail.errorMsg !== ""
                                }
}
                        }
                    }
                }
            }
        }
    }
}
