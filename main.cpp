#include "mainwindow.h"

#include <QApplication>
#include <QNetworkProxyFactory>
#include <QNetworkProxy>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QNetworkProxy proxy;
    proxy.setType(QNetworkProxy::NoProxy); // 设置代理类型为“无代理”
    QNetworkProxy::setApplicationProxy(proxy); // 将此“无代理”对象设置为应用程序的默认代理

    MainWindow w;
    w.show();
    return a.exec();
}
