#include "mainwindow.h"

#include <QApplication>
#include <QFont>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setApplicationName(QStringLiteral("KaiserStudio Qt"));
    QApplication::setOrganizationName(QStringLiteral("Saiwider"));
    QApplication::setApplicationVersion(QStringLiteral("0.2"));

    QFont font(QStringLiteral("Microsoft YaHei UI"), 9);
    QApplication::setFont(font);

    MainWindow w;
    w.show();
    return a.exec();
}
