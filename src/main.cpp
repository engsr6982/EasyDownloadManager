#include "EasyDownloadManager.h"
#include "ui/main/MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication::setStyle("Fusion");
    QApplication app(argc, argv);

    edm::EasyDownloadManager::getOrNewInstance().getMainWindow()->show();
    int code = app.exec();
    edm::EasyDownloadManager::tryDestroyInstance();
    return code;
}
