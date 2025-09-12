#include "EasyDownloadManager.h"

#include <QApplication>
#include <qicon.h>

int main(int argc, char* argv[]) {
    QApplication::setStyle("Fusion");
    QApplication app(argc, argv);

    edm::EasyDownloadManager manager{};
    manager.showMainWindow();

    return app.exec();
}
