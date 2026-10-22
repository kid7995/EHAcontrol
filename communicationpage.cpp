#include "communicationpage.h"

#include <QComboBox>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QStyle>
#include <QVariant>
#include <QVBoxLayout>

namespace {

QFrame *panel(QWidget *parent)
{
    auto *frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("panel"));
    return frame;
}

QLabel *sectionTitle(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("sectionTitle"));
    return label;
}

} // namespace

CommunicationPage::CommunicationPage(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(22, 18, 22, 18);
    root->setSpacing(12);

    auto *header = new QHBoxLayout;
    auto *heading = new QVBoxLayout;
    auto *title = new QLabel(QStringLiteral("串口通信中心"), this);
    title->setObjectName(QStringLiteral("pageHeading"));
    auto *subtitle = new QLabel(
        QStringLiteral("配置串口、执行设备命令并查看收发原始帧"), this);
    subtitle->setObjectName(QStringLiteral("pageSubtitle"));
    heading->addWidget(title);
    heading->addWidget(subtitle);
    header->addLayout(heading);
    header->addStretch();
    m_connectionLabel = new QLabel(QStringLiteral("未连接"), this);
    m_connectionLabel->setObjectName(QStringLiteral("statusPill"));
    header->addWidget(m_connectionLabel);
    root->addLayout(header);

    auto *topRow = new QHBoxLayout;
    topRow->setSpacing(16);

    auto *settingsPanel = panel(this);
    auto *settingsLayout = new QVBoxLayout(settingsPanel);
    settingsLayout->setContentsMargins(20, 18, 20, 18);
    settingsLayout->setSpacing(12);
    settingsLayout->addWidget(sectionTitle(QStringLiteral("串口设置"), settingsPanel));

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);
    auto *portLabel = new QLabel(QStringLiteral("端口"), settingsPanel);
    m_portCombo = new QComboBox(settingsPanel);
    m_portCombo->setMinimumWidth(210);
    auto *refreshButton = new QPushButton(QStringLiteral("刷新"), settingsPanel);
    refreshButton->setObjectName(QStringLiteral("ghostButton"));
    grid->addWidget(portLabel, 0, 0);
    grid->addWidget(m_portCombo, 0, 1, 1, 2);
    grid->addWidget(refreshButton, 0, 3);

    m_baudCombo = new QComboBox(settingsPanel);
    const QList<int> baudRates {9600, 19200, 38400, 57600, 115200, 230400};
    for (int baud : baudRates) {
        m_baudCombo->addItem(QString::number(baud), baud);
    }
    m_baudCombo->setCurrentText(QStringLiteral("115200"));
    m_dataBitsCombo = new QComboBox(settingsPanel);
    for (int bits : {5, 6, 7, 8}) {
        m_dataBitsCombo->addItem(QString::number(bits), bits);
    }
    m_dataBitsCombo->setCurrentText(QStringLiteral("8"));
    m_parityCombo = new QComboBox(settingsPanel);
    m_parityCombo->addItem(QStringLiteral("None"), QSerialPort::NoParity);
    m_parityCombo->addItem(QStringLiteral("Even"), QSerialPort::EvenParity);
    m_parityCombo->addItem(QStringLiteral("Odd"), QSerialPort::OddParity);
    m_stopBitsCombo = new QComboBox(settingsPanel);
    m_stopBitsCombo->addItem(QStringLiteral("1"), QSerialPort::OneStop);
    m_stopBitsCombo->addItem(QStringLiteral("1.5"), QSerialPort::OneAndHalfStop);
    m_stopBitsCombo->addItem(QStringLiteral("2"), QSerialPort::TwoStop);

    grid->addWidget(new QLabel(QStringLiteral("波特率"), settingsPanel), 1, 0);
    grid->addWidget(m_baudCombo, 1, 1);
    grid->addWidget(new QLabel(QStringLiteral("数据位"), settingsPanel), 1, 2);
    grid->addWidget(m_dataBitsCombo, 1, 3);
    grid->addWidget(new QLabel(QStringLiteral("校验"), settingsPanel), 2, 0);
    grid->addWidget(m_parityCombo, 2, 1);
    grid->addWidget(new QLabel(QStringLiteral("停止位"), settingsPanel), 2, 2);
    grid->addWidget(m_stopBitsCombo, 2, 3);
    settingsLayout->addLayout(grid);

    m_connectButton = new QPushButton(QStringLiteral("连接设备"), settingsPanel);
    m_connectButton->setObjectName(QStringLiteral("primaryButton"));
    settingsLayout->addWidget(m_connectButton);
    topRow->addWidget(settingsPanel, 2);

    auto *commandPanel = panel(this);
    auto *commandLayout = new QVBoxLayout(commandPanel);
    commandLayout->setContentsMargins(20, 18, 20, 18);
    commandLayout->setSpacing(10);
    commandLayout->addWidget(sectionTitle(QStringLiteral("设备命令"), commandPanel));
    auto *commandGrid = new QGridLayout;
    m_readButton = new QPushButton(QStringLiteral("读取全部参数"), commandPanel);
    m_readButton->setObjectName(QStringLiteral("secondaryButton"));
    m_enableButton = new QPushButton(QStringLiteral("使能设备"), commandPanel);
    m_enableButton->setObjectName(QStringLiteral("successButton"));
    m_disableButton = new QPushButton(QStringLiteral("安全停止"), commandPanel);
    m_disableButton->setObjectName(QStringLiteral("dangerButton"));
    m_faultButton = new QPushButton(QStringLiteral("故障复位"), commandPanel);
    m_faultButton->setObjectName(QStringLiteral("secondaryButton"));
    m_saveButton = new QPushButton(QStringLiteral("保存参数"), commandPanel);
    m_saveButton->setObjectName(QStringLiteral("secondaryButton"));
    commandGrid->addWidget(m_readButton, 0, 0, 1, 2);
    commandGrid->addWidget(m_enableButton, 1, 0);
    commandGrid->addWidget(m_disableButton, 1, 1);
    commandGrid->addWidget(m_faultButton, 2, 0);
    commandGrid->addWidget(m_saveButton, 2, 1);
    commandLayout->addLayout(commandGrid);
    commandLayout->addStretch();
    topRow->addWidget(commandPanel, 1);
    root->addLayout(topRow);

    auto *logPanel = panel(this);
    auto *logLayout = new QVBoxLayout(logPanel);
    logLayout->setContentsMargins(20, 18, 20, 18);
    logLayout->setSpacing(10);
    auto *logHeader = new QHBoxLayout;
    logHeader->addWidget(sectionTitle(QStringLiteral("原始通信日志"), logPanel));
    logHeader->addStretch();
    auto *clearButton = new QPushButton(QStringLiteral("清空日志"), logPanel);
    clearButton->setObjectName(QStringLiteral("ghostButton"));
    logHeader->addWidget(clearButton);
    logLayout->addLayout(logHeader);

    m_log = new QPlainTextEdit(logPanel);
    m_log->setObjectName(QStringLiteral("communicationLog"));
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(4000);
    m_log->setPlaceholderText(QStringLiteral("连接设备后，这里显示十六进制收发数据。"));
    logLayout->addWidget(m_log, 1);

    auto *rawRow = new QHBoxLayout;
    m_rawInput = new QPlainTextEdit(logPanel);
    m_rawInput->setMaximumHeight(72);
    m_rawInput->setPlaceholderText(
        QStringLiteral("输入十六进制帧，例如 55 55 FE FE EF EF AA AA"));
    auto *sendButton = new QPushButton(QStringLiteral("发送原始帧"), logPanel);
    sendButton->setObjectName(QStringLiteral("primaryButton"));
    rawRow->addWidget(m_rawInput, 1);
    rawRow->addWidget(sendButton);
    logLayout->addLayout(rawRow);
    root->addWidget(logPanel, 1);

    connect(refreshButton, &QPushButton::clicked, this, &CommunicationPage::refreshPorts);
    connect(m_connectButton, &QPushButton::clicked,
            this, &CommunicationPage::toggleConnection);
    connect(m_readButton, &QPushButton::clicked,
            this, &CommunicationPage::readAllRequested);
    connect(m_enableButton, &QPushButton::clicked,
            this, &CommunicationPage::enableRequested);
    connect(m_disableButton, &QPushButton::clicked,
            this, &CommunicationPage::disableRequested);
    connect(m_faultButton, &QPushButton::clicked,
            this, &CommunicationPage::faultResetRequested);
    connect(m_saveButton, &QPushButton::clicked,
            this, &CommunicationPage::saveRequested);
    connect(sendButton, &QPushButton::clicked,
            this, &CommunicationPage::sendRawCommand);
    connect(clearButton, &QPushButton::clicked,
            this, &CommunicationPage::clearLog);

    refreshPorts();
    setConnected(false, QString());
}

