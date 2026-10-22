#include "mainwindow.h"

#include "communicationpage.h"
#include "dashboardpage.h"
#include "driveinfopage.h"
#include "monitorpage.h"
#include "parametermodel.h"
#include "parameterpage.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QVariant>
#include <QVBoxLayout>

namespace {

QLabel *navSectionLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text.toUpper(), parent);
    label->setObjectName(QStringLiteral("navSection"));
    return label;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_serial(new DeviceSerialService(this))
    , m_parameterModel(new ParameterModel(this))
{
    createInterface();
    applyStyle();

    connect(m_serial, &DeviceSerialService::connectionChanged,
            this, &MainWindow::setConnectionState);
    connect(m_serial, &DeviceSerialService::telemetryUpdated,
            this, &MainWindow::handleTelemetry);
    connect(m_serial, &DeviceSerialService::parameterValueReceived,
            m_parameterModel, &ParameterModel::setRemoteValue);
    connect(m_serial, &DeviceSerialService::parameterValueReceived,
            m_driveInfoPage, &DriveInfoPage::setParameterValue);
    connect(m_serial, &DeviceSerialService::parameterReadProgress,
            m_parameterPage, &ParameterPage::setReadProgress);
    connect(m_serial, &DeviceSerialService::parameterReadProgress,
            m_dashboard, &DashboardPage::setReadProgress);
    connect(m_serial, &DeviceSerialService::parameterReadProgress,
            m_driveInfoPage, [this](int received, int) {
                m_driveInfoPage->setParameterReadCount(received);
            });
    connect(m_serial, &DeviceSerialService::parameterReadFinished,
            this, &MainWindow::handleParameterReadFinished);
    connect(m_serial, &DeviceSerialService::frameLogged,
            m_communicationPage, &CommunicationPage::logFrame);
    connect(m_serial, &DeviceSerialService::errorOccurred,
            this, &MainWindow::handleSerialError);

    connect(m_communicationPage, &CommunicationPage::openRequested,
            this, &MainWindow::openSerial);
    connect(m_communicationPage, &CommunicationPage::disconnectRequested,
            this, &MainWindow::closeSerial);
    connect(m_communicationPage, &CommunicationPage::readAllRequested,
            this, &MainWindow::readAllParameters);
    connect(m_communicationPage, &CommunicationPage::enableRequested,
            m_serial, &DeviceSerialService::requestMotorEnable);
    connect(m_communicationPage, &CommunicationPage::disableRequested,
            m_serial, &DeviceSerialService::requestMotorDisable);
    connect(m_communicationPage, &CommunicationPage::faultResetRequested,
            m_serial, &DeviceSerialService::requestFaultReset);
    connect(m_communicationPage, &CommunicationPage::saveRequested,
            this, &MainWindow::requestParameterSave);
    connect(m_communicationPage, &CommunicationPage::rawFrameRequested,
            m_serial, &DeviceSerialService::sendFrame);

    connect(m_dashboard, &DashboardPage::readAllRequested,
            this, &MainWindow::readAllParameters);
    connect(m_dashboard, &DashboardPage::enableRequested,
            m_serial, &DeviceSerialService::requestMotorEnable);
    connect(m_dashboard, &DashboardPage::disableRequested,
            m_serial, &DeviceSerialService::requestMotorDisable);
    connect(m_dashboard, &DashboardPage::saveRequested,
            this, &MainWindow::requestParameterSave);
    connect(m_dashboard, &DashboardPage::openCommunicationRequested,
            this, &MainWindow::showCommunicationPage);
    connect(m_topMonitorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() {
                if (m_connected) {
                    m_serial->writeParameter(
                        2, static_cast<quint16>(m_topMonitorCombo->currentData().toInt()));
                }
            });

    connect(m_parameterPage, &ParameterPage::readAllRequested,
            this, &MainWindow::readAllParameters);
    connect(m_parameterPage, &ParameterPage::writeDirtyRequested,
            this, &MainWindow::writeDirtyParameters);
    connect(m_parameterPage, &ParameterPage::writeAndSaveRequested,
            this, &MainWindow::writeAndSaveParameters);
    connect(m_parameterPage, &ParameterPage::statusMessage,
            this, [this](const QString &message) { statusBar()->showMessage(message, 5000); });
    connect(m_monitorPage, &MonitorPage::statusMessage,
            this, [this](const QString &message) { statusBar()->showMessage(message, 5000); });
    connect(m_driveInfoPage, &DriveInfoPage::readAllRequested,
            this, &MainWindow::readAllParameters);
    connect(m_parameterModel, &ParameterModel::dirtyStateChanged,
            this, &MainWindow::updateDirtyStatus);

    setWindowTitle(QStringLiteral("KaiserStudio Qt · 驱动器调试工作台"));
    setMinimumSize(1180, 760);
    resize(1440, 900);
    updateDirtyStatus(0);
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_parameterModel->dirtyCount() > 0) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("退出程序"),
            QStringLiteral("仍有 %1 个参数修改尚未写入设备，确定退出吗？")
                .arg(m_parameterModel->dirtyCount()));
        if (answer != QMessageBox::Yes) {
            event->ignore();
            return;
        }
    }
    m_serial->close();
    QMainWindow::closeEvent(event);
}

