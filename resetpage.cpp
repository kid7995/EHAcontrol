#include "resetpage.h"

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

ResetPage::ResetPage(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(22, 18, 22, 18);
    root->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("参数复位"), this);
    title->setObjectName(QStringLiteral("pageHeading"));
    auto *subtitle = new QLabel(
        QStringLiteral("将驱动器参数恢复为出厂值。此操作会覆盖当前全部参数设置。"), this);
    subtitle->setObjectName(QStringLiteral("pageSubtitle"));
    subtitle->setWordWrap(true);
    root->addWidget(title);
    root->addWidget(subtitle);

    auto *warningPanel = new QFrame(this);
    warningPanel->setObjectName(QStringLiteral("panel"));
    auto *warningLayout = new QVBoxLayout(warningPanel);
    warningLayout->setContentsMargins(22, 20, 22, 20);
    warningLayout->setSpacing(12);
    auto *warningTitle = new QLabel(QStringLiteral("高风险操作"), warningPanel);
    warningTitle->setObjectName(QStringLiteral("sectionTitle"));
    auto *warningText = new QLabel(
        QStringLiteral("参考版提示：参数复位会恢复出厂参数，机械与控制参数可能需要重新配置。\n\n"
                       "复位前应导出参数备份，确认设备下使能，并核对电机、编码器、"
                       "限位、控制环和现场安全状态。\n\n"
                       "参考复位帧：55 55 00 00 04 57 AA AA\n"
                       "复位完成后通常需要重新上电。"),
        warningPanel);
    warningText->setObjectName(QStringLiteral("fieldHint"));
    warningText->setWordWrap(true);
    auto *resetButton = new QPushButton(QStringLiteral("恢复出厂参数"), warningPanel);
    resetButton->setObjectName(QStringLiteral("dangerButton"));
    m_status = new QLabel(QStringLiteral("等待连接设备"), warningPanel);
    m_status->setObjectName(QStringLiteral("mutedText"));
    warningLayout->addWidget(warningTitle);
    warningLayout->addWidget(warningText);
    warningLayout->addStretch();
    warningLayout->addWidget(resetButton, 0, Qt::AlignLeft);
    warningLayout->addWidget(m_status);
    root->addWidget(warningPanel, 1);

    connect(resetButton, &QPushButton::clicked, this, &ResetPage::resetRequested);
}

void ResetPage::setConnected(bool connected)
{
    m_status->setText(connected ? QStringLiteral("设备已连接，可执行复位")
                                : QStringLiteral("等待连接设备"));
}