void CommunicationPage::setConnected(bool connected, const QString &portName)
{
    m_connected = connected;
    m_connectButton->setText(connected ? QStringLiteral("断开连接")
                                       : QStringLiteral("连接设备"));
    m_connectButton->setProperty("connected", connected);
    m_connectionLabel->setText(connected
                                   ? QStringLiteral("已连接 · %1").arg(portName)
                                   : QStringLiteral("未连接"));
    m_connectionLabel->setProperty("state",
                                   connected ? QStringLiteral("online")
                                             : QStringLiteral("offline"));
    m_portCombo->setEnabled(!connected);
    m_baudCombo->setEnabled(!connected);
    m_dataBitsCombo->setEnabled(!connected);
    m_parityCombo->setEnabled(!connected);
    m_stopBitsCombo->setEnabled(!connected);

    for (QPushButton *button : {m_readButton, m_enableButton, m_disableButton,
                                m_faultButton, m_saveButton}) {
        button->setEnabled(connected);
    }
    m_rawInput->setEnabled(connected);

    m_connectButton->style()->unpolish(m_connectButton);
    m_connectButton->style()->polish(m_connectButton);
    m_connectionLabel->style()->unpolish(m_connectionLabel);
    m_connectionLabel->style()->polish(m_connectionLabel);
}

