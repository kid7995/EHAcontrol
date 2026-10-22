#include "dashboardpage.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QStyle>
#include <QVariant>
#include <QVBoxLayout>

namespace {

QFrame *panel(QWidget *parent = nullptr)
{
    auto *frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("panel"));
    return frame;
}

QLabel *sectionTitle(const QString &text, QWidget *parent = nullptr)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("sectionTitle"));
    return label;
}

} // namespace

DashboardPage::DashboardPage(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(22, 18, 22, 18);
    root->setSpacing(12);

    auto *headingRow = new QHBoxLayout;
    auto *headingText = new QVBoxLayout;
    auto *title = new QLabel(QStringLiteral("实时设备概览"), this);
    title->setObjectName(QStringLiteral("pageHeading"));
    auto *subtitle = new QLabel(
        QStringLiteral("串口在线状态、运动反馈与常用设备命令集中显示"), this);
    subtitle->setObjectName(QStringLiteral("pageSubtitle"));
    headingText->addWidget(title);
    headingText->addWidget(subtitle);
    headingText->addStretch();
    headingRow->addLayout(headingText);
    headingRow->addStretch();

    m_readStatusLabel = new QLabel(QStringLiteral("尚未读取设备参数"), this);
    m_readStatusLabel->setObjectName(QStringLiteral("statusPill"));
    headingRow->addWidget(m_readStatusLabel, 0, Qt::AlignTop);
    root->addLayout(headingRow);

    auto *metricsGrid = new QGridLayout;
    metricsGrid->setHorizontalSpacing(12);
    metricsGrid->setVerticalSpacing(12);
    m_position = addMetric(metricsGrid, 0, 0, QStringLiteral("实际位置"), QStringLiteral("count"),
                           QStringLiteral("#2563EB"));
    m_multiTurn = addMetric(metricsGrid, 0, 1, QStringLiteral("多圈位置"), QStringLiteral("count"),
                            QStringLiteral("#0F766E"));
    m_singleTurn = addMetric(metricsGrid, 0, 2, QStringLiteral("单圈位置"), QStringLiteral("count"),
                             QStringLiteral("#7C3AED"));
    m_speed = addMetric(metricsGrid, 0, 3, QStringLiteral("速度"), QStringLiteral("rpm"),
                        QStringLiteral("#EA580C"));
    m_current = addMetric(metricsGrid, 1, 0, QStringLiteral("电流"), QStringLiteral("A"),
                          QStringLiteral("#0891B2"));
    m_busVoltage = addMetric(metricsGrid, 1, 1, QStringLiteral("母线电压"), QStringLiteral("V"),
                             QStringLiteral("#4F46E5"));
    m_force = addMetric(metricsGrid, 1, 2, QStringLiteral("力/转矩反馈"), QStringLiteral("raw"),
                        QStringLiteral("#DC2626"));
    m_state = addMetric(metricsGrid, 1, 3, QStringLiteral("设备状态"), QString(),
                        QStringLiteral("#475569"));
    root->addLayout(metricsGrid);

    auto *lowerRow = new QHBoxLayout;
    lowerRow->setSpacing(16);

    auto *devicePanel = panel(this);
    auto *deviceLayout = new QVBoxLayout(devicePanel);
    deviceLayout->setContentsMargins(20, 18, 20, 18);
    deviceLayout->setSpacing(12);
    deviceLayout->addWidget(sectionTitle(QStringLiteral("连接与安全状态"), devicePanel));
    m_connectionLabel = new QLabel(QStringLiteral("串口未连接"), devicePanel);
    m_connectionLabel->setObjectName(QStringLiteral("deviceStatusRow"));
    m_motorLabel = new QLabel(QStringLiteral("电机使能：未知"), devicePanel);
    m_motorLabel->setObjectName(QStringLiteral("deviceStatusRow"));
    m_faultLabel = new QLabel(QStringLiteral("故障代码：--"), devicePanel);
    m_faultLabel->setObjectName(QStringLiteral("deviceStatusRow"));
    m_lastUpdateLabel = new QLabel(QStringLiteral("最近数据：--"), devicePanel);
    m_lastUpdateLabel->setObjectName(QStringLiteral("mutedText"));
    deviceLayout->addWidget(m_connectionLabel);
    deviceLayout->addWidget(m_motorLabel);
    deviceLayout->addWidget(m_faultLabel);
    deviceLayout->addWidget(m_lastUpdateLabel);
    deviceLayout->addStretch();

    auto *openButton = new QPushButton(QStringLiteral("打开串口连接"), devicePanel);
    openButton->setObjectName(QStringLiteral("primaryButton"));
    connect(openButton, &QPushButton::clicked,
            this, &DashboardPage::openCommunicationRequested);
    deviceLayout->addWidget(openButton);
    lowerRow->addWidget(devicePanel, 1);

    auto *actionPanel = panel(this);
    auto *actionLayout = new QVBoxLayout(actionPanel);
    actionLayout->setContentsMargins(20, 18, 20, 18);
    actionLayout->setSpacing(12);
    actionLayout->addWidget(sectionTitle(QStringLiteral("快捷操作"), actionPanel));
    auto *actionText = new QLabel(
        QStringLiteral("读取全部 Pn 参数，或对设备执行使能与安全停止。"), actionPanel);
    actionText->setObjectName(QStringLiteral("mutedText"));
    actionText->setWordWrap(true);
    actionLayout->addWidget(actionText);

    auto *buttonRow = new QHBoxLayout;
    m_enableButton = new QPushButton(QStringLiteral("使能"), actionPanel);
    m_enableButton->setObjectName(QStringLiteral("successButton"));
    m_disableButton = new QPushButton(QStringLiteral("禁用"), actionPanel);
    m_disableButton->setObjectName(QStringLiteral("dangerButton"));
    m_readButton = new QPushButton(QStringLiteral("读取全部参数"), actionPanel);
    m_readButton->setObjectName(QStringLiteral("primaryButton"));
    buttonRow->addWidget(m_enableButton);
    buttonRow->addWidget(m_disableButton);
    buttonRow->addWidget(m_readButton);
    actionLayout->addLayout(buttonRow);

    auto *saveButton = new QPushButton(QStringLiteral("保存当前参数到 EEPROM（Pn152）"), actionPanel);
    saveButton->setObjectName(QStringLiteral("secondaryButton"));
    actionLayout->addWidget(saveButton);
    actionLayout->addStretch();
    lowerRow->addWidget(actionPanel, 2);

    root->addLayout(lowerRow, 1);

    connect(m_readButton, &QPushButton::clicked, this, &DashboardPage::readAllRequested);
    connect(m_enableButton, &QPushButton::clicked, this, &DashboardPage::enableRequested);
    connect(m_disableButton, &QPushButton::clicked, this, &DashboardPage::disableRequested);
    connect(saveButton, &QPushButton::clicked, this, &DashboardPage::saveRequested);

    setConnectionState(false, QString());
}

