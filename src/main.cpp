#include "EasyDownloadManager.h"
#include "ui/main/MainWindow.h"

#include <QApplication>
#include <QSurfaceFormat>

int main(int argc, char* argv[]) {
    QApplication::setStyle("Fusion");
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);     // 使用 OpenGL
    format.setProfile(QSurfaceFormat::CoreProfile);       // 核心模式
    format.setVersion(4, 6);                              // OpenGL 版本
    format.setDepthBufferSize(24);                        // 深度缓冲
    format.setStencilBufferSize(8);                       // 模板缓冲
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer); // 双缓冲
    QSurfaceFormat::setDefaultFormat(format);             // 全局默认格式

    QApplication app(argc, argv);

    edm::EasyDownloadManager::getOrNewInstance().getMainWindow()->show();
    int code = app.exec();
    edm::EasyDownloadManager::tryDestroyInstance();
    return code;
}
