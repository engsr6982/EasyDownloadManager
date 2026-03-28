#include "MainWindow.h"
#include "../../event/EventBus.h"
#include "./ui_MainWindow.h"
#include "EdmApplication.h"
#include "database/DownloadDatabase.h"
#include "model/TaskModel.h"
#include "utils/IconUtils.h"
#include "utils/StringUtils.h"
#include "utils/Utils.h"

#include <QCloseEvent>
#include <QSystemTrayIcon>
#include <magic_enum/magic_enum.hpp>
#include <qfileiconprovider.h>
#include <qmenu.h>
#include <qstandarditemmodel.h>
#include <qtoolbar.h>

namespace edm {


MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), ui_(new Ui::MainWindow) {
    ui_->setupUi(this);

    _setupLayout(); // 初始化布局

    initDataFromDB();
}

MainWindow::~MainWindow() { delete ui_; }

void MainWindow::hideFileTree() const { ui_->fileTree_->setVisible(false); }

void MainWindow::showFileTree() const { ui_->fileTree_->setVisible(true); }

void MainWindow::initDataFromDB() {
    auto db = EdmApplication::getInstance().getDatabase();
    assert(db != nullptr);
    ui_->taskList_->setUpdatesEnabled(false);
    db->forEachTask([this](TaskModel const& task) {
        insertTask(task);
        return true;
    });
    ui_->taskList_->setUpdatesEnabled(true);
}

void MainWindow::insertTask(TaskModel const& task) {
    auto table = ui_->taskList_;

    int row = table->rowCount();
    table->insertRow(row);

    // "文件", "大小", "状态", "带宽", "剩余时间", "最后尝试"
    auto first = new QTableWidgetItem(string_utils::string2qstring(task.fileName));
    first->setData(Qt::UserRole, {task.id});
    table->setItem(row, 0, first);
    table->setItem(row, 1, new QTableWidgetItem(string_utils::string2qstring(utils::FileSize2String(task.fileSize))));
    table->setItem(row, 2, new QTableWidgetItem(string_utils::stringview2qstring(magic_enum::enum_name(task.state))));
    table->setItem(row, 3, new QTableWidgetItem(""));
    table->setItem(row, 4, new QTableWidgetItem(string_utils::string2qstring(utils::TimeStamp2String(task.firstTry))));
    table->setItem(row, 5, new QTableWidgetItem(string_utils::string2qstring(utils::TimeStamp2String(task.lastTry))));
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (auto tray = EdmApplication::getInstance().getTrayIcon(); tray && tray->isVisible()) {
        hide();
        event->ignore();
    } else {
        QMainWindow::closeEvent(event);
    }
}
void MainWindow::onRequestOpenTaskInfoDialog(int row) {
    auto col = ui_->taskList_->item(row, 0);
    if (!col) {
        return;
    }
    int  id = col->data(Qt::UserRole).toInt();
    emit EventBus::instance() -> onRequestOpenTaskInfoDialog(id);
}