void CommunicationPage::logFrame(bool transmitted, const QByteArray &frame)
{
    const QString timestamp =
        QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    const QString prefix = transmitted ? QStringLiteral("TX") : QStringLiteral("RX");
    m_log->appendPlainText(
        QStringLiteral("[%1] %2  %3").arg(timestamp, prefix,
                                          QString::fromLatin1(frame.toHex(' ').toUpper())));
}

void CommunicationPage::logMessage(const QString &message)
{
    const QString timestamp =
        QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    m_log->appendPlainText(QStringLiteral("[%1] ERROR  %2").arg(timestamp, message));
}

void CommunicationPage::refreshPorts()
{
    const QString previous = m_portCombo->currentData().toString();
    m_portCombo->clear();
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        QString label = info.portName();
        if (!info.description().isEmpty()) {
            label += QStringLiteral(" · ") + info.description();
        }
        m_portCombo->addItem(label, info.portName());
    }
    if (m_portCombo->count() == 0) {
        m_portCombo->addItem(QStringLiteral("未发现串口"), QString());
    } else {
        const int previousIndex = m_portCombo->findData(previous);
        if (previousIndex >= 0) {
            m_portCombo->setCurrentIndex(previousIndex);
        }
    }
}

void CommunicationPage::toggleConnection()
{
    if (m_connected) {
        emit disconnectRequested();
        return;
    }
    const QString portName = m_portCombo->currentData().toString();
    if (portName.isEmpty()) {
        m_log->appendPlainText(QStringLiteral("[ERROR] 没有可用的串口"));
        return;
    }
    emit openRequested(portName,
                       m_baudCombo->currentData().toInt(),
                       m_dataBitsCombo->currentData().toInt(),
                       m_parityCombo->currentData().toInt(),
                       m_stopBitsCombo->currentData().toInt());
}

void CommunicationPage::sendRawCommand()
{
    QString compact = m_rawInput->toPlainText();
    compact.remove(QLatin1Char(' '));
    compact.remove(QLatin1Char('\n'));
    compact.remove(QLatin1Char('\r'));
    compact.remove(QLatin1Char('\t'));
    const QByteArray frame = QByteArray::fromHex(compact.toLatin1());
    if (frame.isEmpty()) {
        m_log->appendPlainText(QStringLiteral("[ERROR] 十六进制帧无效"));
        return;
    }
    emit rawFrameRequested(frame);
}

void CommunicationPage::clearLog()
{
    m_log->clear();
}