DashboardPage::MetricWidgets DashboardPage::addMetric(
    QGridLayout *layout, int row, int column, const QString &title,
    const QString &unit, const QString &accent)
{
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("metricCard"));
    card->setStyleSheet(QStringLiteral(
        "QFrame#metricCard { border-top: 3px solid %1; }").arg(accent));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(14, 10, 14, 10);
    cardLayout->setSpacing(5);

    auto *titleLabel = new QLabel(title, card);
    titleLabel->setObjectName(QStringLiteral("metricTitle"));
    auto *valueRow = new QHBoxLayout;
    auto *valueLabel = new QLabel(QStringLiteral("--"), card);
    valueLabel->setObjectName(QStringLiteral("metricValue"));
    auto *unitLabel = new QLabel(unit, card);
    unitLabel->setObjectName(QStringLiteral("metricUnit"));
    valueRow->addWidget(valueLabel);
    valueRow->addWidget(unitLabel, 0, Qt::AlignBottom);
    valueRow->addStretch();
    auto *noteLabel = new QLabel(QStringLiteral("等待设备数据"), card);
    noteLabel->setObjectName(QStringLiteral("metricNote"));

    cardLayout->addWidget(titleLabel);
    cardLayout->addLayout(valueRow);
    cardLayout->addWidget(noteLabel);
    layout->addWidget(card, row, column);
    return {valueLabel, noteLabel};
}