void MainWindow::openSerial(const QString &portName, int baudRate, int dataBits,
                            int parity, int stopBits)
{
    DeviceSerialService::Options options;
    options.portName = portName;
    options.baudRate = baudRate;
    options.dataBits = dataBits;
    options.parity = parity;
    options.stopBits = stopBits;
    if (m_serial->open(options)) {
        statusBar()->showMessage(
            QStringLiteral("串口 %1 已连接，115200 8N1 心跳通信已启动").arg(portName),
            5000);
    }
}

void MainWindow::closeSerial()
{
    m_serial->close();
    statusBar()->showMessage(QStringLiteral("串口连接已断开"), 3000);
}

void MainWindow::setConnectionState(bool connected, const QString &portName)
{
    m_connected = connected;
    m_communicationPage->setConnected(connected, portName);
    m_dashboard->setConnectionState(connected, portName);
    m_driveInfoPage->setConnected(connected, portName);
    m_parameterPage->setConnected(connected);
    m_monitorPage->setConnected(connected);

    m_topConnectionLabel->setText(connected
                                      ? QStringLiteral("●  %1").arg(portName)
                                      : QStringLiteral("●  未连接"));
    m_topConnectionLabel->setProperty("state",
                                      connected ? QStringLiteral("online")
                                                : QStringLiteral("offline"));
    m_topConnectionLabel->style()->unpolish(m_topConnectionLabel);
    m_topConnectionLabel->style()->polish(m_topConnectionLabel);
    if (!connected) {
        m_topEnableLabel->setText(QStringLiteral("DISABLED"));
        m_topEnableLabel->setProperty("state", QStringLiteral("warning"));
        m_topFaultLabel->setText(QStringLiteral("NO FLT"));
        m_topFaultLabel->setProperty("state", QStringLiteral("offline"));
        m_topModeLabel->setText(QStringLiteral("驱动模式 · OFFLINE"));
        for (QLabel *label : {m_livePosition, m_liveMultiTurn, m_liveSingleTurn,
                              m_liveSpeed, m_liveCurrent, m_liveVoltage, m_liveForce}) {
            label->setText(QStringLiteral("--"));
        }
        for (QLabel *label : {m_topEnableLabel, m_topFaultLabel}) {
            label->style()->unpolish(label);
            label->style()->polish(label);
        }
    }
}

void MainWindow::handleTelemetry(const DeviceSerialService::Telemetry &telemetry)
{
    m_dashboard->updateTelemetry(telemetry);
    m_monitorPage->updateTelemetry(telemetry);
    m_driveInfoPage->updateTelemetry(telemetry);
    m_livePosition->setText(QString::number(telemetry.position, 'f', 0));
    m_liveMultiTurn->setText(QString::number(telemetry.multiTurnPosition, 'f', 0));
    m_liveSingleTurn->setText(QString::number(telemetry.singleTurnPosition, 'f', 0));
    m_liveSpeed->setText(QString::number(telemetry.speed, 'f', 1));
    m_liveCurrent->setText(QString::number(telemetry.current, 'f', 2));
    m_liveVoltage->setText(QString::number(telemetry.busVoltage, 'f', 2));
    m_liveForce->setText(QString::number(telemetry.force, 'f', 0));
    if (telemetry.statusKnown) {
        m_dashboard->setMotorEnabled(telemetry.motorEnabled);
        m_topEnableLabel->setText(telemetry.motorEnabled
                                      ? QStringLiteral("ENABLED")
                                      : QStringLiteral("DISABLED"));
        m_topEnableLabel->setProperty("state",
                                      telemetry.motorEnabled
                                          ? QStringLiteral("online")
                                          : QStringLiteral("warning"));
        m_topEnableLabel->style()->unpolish(m_topEnableLabel);
        m_topEnableLabel->style()->polish(m_topEnableLabel);
    }
    if (telemetry.faultCode != 0) {
        m_dashboard->setFaultCode(telemetry.faultCode);
        m_topFaultLabel->setText(
            QStringLiteral("FLT %1").arg(telemetry.faultCode));
        m_topFaultLabel->setProperty("state", QStringLiteral("error"));
    } else {
        m_topFaultLabel->setText(QStringLiteral("NO FLT"));
        m_topFaultLabel->setProperty("state", QStringLiteral("online"));
    }
    m_topFaultLabel->style()->unpolish(m_topFaultLabel);
    m_topFaultLabel->style()->polish(m_topFaultLabel);
    m_topModeLabel->setText(telemetry.motorEnabled
                                ? QStringLiteral("驱动模式 · RUN")
                                : QStringLiteral("驱动模式 · SAFE"));
}

