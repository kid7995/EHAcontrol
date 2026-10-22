#include "driveinfopage.h"

#include "parametercatalog.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

QFrame *panel(QWidget *parent)
{
    auto *frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("panel"));
    return frame;
}

QLabel *valueLabel(QWidget *parent)
{
    auto *label = new QLabel(QStringLiteral("--"), parent);
    label->setObjectName(QStringLiteral("infoValue"));
    return label;
}

void addInfoRow(QGridLayout *grid, int row, const QString &caption,
                QLabel *value, QWidget *parent)
{
    auto *captionLabel = new QLabel(caption, parent);
    captionLabel->setObjectName(QStringLiteral("infoCaption"));
    grid->addWidget(captionLabel, row, 0);
    grid->addWidget(value, row, 1);
}

} // namespace

DriveInfoPage::DriveInfoPage(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(14);

    auto *headingRow = new QHBoxLayout;
    auto *heading = new QVBoxLayout;
    auto *title = new QLabel(QStringLiteral("驱动器信息"), this);
    title->setObjectName(QStringLiteral("pageHeading"));
    auto *subtitle = new QLabel(
        QStringLiteral("设备身份、控制模式、关键标定参数与最近运行状态"), this);
    subtitle->setObjectName(QStringLiteral("pageSubtitle"));
    heading->addWidget(title);
    heading->addWidget(subtitle);
    headingRow->addLayout(heading);
    headingRow->addStretch();
    auto *readButton = new QPushButton(QStringLiteral("读取设备参数"), this);
    readButton->setObjectName(QStringLiteral("primaryButton"));
    headingRow->addWidget(readButton);
    root->addLayout(headingRow);

    auto *contentRow = new QHBoxLayout;
    contentRow->setSpacing(14);

    auto *identityPanel = panel(this);
    auto *identityLayout = new QVBoxLayout(identityPanel);
    identityLayout->setContentsMargins(20, 18, 20, 18);
    identityLayout->setSpacing(12);
    auto *deviceTitle = new QLabel(QStringLiteral("EHA / ROTARY ACTUATOR"), identityPanel);
    deviceTitle->setObjectName(QStringLiteral("deviceHero"));
    auto *deviceSub = new QLabel(
        QStringLiteral("串口伺服驱动器 · 200 项 Pn 参数工作集"), identityPanel);
    deviceSub->setObjectName(QStringLiteral("mutedText"));
    identityLayout->addWidget(deviceTitle);
    identityLayout->addWidget(deviceSub);

    auto *identityGrid = new QGridLayout;
    identityGrid->setHorizontalSpacing(18);
    identityGrid->setVerticalSpacing(10);
    m_connection = valueLabel(identityPanel);
    m_enableState = valueLabel(identityPanel);
    m_parameterCount = valueLabel(identityPanel);
    addInfoRow(identityGrid, 0, QStringLiteral("通信端口"), m_connection, identityPanel);
    addInfoRow(identityGrid, 1, QStringLiteral("使能状态"), m_enableState, identityPanel);
    addInfoRow(identityGrid, 2, QStringLiteral("参数缓存"), m_parameterCount, identityPanel);
    identityLayout->addLayout(identityGrid);

    auto *architecture = new QLabel(
        QStringLiteral("通信架构\n串口 115200 · 8N1 · 8 字节定长帧\n"
                       "握手 55 55 B9 9B 9B B9 AA AA\n"
                       "读取 55 55 FE FE EF EF AA AA"),
        identityPanel);
    architecture->setObjectName(QStringLiteral("infoCodeBlock"));
    architecture->setWordWrap(true);
    identityLayout->addWidget(architecture);
    identityLayout->addStretch();
    contentRow->addWidget(identityPanel, 1);

    auto *parameterPanel = panel(this);
    auto *parameterLayout = new QVBoxLayout(parameterPanel);
    parameterLayout->setContentsMargins(20, 18, 20, 18);
    parameterLayout->setSpacing(12);
    auto *parameterTitle = new QLabel(QStringLiteral("关键设备参数"), parameterPanel);
    parameterTitle->setObjectName(QStringLiteral("sectionTitle"));
    parameterLayout->addWidget(parameterTitle);

    auto *parameterGrid = new QGridLayout;
    parameterGrid->setHorizontalSpacing(18);
    parameterGrid->setVerticalSpacing(10);
    m_controlMode = valueLabel(parameterPanel);
    m_displayMode = valueLabel(parameterPanel);
    m_kValue = valueLabel(parameterPanel);
    m_bValue = valueLabel(parameterPanel);
    m_massCompensation = valueLabel(parameterPanel);
    m_lastPosition = valueLabel(parameterPanel);
    m_lastVoltage = valueLabel(parameterPanel);
    addInfoRow(parameterGrid, 0, QStringLiteral("控制模式 Pn160"), m_controlMode, parameterPanel);
    addInfoRow(parameterGrid, 1, QStringLiteral("显示模式 Pn002"), m_displayMode, parameterPanel);
    addInfoRow(parameterGrid, 2, QStringLiteral("标定增益 Pn168"), m_kValue, parameterPanel);
    addInfoRow(parameterGrid, 3, QStringLiteral("标定偏置 Pn169"), m_bValue, parameterPanel);
    addInfoRow(parameterGrid, 4, QStringLiteral("质量补偿 Pn170"), m_massCompensation, parameterPanel);
    addInfoRow(parameterGrid, 5, QStringLiteral("最新位置反馈"), m_lastPosition, parameterPanel);
    addInfoRow(parameterGrid, 6, QStringLiteral("最新母线电压"), m_lastVoltage, parameterPanel);
    parameterLayout->addLayout(parameterGrid);
    parameterLayout->addStretch();
    contentRow->addWidget(parameterPanel, 2);
    root->addLayout(contentRow, 1);

    connect(readButton, &QPushButton::clicked, this, &DriveInfoPage::readAllRequested);
    setConnected(false, QString());
}

