#include "EasyDownloadManager.h"
#include "windows/main/MainWindow.h"

#include <QApplication>
#include <qicon.h>

int main(int argc, char* argv[]) {
    QApplication::setStyle("Fusion");
    QApplication app(argc, argv);

    edm::EasyDownloadManager::getOrNewInstance().getMainWindow()->show();

    return app.exec();
}
