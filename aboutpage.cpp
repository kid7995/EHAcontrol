#include "aboutpage.h"

#include <QCoreApplication>
#include <QFrame>
#include <QLabel>
#include <QPixmap>
#include <QVBoxLayout>
#include <QtGlobal>

AboutPage::AboutPage(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(22, 18, 22, 18);
    root->setSpacing(14);

    auto *panel = new QFrame(this);
    panel->setObjectName(QStringLiteral("panel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(36, 32, 36, 32);
    layout->setSpacing(12);
    layout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    auto *logo = new QLabel(panel);
    const QPixmap pixmap(QStringLiteral(":/pic/image/logo.ico"));
    if (!pixmap.isNull()) {
        logo->setPixmap(pixmap.scaled(96, 96, Qt::KeepAspectRatio,
                                      Qt::SmoothTransformation));
        logo->setAlignment(Qt::AlignCenter);
        layout->addWidget(logo);
    }

    auto *title = new QLabel(QStringLiteral("KaiserStudio Qt"), panel);
    title->setObjectName(QStringLiteral("deviceHero"));
    title->setAlignment(Qt::AlignCenter);
    auto *version = new QLabel(
        QStringLiteral("Version %1 · Qt %2")
            .arg(QCoreApplication::applicationVersion(),
                 QString::fromLatin1(qVersion())),
        panel);
    version->setObjectName(QStringLiteral("mutedText"));
    version->setAlignment(Qt::AlignCenter);
    auto *description = new QLabel(
        QStringLiteral("面向旋转执行器与伺服驱动器的串口调试工作台。\n\n"
                       "功能包括参数读取与写入、实时反馈、示波器、控制环、"
                       "回零方式、故障诊断、数字 I/O 和参数备份恢复。\n\n"
                       "硬件动作和参数写入前，请确认设备处于安全状态。"),
        panel);
    description->setObjectName(QStringLiteral("pageSubtitle"));
    description->setWordWrap(true);
    description->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);
    layout->addWidget(version);
    layout->addWidget(description);
    root->addWidget(panel, 1);
}
