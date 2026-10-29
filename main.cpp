#include "mainwindow.h"

#include <QApplication>
#include <QFont>
#include <QPixmap>
#include <QSettings>
#include <QSplashScreen>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setApplicationName(QStringLiteral("KaiserStudio Qt"));
    QApplication::setOrganizationName(QStringLiteral("Saiwider"));
    QApplication::setApplicationVersion(QStringLiteral("0.2"));

    QFont font(QStringLiteral("Microsoft YaHei UI"), 9);
    QApplication::setFont(font);

    MainWindow w;
    QSettings settings;
    const bool showSplash = settings.value(QStringLiteral("ui/showSplash"), false).toBool();
    if (showSplash) {
        QPixmap splashPixmap(QStringLiteral(":/pic/image/logo.ico"));
        if (!splashPixmap.isNull()) {
            splashPixmap = splashPixmap.scaled(320, 320, Qt::KeepAspectRatio,
                                               Qt::SmoothTransformation);
            QSplashScreen splash(splashPixmap);
            splash.showMessage(QStringLiteral("KaiserStudio Qt · 正在加载驱动器工作台"),
                               Qt::AlignBottom | Qt::AlignHCenter, Qt::white);
            splash.show();
            QApplication::processEvents();
            QTimer::singleShot(900, &w, [&w, &splash]() {
                w.show();
                splash.finish(&w);
            });
            return a.exec();
        }
    }
    w.show();
    return a.exec();
}