void MainWindow::_setupLayout() {
    setWindowTitle("EasyDownloadManager"); // 设置窗口标题
    setMinimumSize(850, 500);              // 设置窗口最小尺寸

    _buildToolBar();
    _buildFileTree();
    _buildTaskList();
}
void MainWindow::_buildToolBar() const {
    auto toolBar = ui_->mainToolBar_;
    toolBar->setMovable(false); // 工具栏不可移动
    auto newTask    = toolBar->addAction("新建");
    auto resumeTask = toolBar->addAction("恢复");
    auto stopTask   = toolBar->addAction("停止");
    auto deleteTask = toolBar->addAction("删除");

    // 添加伸缩占位，把后面的动作推到右边 (右对齐)
    auto spacer = new QWidget(toolBar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolBar->addWidget(spacer);

    auto settings = toolBar->addAction("设置");

    // 连接信号
    connect(newTask, &QAction::triggered, EventBus::instance(), &EventBus::onRequestOpenNewTaskDialog);
    connect(resumeTask, &QAction::triggered, this, []() {
        qDebug() << "[MainWindow] onResumeTask";
        // TODO: impl
    });
    connect(stopTask, &QAction::triggered, this, []() {
        qDebug() << "[MainWindow] onStopTask";
        // TODO: impl
    });
    connect(deleteTask, &QAction::triggered, this, []() {
        qDebug() << "[MainWindow] onDeleteTask";
        // TODO: impl
    });
    connect(settings, &QAction::triggered, EventBus::instance(), &EventBus::onRequestOpenSettingDialog);
}
void MainWindow::_buildFileTree() {
    auto tree = ui_->fileTree_;
    tree->setFixedWidth(145);    // 固定宽度
    tree->setHeaderHidden(true); // 隐藏表头

    tree->clear(); // 清空已有节点

    // 顶层类别
    auto topCategories = QStringList{"所有任务", "进行中", "已完成", "已取消"};
    using TypeWithIcon = std::pair<QString, QIcon>;
    auto subCategories = std::array<TypeWithIcon, 6>{
        TypeWithIcon{      "视频",      icon_utils::getIcon(icon_utils::IconType::VideoFile)},
        TypeWithIcon{      "音频",      icon_utils::getIcon(icon_utils::IconType::AudioFile)},
        TypeWithIcon{  "压缩文件", icon_utils::getIcon(icon_utils::IconType::CompressedFile)},
        TypeWithIcon{      "文档",   icon_utils::getIcon(icon_utils::IconType::DocumentFile)},
        TypeWithIcon{"可执行程序",    icon_utils::getIcon(icon_utils::IconType::Application)},
        TypeWithIcon{      "其它",           icon_utils::getIcon(icon_utils::IconType::File)},
    };

    auto const folderIcon = icon_utils::getIcon(icon_utils::IconType::Folder);

    bool isFirst = true;
    for (const auto& top : topCategories) {
        auto* topItem = new QTreeWidgetItem(tree);
        topItem->setText(0, top);
        topItem->setIcon(0, folderIcon);

        // 添加子类别
        for (const auto& sub : subCategories) {
            auto* subItem = new QTreeWidgetItem(topItem);
            subItem->setText(0, sub.first);
            subItem->setIcon(0, sub.second);
        }

        if (isFirst) {
            isFirst = false;
            topItem->setExpanded(true); // 默认展开第一个顶层类别
        }
    }

    // 点击进行过滤操作
    connect(tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int column) {
        Q_UNUSED(column);
        QString category = item->text(0);
        // TODO: impl

        qDebug() << "[FileTree] Clicked category: " << category;
    });
}
void MainWindow::_buildTaskList() {
    auto list = ui_->taskList_;

    // 基本属性
    QStringList headers = {"文件", "大小", "状态", "带宽", "剩余时间", "最后尝试"};
    list->setColumnCount(static_cast<int>(headers.size()));
    list->setHorizontalHeaderLabels(headers); // 设置表头

    list->setSelectionBehavior(QAbstractItemView::SelectRows);     // 整行选中
    list->setEditTriggers(QAbstractItemView::NoEditTriggers);      // 禁止编辑
    list->setAlternatingRowColors(true);                           // 交替行背景
    list->setSortingEnabled(true);                                 // 允许排序
    list->verticalHeader()->setVisible(false);                     // 隐藏行号
    list->horizontalHeader()->setStretchLastSection(true);         // 最后一列拉伸填满
    list->sortItems(list->columnCount() - 1, Qt::DescendingOrder); // 默认按最后尝试时间降序排序
    list->verticalHeader()->setDefaultSectionSize(20);             // 设置行高

    list->setStyleSheet(R"(
        QTableWidget::item:selected {
            background-color: #D0E8FF; /* 淡蓝色 */
            color: black;              /* 保证文字可读 */
        }
    )");

    // 双击打开任务信息窗口
    connect(list, &QTableWidget::cellDoubleClicked, this, [this](int row, int column) {
        Q_UNUSED(column);
        onRequestOpenTaskInfoDialog(row);
    });

    // 自动设置首列的文件 Icon
    connect(list, &QTableWidget::itemChanged, this, [list](QTableWidgetItem* item) {
        if (item->column() == 0 && !item->icon().isNull() == false) {
            item->setIcon(icon_utils::EdmIconProvider::getIconWithFileExtension(item->text()));
        }
    });

    // 右键菜单
    list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(list, &QTableWidget::customContextMenuRequested, this, [this, list](const QPoint& pos) {
        QMenu menu;
        menu.addAction("暂停/恢复", [this, list, pos]() {
            // TODO: impl
        });
        menu.addAction("重新下载", [this, list, pos]() {
            // TODO: impl
        });
        menu.addAction("显示窗口", [this, list, pos]() {
            // TODO: impl
        });
        menu.addSeparator();
        menu.addAction("打开", [this, list]() {
            // TODO: impl
        });
        menu.addAction("打开文件夹", [this, list]() {
            // TODO: impl
        });
        menu.addSeparator();
        menu.addAction("删除", [this, list]() {
            // TODO: impl
        });
        menu.addAction("彻底删除", [this, list]() {
            // TODO: impl
        });
        menu.addSeparator();
        menu.addAction("属性", [this, list]() { onRequestOpenTaskInfoDialog(list->currentRow()); });

        menu.exec(list->viewport()->mapToGlobal(pos));
    });
}


} // namespace edm