void MainWindow::readAllParameters()
{
    if (!confirmReadWhileDirty()) {
        return;
    }
    if (m_serial->readAllParameters()) {
        statusBar()->showMessage(QStringLiteral("正在读取设备 Pn000–Pn199…"));
    }
}

void MainWindow::writeDirtyParameters()
{
    const QMap<quint16, quint16> values = m_parameterModel->dirtyValues();
    if (!confirmWrite(values)) {
        return;
    }

    int count = 0;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        if (m_serial->writeParameter(it.key(), it.value())) {
            m_parameterModel->markWritten(it.key());
            ++count;
        }
    }
    statusBar()->showMessage(
        QStringLiteral("已将 %1 个参数加入发送队列").arg(count), 5000);
}

void MainWindow::writeAndSaveParameters()
{
    const QMap<quint16, quint16> values = m_parameterModel->dirtyValues();
    if (!confirmWrite(values)) {
        return;
    }

    int count = 0;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        if (m_serial->writeParameter(it.key(), it.value())) {
            m_parameterModel->markWritten(it.key());
            ++count;
        }
    }
    requestParameterSave();
    statusBar()->showMessage(
        QStringLiteral("已提交 %1 个参数，并发送 Pn152 保存命令").arg(count), 6000);
}

void MainWindow::requestParameterSave()
{
    if (!m_connected) {
        QMessageBox::information(this, QStringLiteral("保存参数"),
                                 QStringLiteral("请先连接设备"));
        return;
    }
    if (QMessageBox::question(
            this, QStringLiteral("写入 EEPROM"),
            QStringLiteral("将当前设备参数保存到 EEPROM（Pn152 = 1），是否继续？"))
        != QMessageBox::Yes) {
        return;
    }
    if (m_serial->requestParameterSave()) {
        m_parameterModel->markWritten(152);
        statusBar()->showMessage(QStringLiteral("Pn152 保存命令已发送"), 5000);
    }
}

void MainWindow::showParametersForGroup(const QString &group)
{
    m_parameterPage->selectGroup(group);
    showPage(1);
    setCurrentPageTitle(group == QStringLiteral("全部参数")
                            ? QStringLiteral("Pn 参数工作台")
                            : QStringLiteral("%1参数").arg(group));
}

void MainWindow::showPage(int index)
{
    if (index < 0 || index >= m_stack->count()) {
        return;
    }
    m_stack->setCurrentIndex(index);
    static const QStringList titles {
        QStringLiteral("实时设备概览"),
        QStringLiteral("Pn 参数工作台"),
        QStringLiteral("实时示波器"),
        QStringLiteral("串口通信中心"),
        QStringLiteral("驱动器信息"),
    };
    if (index < titles.size()) {
        setCurrentPageTitle(titles.at(index));
        for (QAbstractButton *button : m_navGroup->buttons()) {
            if (button->property("pageIndex").toInt() == index
                && button->property("group").toString().isEmpty()) {
                button->setChecked(true);
                break;
            }
        }
    }
}

void MainWindow::handleParameterReadFinished(bool complete, int received)
{
    m_parameterPage->setReadFinished(complete, received);
    m_dashboard->setParameterReadFinished(complete, received);
    statusBar()->showMessage(
        complete
            ? QStringLiteral("参数读取完成，共 %1 项").arg(received)
            : QStringLiteral("参数读取未完整结束，收到 %1 项；请检查串口连接").arg(received),
        complete ? 5000 : 8000);
}

void MainWindow::handleSerialError(const QString &message)
{
    statusBar()->showMessage(message, 8000);
    m_communicationPage->logMessage(message);
}

void MainWindow::showCommunicationPage()
{
    showPage(3);
    for (QAbstractButton *button : m_navGroup->buttons()) {
        if (button->property("pageIndex").toInt() == 3) {
            button->setChecked(true);
            break;
        }
    }
}

bool MainWindow::confirmReadWhileDirty()
{
    if (m_parameterModel->dirtyCount() == 0) {
        return true;
    }
    return QMessageBox::question(
               this, QStringLiteral("读取全部参数"),
               QStringLiteral("读取会覆盖 %1 个尚未写入的参数修改，是否继续？")
                   .arg(m_parameterModel->dirtyCount()))
        == QMessageBox::Yes;
}

