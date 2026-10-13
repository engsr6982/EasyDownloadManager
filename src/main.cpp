#include "EdmApplication.h"

#include <QApplication>
#include <QQuickStyle>
#include <QSurfaceFormat>


namespace {}

int main(int argc, char* argv[]) {
    QQuickStyle::setStyle("Basic");
    QApplication qtApp(argc, argv);

    auto& application = edm::EdmApplication::getInstance();
    application.initialize();

    int code = QApplication::exec();

    application.shutdown();
    return code;
}