void DriveInfoPage::setConnected(bool connected, const QString &portName)
{
    m_connected = connected;
    m_connection->setText(connected ? portName : QStringLiteral("未连接"));
}

void DriveInfoPage::updateTelemetry(const DeviceSerialService::Telemetry &telemetry)
{
    if (telemetry.statusKnown) {
        m_enableState->setText(telemetry.motorEnabled
                                   ? QStringLiteral("已使能")
                                   : QStringLiteral("未使能"));
    }
    m_lastPosition->setText(QString::number(telemetry.position, 'f', 0));
    m_lastVoltage->setText(QString::number(telemetry.busVoltage, 'f', 2)
                           + QStringLiteral(" V"));
}

void DriveInfoPage::setParameterValue(quint16 parameter, quint16 value)
{
    m_parameters.insert(parameter, value);
    refreshSummary();
}

void DriveInfoPage::setParameterReadCount(int count)
{
    m_parameterCount->setText(QStringLiteral("%1 / 200").arg(count));
}

void DriveInfoPage::refreshSummary()
{
    const auto rawValue = [this](quint16 parameter, const QString &fallback) {
        return m_parameters.contains(parameter)
            ? QString::number(m_parameters.value(parameter))
            : fallback;
    };

    const quint16 controlMode = m_parameters.value(160, 0xFFFF);
    if (controlMode == 0) {
        m_controlMode->setText(QStringLiteral("0 · 串口控制"));
    } else if (controlMode == 1) {
        m_controlMode->setText(QStringLiteral("1 · EtherCAT"));
    } else {
        m_controlMode->setText(QStringLiteral("未读取"));
    }

    const quint16 displayMode = m_parameters.value(2, 0xFFFF);
    if (displayMode == 79) {
        m_displayMode->setText(QStringLiteral("79 · 力/转矩反馈"));
    } else if (displayMode == 81) {
        m_displayMode->setText(QStringLiteral("81 · 位移量"));
    } else {
        m_displayMode->setText(displayMode == 0xFFFF
                                   ? QStringLiteral("未读取")
                                   : QString::number(displayMode));
    }

    m_kValue->setText(rawValue(168, QStringLiteral("未读取")));
    if (m_parameters.contains(169)) {
        m_bValue->setText(QString::number(
            ParameterCatalog::decodePn169(m_parameters.value(169))));
    } else {
        m_bValue->setText(QStringLiteral("未读取"));
    }
    m_massCompensation->setText(rawValue(170, QStringLiteral("未读取")));
}