bool MainWindow::confirmWrite(const QMap<quint16, quint16> &values)
{
    if (!m_connected) {
        QMessageBox::information(this, QStringLiteral("写入参数"),
                                 QStringLiteral("请先连接设备"));
        return false;
    }
    if (values.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("写入参数"),
                                 QStringLiteral("当前没有待写入的参数修改"));
        return false;
    }

    QStringList preview;
    int shown = 0;
    for (auto it = values.constBegin(); it != values.constEnd() && shown < 8; ++it, ++shown) {
        preview.append(QStringLiteral("Pn%1 = %2")
                           .arg(it.key(), 3, 10, QLatin1Char('0'))
                           .arg(it.value()));
    }
    if (values.size() > shown) {
        preview.append(QStringLiteral("…以及另外 %1 项").arg(values.size() - shown));
    }

    return QMessageBox::question(
               this, QStringLiteral("确认写入设备"),
               QStringLiteral("将写入 %1 个参数：\n\n%2\n\n参数写入可能立即影响设备动作，请确认现场安全。")
                   .arg(values.size())
                   .arg(preview.join(QLatin1Char('\n'))))
        == QMessageBox::Yes;
}

void MainWindow::createInterface()
{
    auto *shell = new QWidget(this);
    shell->setObjectName(QStringLiteral("appShell"));
    auto *shellLayout = new QHBoxLayout(shell);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);
    setCentralWidget(shell);

    auto *sidebar = new QWidget(shell);
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(232);
    auto *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(18, 22, 18, 18);
    sidebarLayout->setSpacing(10);

    auto *brand = new QLabel(QStringLiteral("KAISER"), sidebar);
    brand->setObjectName(QStringLiteral("brand"));
    auto *brandAccent = new QLabel(QStringLiteral("STUDIO  ·  QT"), sidebar);
    brandAccent->setObjectName(QStringLiteral("brandAccent"));
    auto *brandSub = new QLabel(QStringLiteral("SERVO DRIVE WORKBENCH"), sidebar);
    brandSub->setObjectName(QStringLiteral("brandSub"));
    sidebarLayout->addWidget(brand);
    sidebarLayout->addWidget(brandAccent);
    sidebarLayout->addWidget(brandSub);

    auto *navScroll = new QScrollArea(sidebar);
    navScroll->setObjectName(QStringLiteral("navScroll"));
    navScroll->setWidgetResizable(true);
    navScroll->setFrameShape(QFrame::NoFrame);
    navScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *navContainer = new QWidget(navScroll);
    navContainer->setObjectName(QStringLiteral("navContainer"));
    auto *navLayout = new QVBoxLayout(navContainer);
    navLayout->setContentsMargins(0, 10, 0, 0);
    navLayout->setSpacing(7);
    navScroll->setWidget(navContainer);
    sidebarLayout->addWidget(navScroll, 1);

    m_stack = new QStackedWidget(shell);
    m_dashboard = new DashboardPage(m_stack);
    m_parameterPage = new ParameterPage(m_parameterModel, m_stack);
    m_monitorPage = new MonitorPage(m_stack);
    m_communicationPage = new CommunicationPage(m_stack);
    m_driveInfoPage = new DriveInfoPage(m_stack);
    m_stack->addWidget(m_dashboard);
    m_stack->addWidget(m_parameterPage);
    m_stack->addWidget(m_monitorPage);
    m_stack->addWidget(m_communicationPage);
    m_stack->addWidget(m_driveInfoPage);

    const auto addPageShortcut = [this](Qt::Key key, int pageIndex) {
        auto *shortcut = new QShortcut(QKeySequence(key), this);
        connect(shortcut, &QShortcut::activated, this,
                [this, pageIndex]() { showPage(pageIndex); });
    };
    addPageShortcut(Qt::Key_F1, 0);
    addPageShortcut(Qt::Key_F2, 1);
    addPageShortcut(Qt::Key_F3, 2);
    addPageShortcut(Qt::Key_F4, 3);
    addPageShortcut(Qt::Key_F5, 4);

    m_navGroup = new QButtonGroup(this);
    m_navGroup->setExclusive(true);
    navLayout->addWidget(navSectionLabel(QStringLiteral("主要工作区"), sidebar));
    navLayout->addWidget(createNavButton(QStringLiteral("连接设备"), 3));
    navLayout->addWidget(createNavButton(QStringLiteral("实时概览"), 0));
    navLayout->addWidget(createNavButton(QStringLiteral("参数设定"), 1,
                                         QStringLiteral("全部参数")));
    navLayout->addWidget(createNavButton(QStringLiteral("实时示波器"), 2));
    navLayout->addWidget(createNavButton(QStringLiteral("驱动器信息"), 4));
    navLayout->addSpacing(12);
    navLayout->addWidget(navSectionLabel(QStringLiteral("设备参数"), sidebar));
    navLayout->addWidget(createNavButton(QStringLiteral("电机"), 1,
                                         QStringLiteral("电机")));
    navLayout->addWidget(createNavButton(QStringLiteral("反馈"), 1,
                                         QStringLiteral("反馈")));
    navLayout->addWidget(createNavButton(QStringLiteral("制动器"), 1,
                                         QStringLiteral("制动器")));
    navLayout->addWidget(createNavButton(QStringLiteral("限定"), 1,
                                         QStringLiteral("限定")));
    navLayout->addWidget(createNavButton(QStringLiteral("再生电阻"), 1,
                                         QStringLiteral("再生电阻")));
    navLayout->addWidget(createNavButton(QStringLiteral("数字 I/O"), 1,
                                         QStringLiteral("数字 I/O")));
    navLayout->addWidget(createNavButton(QStringLiteral("控制与标定"), 1,
                                         QStringLiteral("控制与标定")));
    navLayout->addWidget(createNavButton(QStringLiteral("参数复位"), 1,
                                         QStringLiteral("系统"),
                                         QStringLiteral("参数复位与系统功能")));
    navLayout->addWidget(createNavButton(QStringLiteral("系统"), 1,
                                         QStringLiteral("系统")));
    navLayout->addWidget(createNavButton(QStringLiteral("备份与恢复"), 1,
                                         QStringLiteral("全部参数"),
                                         QStringLiteral("参数备份与恢复")));
    navLayout->addWidget(createNavButton(QStringLiteral("使能与故障"), 0,
                                         QString(),
                                         QStringLiteral("使能与故障状态")));
    navLayout->addStretch();

    auto *safetyNote = new QLabel(
        QStringLiteral("参数写入前请确认设备处于安全状态。"), sidebar);
    safetyNote->setObjectName(QStringLiteral("sidebarNote"));
    safetyNote->setWordWrap(true);
    sidebarLayout->addWidget(safetyNote);
    auto *version = new QLabel(QStringLiteral("KaiserStudio Qt · 0.2"), sidebar);
    version->setObjectName(QStringLiteral("versionLabel"));
    sidebarLayout->addWidget(version);
    shellLayout->addWidget(sidebar);

    auto *workspace = new QWidget(shell);
    workspace->setObjectName(QStringLiteral("workspace"));
    auto *workspaceLayout = new QVBoxLayout(workspace);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    workspaceLayout->setSpacing(0);

    auto *topBar = new QFrame(workspace);
    topBar->setObjectName(QStringLiteral("topBar"));
    topBar->setFixedHeight(58);
    auto *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(28, 0, 28, 0);
    m_pageTitle = new QLabel(QStringLiteral("实时设备概览"), topBar);
    m_pageTitle->setObjectName(QStringLiteral("topTitle"));
    m_topEnableLabel = new QLabel(QStringLiteral("DISABLED"), topBar);
    m_topEnableLabel->setObjectName(QStringLiteral("ribbonChip"));
    m_topEnableLabel->setProperty("state", QStringLiteral("warning"));
    m_topFaultLabel = new QLabel(QStringLiteral("NO FLT"), topBar);
    m_topFaultLabel->setObjectName(QStringLiteral("ribbonChip"));
    m_topFaultLabel->setProperty("state", QStringLiteral("online"));
    m_topModeLabel = new QLabel(QStringLiteral("驱动模式 · SAFE"), topBar);
    m_topModeLabel->setObjectName(QStringLiteral("ribbonChip"));
    m_topMonitorCombo = new QComboBox(topBar);
    m_topMonitorCombo->setObjectName(QStringLiteral("monitorCombo"));
    m_topMonitorCombo->addItem(QStringLiteral("监视 · 力/转矩"), 79);
    m_topMonitorCombo->addItem(QStringLiteral("监视 · 位移量"), 81);
    m_topConnectionLabel = new QLabel(QStringLiteral("●  未连接"), topBar);
    m_topConnectionLabel->setObjectName(QStringLiteral("connectionChip"));
    m_topConnectionLabel->setProperty("state", QStringLiteral("offline"));
    topLayout->addWidget(m_pageTitle);
    topLayout->addStretch();
    topLayout->addWidget(m_topEnableLabel);
    topLayout->addWidget(m_topFaultLabel);
    topLayout->addWidget(m_topMonitorCombo);
    topLayout->addWidget(m_topModeLabel);
    topLayout->addWidget(m_topConnectionLabel);
    workspaceLayout->addWidget(topBar);

    auto *liveRibbon = new QFrame(workspace);
    liveRibbon->setObjectName(QStringLiteral("liveRibbon"));
    liveRibbon->setFixedHeight(66);
    auto *liveLayout = new QHBoxLayout(liveRibbon);
    liveLayout->setContentsMargins(24, 8, 24, 8);
    liveLayout->setSpacing(8);
    const auto addLiveMetric = [liveRibbon, liveLayout](
                                   const QString &caption,
                                   const QString &unit,
                                   QLabel *&valueLabel) {
        auto *cell = new QWidget(liveRibbon);
        cell->setObjectName(QStringLiteral("liveMetric"));
        auto *cellLayout = new QVBoxLayout(cell);
        cellLayout->setContentsMargins(12, 3, 12, 3);
        cellLayout->setSpacing(0);
        auto *captionLabel = new QLabel(caption, cell);
        captionLabel->setObjectName(QStringLiteral("liveCaption"));
        auto *valueRow = new QHBoxLayout;
        valueRow->setContentsMargins(0, 0, 0, 0);
        valueRow->setSpacing(4);
        valueLabel = new QLabel(QStringLiteral("--"), cell);
        valueLabel->setObjectName(QStringLiteral("liveValue"));
        auto *unitLabel = new QLabel(unit, cell);
        unitLabel->setObjectName(QStringLiteral("liveUnit"));
        valueRow->addWidget(valueLabel);
        valueRow->addWidget(unitLabel, 0, Qt::AlignBottom);
        valueRow->addStretch();
        cellLayout->addWidget(captionLabel);
        cellLayout->addLayout(valueRow);
        liveLayout->addWidget(cell, 1);
    };
    addLiveMetric(QStringLiteral("位置"), QStringLiteral("count"), m_livePosition);
    addLiveMetric(QStringLiteral("多圈"), QStringLiteral("count"), m_liveMultiTurn);
    addLiveMetric(QStringLiteral("单圈"), QStringLiteral("count"), m_liveSingleTurn);
    addLiveMetric(QStringLiteral("速度"), QStringLiteral("rpm"), m_liveSpeed);
    addLiveMetric(QStringLiteral("电流"), QStringLiteral("A"), m_liveCurrent);
    addLiveMetric(QStringLiteral("母线电压"), QStringLiteral("V"), m_liveVoltage);
    addLiveMetric(QStringLiteral("力/转矩"), QStringLiteral("raw"), m_liveForce);
    workspaceLayout->addWidget(liveRibbon);
    workspaceLayout->addWidget(m_stack, 1);
    shellLayout->addWidget(workspace, 1);

    statusBar()->setSizeGripEnabled(false);
    m_dirtyStatusLabel = new QLabel(this);
    m_dirtyStatusLabel->setObjectName(QStringLiteral("statusBarText"));
    statusBar()->addPermanentWidget(m_dirtyStatusLabel);
    statusBar()->showMessage(QStringLiteral("就绪"));

    showPage(0);
}

