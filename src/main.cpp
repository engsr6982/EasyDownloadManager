#include "EdmApplication.h"

#include <QApplication>
#include <QSurfaceFormat>
#include <QQuickStyle>

int main(int argc, char* argv[]) {
    QQuickStyle::setStyle("Basic");
    QApplication qtApp(argc, argv);

    auto& application = edm::EdmApplication::getInstance();
    application.initApp();

    int code = QApplication::exec();

    application.destroyApp();
    return code;
}
