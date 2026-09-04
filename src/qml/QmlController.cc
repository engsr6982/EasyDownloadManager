#include "QmlController.h"

#include "EdmApplication.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QQuickWindow>
#include <QUrl>
#include <qstringview.h>

namespace edm {

QmlController::QmlController(QObject* parent) : QObject(parent) {}

void QmlController::minimizeToTray() {
    auto* mainWin = EdmApplication::getInstance().getMainWindow();
    if (mainWin) {
        mainWin->hide();
    }
}

bool QmlController::openFile(QString const& path) {
    if (path.isEmpty()) return false;
    QUrl url = QUrl::fromLocalFile(path);
    if (!QDesktopServices::openUrl(url)) {
        emit errorOccurred(QStringLiteral("Failed to open file: ") + path);
        return false;
    }
    return true;
}

bool QmlController::openFolder(QString const& path) {
    if (path.isEmpty()) return false;
    QUrl url = QUrl::fromLocalFile(path);
    if (!QDesktopServices::openUrl(url)) {
        emit errorOccurred(QStringLiteral("Failed to open folder: ") + path);
        return false;
    }
    return true;
}

QString QmlController::clipboardText() const { return QApplication::clipboard()->text(); }

QString QmlController::removeFileUrlPrefix(QString const& url) {
    // Since QUrl can't be called directly in QML,
    //  here's a helper to convert file URLs in QML
    return QUrl{url}.toLocalFile();
}

} // namespace edm