QPushButton *MainWindow::createNavButton(const QString &text, int pageIndex,
                                         const QString &group,
                                         const QString &viewTitle)
{
    auto *button = new QPushButton(text, this);
    button->setObjectName(QStringLiteral("navButton"));
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setProperty("pageIndex", pageIndex);
    button->setProperty("group", group);
    button->setMinimumHeight(38);
    m_navGroup->addButton(button);

    connect(button, &QPushButton::clicked, this,
            [this, pageIndex, group, viewTitle]() {
                if (!group.isEmpty()) {
                    showParametersForGroup(group);
                } else {
                    showPage(pageIndex);
                }
                if (!viewTitle.isEmpty()) {
                    setCurrentPageTitle(viewTitle);
                }
            });
    return button;
}

void MainWindow::setCurrentPageTitle(const QString &title)
{
    m_pageTitle->setText(title);
}

void MainWindow::updateDirtyStatus(int count)
{
    if (count == 0) {
        m_dirtyStatusLabel->setText(QStringLiteral("参数无未写入修改"));
    } else {
        m_dirtyStatusLabel->setText(QStringLiteral("%1 个参数待写入").arg(count));
    }
}

void MainWindow::applyStyle()
{
    setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget#appShell, QWidget#workspace {
            background: #F3F6FA;
            color: #172033;
            font-family: "Microsoft YaHei UI", "Segoe UI";
            font-size: 13px;
        }
        QWidget#sidebar {
            background: #111827;
            border-right: 1px solid #1F2937;
        }
        QScrollArea#navScroll, QWidget#navContainer {
            background: transparent;
            border: none;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 6px;
            margin: 2px 0;
        }
        QScrollBar::handle:vertical {
            background: #334155;
            border-radius: 3px;
            min-height: 24px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QLabel#brand {
            color: #F8FAFC;
            font-size: 28px;
            font-weight: 800;
            letter-spacing: 2px;
        }
        QLabel#brandAccent {
            color: #2DD4BF;
            font-size: 17px;
            font-weight: 700;
            letter-spacing: 1px;
        }
        QLabel#brandSub, QLabel#versionLabel {
            color: #64748B;
            font-size: 10px;
            letter-spacing: 1px;
        }
        QLabel#navSection {
            color: #64748B;
            font-size: 10px;
            font-weight: 700;
            padding: 9px 8px 3px 8px;
            letter-spacing: 1px;
        }
        QPushButton#navButton {
            color: #CBD5E1;
            background: transparent;
            border: none;
            border-radius: 7px;
            text-align: left;
            padding: 9px 12px;
        }
        QPushButton#navButton:hover {
            color: #FFFFFF;
            background: #1E293B;
        }
        QPushButton#navButton:checked {
            color: #FFFFFF;
            background: #0F766E;
            font-weight: 700;
        }
        QLabel#sidebarNote {
            color: #94A3B8;
            background: #182235;
            border: 1px solid #273449;
            border-radius: 8px;
            padding: 10px;
            font-size: 11px;
        }
        QFrame#topBar {
            background: #FFFFFF;
            border-bottom: 1px solid #E2E8F0;
        }
        QFrame#liveRibbon {
            background: #111827;
            border-bottom: 1px solid #1F2937;
        }
        QWidget#liveMetric {
            background: #172235;
            border: 1px solid #263449;
            border-radius: 7px;
        }
        QLabel#liveCaption {
            color: #94A3B8;
            font-size: 10px;
            font-weight: 600;
        }
        QLabel#liveValue {
            color: #F8FAFC;
            font-family: "Cascadia Mono", "Consolas";
            font-size: 18px;
            font-weight: 700;
        }
        QLabel#liveUnit {
            color: #64748B;
            font-size: 9px;
            padding-bottom: 3px;
        }
        QLabel#topTitle {
            color: #172033;
            font-size: 18px;
            font-weight: 700;
        }
        QLabel#connectionChip, QLabel#statusPill {
            padding: 7px 12px;
            border-radius: 14px;
            background: #E2E8F0;
            color: #475569;
            font-weight: 600;
        }
        QLabel#connectionChip[state="online"], QLabel#statusPill[state="online"] {
            color: #0F766E;
            background: #CCFBF1;
        }
        QLabel#statusPill[state="warning"] {
            color: #B45309;
            background: #FEF3C7;
        }
        QLabel#ribbonChip {
            padding: 5px 9px;
            margin-left: 5px;
            border-radius: 5px;
            background: #F1F5F9;
            color: #64748B;
            font-size: 10px;
            font-weight: 700;
        }
        QLabel#ribbonChip[state="online"] {
            color: #0F766E;
            background: #CCFBF1;
        }
        QLabel#ribbonChip[state="warning"] {
            color: #B45309;
            background: #FEF3C7;
        }
        QLabel#ribbonChip[state="error"] {
            color: #B91C1C;
            background: #FEE2E2;
        }
        QComboBox#monitorCombo {
            min-height: 26px;
            padding: 1px 8px;
            color: #475569;
            background: #F1F5F9;
            border: 1px solid #E2E8F0;
            border-radius: 5px;
            font-size: 10px;
            font-weight: 600;
        }
        QLabel#pageHeading {
            color: #111827;
            font-size: 24px;
            font-weight: 800;
        }
        QLabel#pageSubtitle, QLabel#mutedText {
            color: #64748B;
        }
        QFrame#panel {
            background: #FFFFFF;
            border: 1px solid #E2E8F0;
            border-radius: 10px;
        }
        QLabel#sectionTitle {
            color: #1E293B;
            font-size: 15px;
            font-weight: 700;
        }
        QFrame#metricCard {
            background: #FFFFFF;
            border: 1px solid #E2E8F0;
            border-radius: 10px;
        }
        QLabel#metricTitle {
            color: #64748B;
            font-size: 12px;
            font-weight: 600;
        }
        QLabel#metricValue {
            color: #0F172A;
            font-size: 25px;
            font-weight: 800;
        }
        QLabel#metricUnit {
            color: #94A3B8;
            font-size: 11px;
            padding-bottom: 4px;
        }
        QLabel#metricNote {
            color: #94A3B8;
            font-size: 10px;
        }
        QFrame#smallStatCard {
            background: #FFFFFF;
            border: 1px solid #E2E8F0;
            border-radius: 8px;
        }
        QLabel#smallStatValue {
            color: #0F172A;
            font-size: 20px;
            font-weight: 700;
        }
        QLabel#deviceStatusRow {
            color: #334155;
            background: #F8FAFC;
            border-radius: 6px;
            padding: 9px 10px;
        }
        QLabel#deviceStatusRow[state="online"] {
            color: #0F766E;
            background: #ECFDF5;
        }
        QLabel#deviceStatusRow[state="offline"] {
            color: #64748B;
        }
        QLabel#deviceHero {
            color: #0F172A;
            font-family: "Cascadia Mono", "Consolas";
            font-size: 24px;
            font-weight: 800;
        }
        QLabel#infoCaption {
            color: #64748B;
            font-size: 11px;
        }
        QLabel#infoValue {
            color: #1E293B;
            font-size: 13px;
            font-weight: 700;
        }
        QLabel#infoCodeBlock {
            color: #D1FAE5;
            background: #0F172A;
            border-radius: 8px;
            padding: 12px;
            font-family: "Cascadia Mono", "Consolas";
            font-size: 11px;
        }
        QLabel#warningText {
            color: #B45309;
            font-weight: 600;
        }
        QPushButton {
            min-height: 34px;
            border-radius: 7px;
            padding: 0 14px;
            font-weight: 600;
        }
        QPushButton#primaryButton {
            color: #FFFFFF;
            background: #0F766E;
            border: 1px solid #0F766E;
        }
        QPushButton#primaryButton:hover { background: #0D9488; }
        QPushButton#secondaryButton {
            color: #1E293B;
            background: #FFFFFF;
            border: 1px solid #CBD5E1;
        }
        QPushButton#secondaryButton:hover { background: #F8FAFC; }
        QPushButton#successButton {
            color: #FFFFFF;
            background: #15803D;
            border: 1px solid #15803D;
        }
        QPushButton#successButton:hover { background: #16A34A; }
        QPushButton#dangerButton {
            color: #FFFFFF;
            background: #B91C1C;
            border: 1px solid #B91C1C;
        }
        QPushButton#dangerButton:hover { background: #DC2626; }
        QPushButton#ghostButton {
            color: #475569;
            background: transparent;
            border: 1px solid transparent;
        }
        QPushButton#ghostButton:hover {
            color: #0F766E;
            background: #ECFDF5;
        }
        QPushButton#modeButton {
            color: #64748B;
            background: #F1F5F9;
            border: 1px solid #E2E8F0;
            padding: 0 10px;
        }
        QPushButton#modeButton:checked {
            color: #FFFFFF;
            background: #334155;
            border-color: #334155;
        }
        QPushButton:disabled {
            color: #94A3B8;
            background: #E2E8F0;
            border-color: #E2E8F0;
        }
        QLineEdit, QComboBox, QPlainTextEdit {
            background: #FFFFFF;
            color: #172033;
            border: 1px solid #CBD5E1;
            border-radius: 7px;
            padding: 6px 9px;
            selection-background-color: #0F766E;
        }
        QLineEdit:focus, QComboBox:focus, QPlainTextEdit:focus {
            border: 1px solid #0F766E;
        }
        QTableView#parameterTable {
            background: #FFFFFF;
            alternate-background-color: #F8FAFC;
            border: 1px solid #E2E8F0;
            border-radius: 9px;
            gridline-color: #EEF2F7;
            selection-background-color: #DDF7F1;
            selection-color: #0F172A;
        }
        QTableWidget#compactParameterTable {
            background: #FFFFFF;
            alternate-background-color: #F8FAFC;
            border: 1px solid #E2E8F0;
            border-radius: 9px;
            gridline-color: #E2E8F0;
            selection-background-color: #DDF7F1;
            selection-color: #0F172A;
        }
        QTableWidget#compactParameterTable::item {
            padding: 3px 6px;
        }
        QHeaderView::section {
            background: #F1F5F9;
            color: #475569;
            border: none;
            border-bottom: 1px solid #E2E8F0;
            padding: 9px;
            font-weight: 700;
        }
        QTableView QLineEdit {
            border: none;
            border-radius: 0;
            background: #FFF7D6;
        }
        QProgressBar {
            background: #E2E8F0;
            border: none;
            border-radius: 6px;
            text-align: center;
            color: #334155;
            min-height: 18px;
        }
        QProgressBar::chunk {
            background: #0D9488;
            border-radius: 6px;
        }
        QPlainTextEdit#communicationLog {
            background: #0F172A;
            color: #D1FAE5;
            border: 1px solid #1E293B;
            font-family: "Cascadia Mono", "Consolas";
            font-size: 12px;
            padding: 10px;
        }
        QStatusBar {
            background: #FFFFFF;
            border-top: 1px solid #E2E8F0;
            color: #64748B;
        }
        QLabel#statusBarText {
            color: #64748B;
            padding-right: 12px;
        }
        QChartView#chartView {
            background: #FFFFFF;
            border: 1px solid #E2E8F0;
            border-radius: 10px;
        }
    )"));
}