void DashboardPage::setConnectionState(bool connected, const QString &portName)
{
    if (connected) {
        m_connectionLabel->setText(QStringLiteral("串口已连接：%1").arg(portName));
        m_connectionLabel->setProperty("state", QStringLiteral("online"));
        m_readStatusLabel->setText(QStringLiteral("通信在线"));
        m_readStatusLabel->setProperty("state", QStringLiteral("online"));
        m_enableButton->setEnabled(true);
        m_disableButton->setEnabled(true);
        m_readButton->setEnabled(true);
    } else {
        m_connectionLabel->setText(QStringLiteral("串口未连接"));
        m_connectionLabel->setProperty("state", QStringLiteral("offline"));
        m_readStatusLabel->setText(QStringLiteral("通信离线"));
        m_readStatusLabel->setProperty("state", QStringLiteral("offline"));
        m_enableButton->setEnabled(false);
        m_disableButton->setEnabled(false);
        m_readButton->setEnabled(false);
    }
    m_connectionLabel->style()->unpolish(m_connectionLabel);
    m_connectionLabel->style()->polish(m_connectionLabel);
    m_readStatusLabel->style()->unpolish(m_readStatusLabel);
    m_readStatusLabel->style()->polish(m_readStatusLabel);
}

void DashboardPage::updateTelemetry(const DeviceSerialService::Telemetry &telemetry)
{
    setMetric(m_position, telemetry.position);
    setMetric(m_multiTurn, telemetry.multiTurnPosition);
    setMetric(m_singleTurn, telemetry.singleTurnPosition);
    setMetric(m_speed, telemetry.speed, 1);
    setMetric(m_current, telemetry.current, 2);
    setMetric(m_busVoltage, telemetry.busVoltage, 2);
    setMetric(m_force, telemetry.force);

    m_position.note->setText(QStringLiteral("反馈帧 0x0C"));
    m_multiTurn.note->setText(QStringLiteral("反馈帧 0x0A"));
    m_singleTurn.note->setText(QStringLiteral("反馈帧 0x0B"));
    m_speed.note->setText(QStringLiteral("反馈帧 0x09"));
    m_current.note->setText(QStringLiteral("反馈帧 0x08 · 原始值 × 0.01"));
    m_busVoltage.note->setText(QStringLiteral("反馈帧 0x1A"));
    m_force.note->setText(QStringLiteral("反馈帧 0x07"));
    m_state.note->setText(telemetry.valid ? QStringLiteral("数据帧有效") : QStringLiteral("等待数据"));

    if (telemetry.timestamp.isValid()) {
        m_lastUpdateLabel->setText(
            QStringLiteral("最近数据：%1").arg(
                telemetry.timestamp.toString(QStringLiteral("HH:mm:ss.zzz"))));
    }
}

void DashboardPage::setMotorEnabled(bool enabled)
{
    m_motorLabel->setText(enabled ? QStringLiteral("电机使能：已使能")
                                  : QStringLiteral("电机使能：未使能"));
    setMetric(m_state, enabled ? QStringLiteral("运行") : QStringLiteral("待机"));
    m_state.note->setText(enabled ? QStringLiteral("设备已进入可控状态")
                                  : QStringLiteral("设备保持安全停止状态"));
}

void DashboardPage::setFaultCode(quint32 faultCode)
{
    m_faultLabel->setText(faultCode == 0
                              ? QStringLiteral("故障代码：无")
                              : QStringLiteral("故障代码：0x%1")
                                    .arg(faultCode, 8, 16, QLatin1Char('0')).toUpper());
}

void DashboardPage::setReadProgress(int received, int total)
{
    m_readStatusLabel->setText(
        QStringLiteral("参数读取 %1/%2").arg(received).arg(total));
}

void DashboardPage::setParameterReadFinished(bool complete, int received)
{
    if (complete) {
        m_readStatusLabel->setText(QStringLiteral("参数读取完成 · %1 项").arg(received));
        m_readStatusLabel->setProperty("state", QStringLiteral("online"));
    } else {
        m_readStatusLabel->setText(QStringLiteral("参数读取超时 · %1 项").arg(received));
        m_readStatusLabel->setProperty("state", QStringLiteral("warning"));
    }
    m_readStatusLabel->style()->unpolish(m_readStatusLabel);
    m_readStatusLabel->style()->polish(m_readStatusLabel);
}

void DashboardPage::setMetric(MetricWidgets &metric, double value, int decimals)
{
    metric.value->setText(QString::number(value, 'f', decimals));
}

void DashboardPage::setMetric(MetricWidgets &metric, const QString &value)
{
    metric.value->setText(value);
}
