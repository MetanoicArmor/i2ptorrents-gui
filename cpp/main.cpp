#include "mainwindow.hpp"

#include "app_constants.hpp"
#include "chrome.hpp"
#include "config.hpp"
#include "i18n.hpp"
#include "resources.hpp"
#include "theme.hpp"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QString::fromUtf8(i2p::APP_NAME));
    QApplication::setApplicationVersion(i2p::appVersion());

    const QString iconPath = i2p::applicationIconPath();
    if (!iconPath.isEmpty()) {
        app.setWindowIcon(QIcon(iconPath));
    }

    i2p::installRoundedTooltips();
    i2p::MainWindow window;
    return app.exec();
}
