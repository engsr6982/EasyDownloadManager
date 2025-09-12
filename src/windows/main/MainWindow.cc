#include "MainWindow.h"
#include "./ui_MainWindow.h"
#include "utils/IconUtils.h"

#include <qfileiconprovider.h>
#include <qmenu.h>
#include <qstandarditemmodel.h>
#include <qtoolbar.h>

namespace edm {


MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), ui_(new Ui::MainWindow) {
    ui_->setupUi(this);

    _setupLayout(); // 初始化布局

#ifdef EDM_DEBUG
    _addDebugDatas();
#endif
}

MainWindow::~MainWindow() { delete ui_; }

void MainWindow::hideFileTree() const { ui_->fileTree_->setVisible(false); }

void MainWindow::showFileTree() const { ui_->fileTree_->setVisible(true); }

void MainWindow::_addDebugDatas() const {
    auto list = ui_->taskList_;
    list->setRowCount(2);
    list->setItem(0, 0, new QTableWidgetItem("示例文件.mp4"));
    list->setItem(0, 1, new QTableWidgetItem("700 PB"));
    list->setItem(0, 2, new QTableWidgetItem("进行中"));
    list->setItem(0, 3, new QTableWidgetItem("12 TB/s"));
    list->setItem(0, 4, new QTableWidgetItem("00:12:34"));
    list->setItem(0, 5, new QTableWidgetItem("2025-09-12 22:00"));

    list->setItem(1, 0, new QTableWidgetItem("explorer.exe"));
    list->setItem(1, 1, new QTableWidgetItem("11mb"));
    list->setItem(1, 2, new QTableWidgetItem("进行中"));
    list->setItem(1, 3, new QTableWidgetItem("12 kb/s"));
    list->setItem(1, 4, new QTableWidgetItem("00:1:34"));
    list->setItem(1, 5, new QTableWidgetItem("2025-09-12 23:00"));
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
    toolBar->addAction("新建");
    toolBar->addAction("恢复");
    toolBar->addAction("停止");
    toolBar->addAction("删除");
    toolBar->addAction("清除所有");

    // 添加伸缩占位，把后面的动作推到右边 (右对齐)
    auto spacer = new QWidget(toolBar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolBar->addWidget(spacer);

    toolBar->addAction("设置");
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
        TypeWithIcon{      "视频",             icon_utils::getIcon(icon_utils::IconType::VideoFile)},
        TypeWithIcon{      "音频",             icon_utils::getIcon(icon_utils::IconType::AudioFile)},
        TypeWithIcon{  "压缩文件",        icon_utils::getIcon(icon_utils::IconType::CompressedFile)},
        TypeWithIcon{      "文档",          icon_utils::getIcon(icon_utils::IconType::DocumentFile)},
        TypeWithIcon{"可执行程序", icon_utils::getIcon(icon_utils::IconType::ExecutableProgramFile)},
        TypeWithIcon{      "其它",                  icon_utils::getIcon(icon_utils::IconType::File)},
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

    list->setSelectionBehavior(QAbstractItemView::SelectRows); // 整行选中
    list->setEditTriggers(QAbstractItemView::NoEditTriggers);  // 禁止编辑
    list->setAlternatingRowColors(true);                       // 交替行背景
    list->setSortingEnabled(true);                             // 允许排序
    list->verticalHeader()->setVisible(false);                 // 隐藏行号
    list->horizontalHeader()->setStretchLastSection(true);     // 最后一列拉伸填满

    list->sortItems(list->columnCount() - 1, Qt::DescendingOrder); // 默认按最后尝试时间降序排序

    // 双击打开任务信息窗口
    connect(list, &QTableWidget::cellDoubleClicked, this, [this](int row, int column) {
        Q_UNUSED(column);
        // TODO: 调用打开任务信息窗口
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
        menu.addAction("属性", [this, list]() {
            // TODO: impl
        });

        menu.exec(list->viewport()->mapToGlobal(pos));
    });
}


} // namespace edm