#include "homingpage.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

QLabel *panelTitle(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("sectionTitle"));
    return label;
}

} // namespace

HomingPage::HomingPage(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(22, 18, 22, 18);
    root->setSpacing(12);

    auto *headingRow = new QHBoxLayout;
    auto *heading = new QVBoxLayout;
    auto *title = new QLabel(QStringLiteral("回零方式与轨迹"), this);
    title->setObjectName(QStringLiteral("pageHeading"));
    auto *subtitle = new QLabel(
        QStringLiteral("依据 Homing method (0x6098) 展示挡片、Index、原点开关和限位开关的动作时序。"),
        this);
    subtitle->setObjectName(QStringLiteral("pageSubtitle"));
    subtitle->setWordWrap(true);
    heading->addWidget(title);
    heading->addWidget(subtitle);
    headingRow->addLayout(heading, 1);

    m_methodCombo = new QComboBox(this);
    m_methodCombo->setMinimumWidth(360);
    addMethod(1, QStringLiteral("离开负限位后，第一个 Index 标记回零"));
    addMethod(2, QStringLiteral("离开正限位后，第一个 Index 标记回零"));
    addMethod(7, QStringLiteral("离开原点开关负边沿后，第一个 Index 标记回零，初始正向运动"));
    addMethod(8, QStringLiteral("接触原点开关负边沿后，第一个 Index 标记回零，初始正向运动"));
    addMethod(9, QStringLiteral("接触原点开关正边沿后，第一个 Index 标记回零，初始正向运动"));
    addMethod(10, QStringLiteral("离开原点开关正边沿后，第一个 Index 标记回零，初始正向运动"));
    addMethod(11, QStringLiteral("离开原点开关正边沿后，第一个 Index 标记回零，初始负向运动"));
    addMethod(12, QStringLiteral("接触原点开关正边沿后，第一个 Index 标记回零，初始负向运动"));
    addMethod(13, QStringLiteral("接触原点开关负边沿后，第一个 Index 标记回零，初始负向运动"));
    addMethod(14, QStringLiteral("离开原点开关负边沿后，第一个 Index 标记回零，初始负向运动"));
    addMethod(24, QStringLiteral("接触原点开关负边上升沿回零，初始正向运动"));
    addMethod(28, QStringLiteral("接触原点开关正边上升沿回零，初始负向运动"));
    addMethod(33, QStringLiteral("第一个 Index 标记回零，初始负向运动"));
    addMethod(34, QStringLiteral("第一个 Index 标记回零，初始正向运动"));
    addMethod(35, QStringLiteral("声明当前位置为零位"));
    headingRow->addWidget(m_methodCombo);
    root->addLayout(headingRow);

    auto *contentRow = new QHBoxLayout;
    contentRow->setSpacing(12);

    auto *legendPanel = new QFrame(this);
    legendPanel->setObjectName(QStringLiteral("panel"));
    legendPanel->setFixedWidth(275);
    auto *legendLayout = new QVBoxLayout(legendPanel);
    legendLayout->setContentsMargins(16, 15, 16, 15);
    legendLayout->setSpacing(10);
    legendLayout->addWidget(panelTitle(QStringLiteral("回零方式说明"), legendPanel));
    m_description = new QLabel(legendPanel);
    m_description->setWordWrap(true);
    m_description->setObjectName(QStringLiteral("mutedText"));
    legendLayout->addWidget(m_description);

    auto *legendText = new QLabel(
        QStringLiteral("图例\n"
                       "粗实线：第一回零速度，用于寻找开关\n"
                       "细箭头：第二回零速度，用于寻找零位\n"
                       "Index：编码器每圈零位标记\n"
                       "HOME：机械原点开关\n"
                       "NOT / POT：负 / 正限位开关\n\n"
                       "回零前应确认机械行程、开关极性和旋转方向。"),
        legendPanel);
    legendText->setObjectName(QStringLiteral("fieldHint"));
    legendText->setWordWrap(true);
    legendLayout->addWidget(legendText);

    auto *commandText = new QLabel(
        QStringLiteral("参考程序固定帧\n"
                       "开始：55 55 1C C1 C1 1C AA AA\n"
                       "停止：55 55 C1 CD CD C1 AA AA\n\n"
                       "当前版本仅展示轨迹和命令依据，未自动执行运动。"),
        legendPanel);
    commandText->setObjectName(QStringLiteral("infoCodeBlock"));
    commandText->setWordWrap(true);
    legendLayout->addWidget(commandText);
    legendLayout->addStretch();
    contentRow->addWidget(legendPanel);

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    auto *imageContainer = new QWidget(m_scroll);
    imageContainer->setObjectName(QStringLiteral("homingImageContainer"));
    auto *imageLayout = new QVBoxLayout(imageContainer);
    imageLayout->setContentsMargins(10, 8, 10, 8);
    imageLayout->setSpacing(12);
    m_image = new QLabel(imageContainer);
    m_image->setAlignment(Qt::AlignCenter);
    m_image->setMinimumHeight(330);
    m_textImage = new QLabel(imageContainer);
    m_textImage->setAlignment(Qt::AlignCenter);
    imageLayout->addWidget(m_image);
    imageLayout->addWidget(m_textImage);
    imageLayout->addStretch();
    m_scroll->setWidget(imageContainer);
    contentRow->addWidget(m_scroll, 1);
    root->addLayout(contentRow, 1);

    connect(m_methodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HomingPage::updateMethod);
    updateMethod();
}

void HomingPage::updateMethod()
{
    const int method = m_methodCombo->currentData().toInt();
    m_description->setText(descriptionForMethod(method));

    const QPixmap diagram(QStringLiteral(":/reference/homing/hom%1.png").arg(method));
    m_image->setPixmap(diagram.scaled(760, 390, Qt::KeepAspectRatio,
                                      Qt::SmoothTransformation));

    const QPixmap explanation(
        QStringLiteral(":/reference/homing/homtext%1.png").arg(method));
    m_textImage->setPixmap(explanation.scaled(760, 260, Qt::KeepAspectRatio,
                                              Qt::SmoothTransformation));
    m_textImage->setVisible(!explanation.isNull());
}

void HomingPage::addMethod(int value, const QString &description)
{
    m_methodCombo->addItem(QStringLiteral("%1 · %2").arg(value).arg(description), value);
}

QString HomingPage::descriptionForMethod(int method) const
{
    return m_methodCombo->itemText(m_methodCombo->findData(method));
}
