#include "mainwindow.h"

#include "communicationpage.h"
#include "dashboardpage.h"
#include "deviceconfigpage.h"
#include "digitaliopage.h"
#include "driveinfopage.h"
#include "faultpage.h"
#include "feedbackpage.h"
#include "homingpage.h"
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
    connect(m_serial, &DeviceSerialService::parameterValueReceived,
            m_feedbackPage, &FeedbackPage::setParameterValue);
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
    connect(m_faultPage, &FaultPage::enableRequested,
            m_serial, &DeviceSerialService::requestMotorEnable);
    connect(m_faultPage, &FaultPage::disableRequested,
            m_serial, &DeviceSerialService::requestMotorDisable);
    connect(m_faultPage, &FaultPage::faultResetRequested,
            m_serial, &DeviceSerialService::requestFaultReset);
    connect(m_digitalIoPage, &DigitalIoPage::readAllRequested,
            this, &MainWindow::readAllParameters);
    connect(m_feedbackPage, &FeedbackPage::readAllRequested,
            this, &MainWindow::readAllParameters);
    connect(m_feedbackPage, &FeedbackPage::parameterWriteRequested,
            this, [this](quint16 parameter, quint16 value) {
                m_serial->writeParameter(parameter, value);
            });
    connect(m_feedbackPage, &FeedbackPage::rawFrameRequested,
            m_serial, &DeviceSerialService::sendFrame);
    connect(m_feedbackPage, &FeedbackPage::statusMessage,
            this, [this](const QString &message) {
                statusBar()->showMessage(message, 5000);
            });
    for (DeviceConfigPage *page : {m_motorPage, m_brakePage, m_limitsPage,
                                   m_resistorPage, m_currentLoopPage,
                                   m_velocityLoopPage, m_positionLoopPage,
                                   m_motionModePage}) {
        connect(m_serial, &DeviceSerialService::parameterValueReceived,
                page, &DeviceConfigPage::setParameterValue);
        connect(page, &DeviceConfigPage::readAllRequested,
                this, &MainWindow::readAllParameters);
        connect(page, &DeviceConfigPage::writeRequested,
                this, [this](quint16 parameter, quint16 value) {
                    if (m_serial->writeParameter(parameter, value)) {
                        statusBar()->showMessage(
                            QStringLiteral("已提交 Pn%1 = %2")
                                .arg(parameter, 3, 10, QLatin1Char('0'))
                                .arg(value),
                            4000);
                    }
                });
        connect(page, &DeviceConfigPage::statusMessage,
                this, [this](const QString &message) {
                    statusBar()->showMessage(message, 5000);
                });
    }
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
    m_faultPage->setConnected(connected);
    m_feedbackPage->setConnected(connected);
    for (DeviceConfigPage *page : {m_motorPage, m_brakePage, m_limitsPage,
                                   m_resistorPage, m_currentLoopPage,
                                   m_velocityLoopPage, m_positionLoopPage,
                                   m_motionModePage}) {
        page->setConnected(connected);
    }
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
    if (telemetry.statusKnown) {
        m_faultPage->setMotorEnabled(telemetry.motorEnabled);
        m_feedbackPage->setMotorEnabled(telemetry.motorEnabled);
    }
    m_faultPage->setFaultCode(telemetry.faultCode);
    m_feedbackPage->updateTelemetry(telemetry);
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
        QStringLiteral("电机参数"),
        QStringLiteral("制动器参数"),
        QStringLiteral("限定与保护"),
        QStringLiteral("再生电阻"),
        QStringLiteral("回零方式与轨迹"),
        QStringLiteral("使能、报警与故障"),
        QStringLiteral("数字 I/O"),
        QStringLiteral("反馈与编码器"),
        QStringLiteral("电流环"),
        QStringLiteral("速度环"),
        QStringLiteral("位置环"),
        QStringLiteral("驱动模式与运动配置"),
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
    m_motorPage = new DeviceConfigPage(
        QStringLiteral("电机参数"),
        QStringLiteral("配置电机额定电流与极对数。页面字段及参数编号来自参考版电机页的写入事件。"),
        {
            {85, QStringLiteral("电机额定电流"), QStringLiteral("A"),
             QStringLiteral("驱动器内部按原始值 × 0.01 显示，例如 420 表示 4.20 A。"),
             100.0, 2, 4.20},
            {80, QStringLiteral("电机极对数"), QStringLiteral("对"),
             QStringLiteral("极对数必须与电机铭牌一致，否则可能导致方向、转矩或过载异常。"),
             1.0, 0, 8.0},
        },
        m_stack);
    m_brakePage = new DeviceConfigPage(
        QStringLiteral("制动器参数"),
        QStringLiteral("配置制动器控制方式、动作电压和稳态保持电压。修改后应确认垂直负载不会下滑。"),
        {
            {151, QStringLiteral("制动器控制类型"), QString(),
             QStringLiteral("0 = 伺服 ON/OFF 控制制动器；1 = 强制松开；2 = 强制闭合。"),
             1.0, 0, 0.0,
             {QStringLiteral("伺服 ON/OFF 控制（正常模式）"),
              QStringLiteral("强制松开制动器"),
              QStringLiteral("强制闭合制动器")}},
            {26, QStringLiteral("制动器控制电压"), QStringLiteral("V"),
             QStringLiteral("制动器释放或动作阶段的控制电压。"),
             1.0, 0, 7.0},
            {25, QStringLiteral("制动器稳态保持电压"), QStringLiteral("V"),
             QStringLiteral("制动器保持阶段电压；参考参数文件中默认原始值为 150。"),
             1.0, 0, 150.0},
        },
        m_stack);
    m_limitsPage = new DeviceConfigPage(
        QStringLiteral("限定与保护"),
        QStringLiteral("配置母线电压、过载、额定电流和速度保护。保护阈值应覆盖正常工况并留出安全裕量。"),
        {
            {24, QStringLiteral("母线电压限定"), QStringLiteral("V"),
             QStringLiteral("过压报警与再生制动行为会受该阈值影响。"),
             1.0, 0, 430.0},
            {91, QStringLiteral("瞬时过载保护倍数"), QStringLiteral("×"),
             QStringLiteral("参考 Er006：瞬时软件过载报警会检查该参数。"),
             1.0, 0, 30.0},
            {92, QStringLiteral("长时过载保护倍数"), QStringLiteral("×"),
             QStringLiteral("参考 Er005：长时软件过载报警会检查该参数。"),
             1.0, 0, 15.0},
            {85, QStringLiteral("电机额定电流"), QStringLiteral("A"),
             QStringLiteral("过载保护基准，按原始值 × 0.01 显示。"),
             100.0, 2, 4.20},
            {130, QStringLiteral("电机转速限定"), QStringLiteral("rpm"),
             QStringLiteral("Er014/Er015 表明给定或实际转速超过该值时触发报警。"),
             1.0, 0, 6000.0},
        },
        m_stack);
    m_resistorPage = new DeviceConfigPage(
        QStringLiteral("再生电阻"),
        QStringLiteral("设置再生电阻投入电压阈值。阈值过高可能削弱制动效果，过低可能造成误投入。"),
        {
            {20, QStringLiteral("再生电阻投入电压阈值"), QStringLiteral("V"),
             QStringLiteral("参考 Er020：报警处理需要检查再生电阻接入、功率等级和该阈值。"),
             1.0, 0, 390.0},
        },
        m_stack);
    m_homingPage = new HomingPage(m_stack);
    m_faultPage = new FaultPage(m_stack);
    m_digitalIoPage = new DigitalIoPage(m_stack);
    m_feedbackPage = new FeedbackPage(m_stack);
    m_currentLoopPage = new DeviceConfigPage(
        QStringLiteral("电流环"),
        QStringLiteral("配置电流环比例、积分和反电势补偿。调试前应确认电机额定电流与过载保护。"),
        {
            {31, QStringLiteral("电流环比例增益 KCP"), QString(),
             QStringLiteral("增大可提高动态响应，但过高可能引起噪声或振荡。"), 1.0, 0, 1000.0},
            {32, QStringLiteral("电流环积分增益 KCI"), QString(),
             QStringLiteral("用于减小稳态误差，应结合 KCP 成组调整。"), 1.0, 0, 300.0},
            {141, QStringLiteral("反电势补偿系数 KEMF"), QString(),
             QStringLiteral("参考版当前页显示 Pn141。"), 1.0, 0, 0.0},
            {140, QStringLiteral("反电势补偿开关"), QString(),
             QStringLiteral("参考版将 Pn140 的 0/1 映射为 Disabled/Enabled。"),
             1.0, 0, 0.0,
             {QStringLiteral("Disabled"), QStringLiteral("Enabled")}},
        },
        m_stack);
    m_velocityLoopPage = new DeviceConfigPage(
        QStringLiteral("速度环"),
        QStringLiteral("配置速度环 KVP、KVI 和速度前馈 KVFR；滤波器参数需结合负载惯量调整。"),
        {
            {33, QStringLiteral("速度环比例增益 KVP"), QString(),
             QStringLiteral("参数文件默认原始值 500。"), 1.0, 0, 500.0},
            {34, QStringLiteral("速度环积分增益 KVI"), QString(),
             QStringLiteral("参数文件默认原始值 1000。"), 1.0, 0, 1000.0},
            {35, QStringLiteral("速度前馈 KVFR"), QString(),
             QStringLiteral("参考版速度前馈组对应 Pn035。"), 1.0, 0, 0.0},
        },
        m_stack);
    m_positionLoopPage = new DeviceConfigPage(
        QStringLiteral("位置环"),
        QStringLiteral("配置位置环双增益、积分、前馈和平滑/输出滤波。修改前应保存可恢复的调参方案。"),
        {
            {65, QStringLiteral("位置环比例增益1 KPP1"), QString(),
             QStringLiteral("Er010 建议可适当提高 Pn065 以改善位置偏差。"), 1.0, 0, 21.0},
            {68, QStringLiteral("位置环积分增益 KPI"), QString(),
             QStringLiteral("参考版位置环剩余独立参数对应 Pn068。"), 1.0, 0, 0.0},
            {63, QStringLiteral("位置环比例增益2 KPP2"), QString(),
             QStringLiteral("参数文件默认原始值 2000。"), 1.0, 0, 2000.0},
            {64, QStringLiteral("位置前馈 KPVFR"), QStringLiteral("%"),
             QStringLiteral("参考版性能参数表的百分比字段。"), 1.0, 0, 100.0},
            {66, QStringLiteral("平滑位置指令滤波 PCMDF"), QStringLiteral("%"),
             QStringLiteral("用于平滑位置指令，过大会增加响应延迟。"), 1.0, 0, 0.0},
            {67, QStringLiteral("位置环输出滤波 POUTF"), QStringLiteral("%"),
             QStringLiteral("用于抑制位置环输出高频成分。"), 1.0, 0, 0.0},
        },
        m_stack);
    m_motionModePage = new DeviceConfigPage(
        QStringLiteral("驱动模式与运动配置"),
        QStringLiteral("配置串行电流、串行速度、串行位置、位置齿轮、回零或内部速度模式，"
                       "并集中调整三类控制环性能参数。模式切换应在下使能状态执行。"),
        {
            {1, QStringLiteral("驱动模式 Pn001"), QString(),
             QStringLiteral("0 串行电流；1 串行速度；2 串行位置；3 位置齿轮；4 回零；5 内部速度。"),
             1.0, 0, 0.0,
             {QStringLiteral("串行电流"), QStringLiteral("串行速度"),
              QStringLiteral("串行位置"), QStringLiteral("位置齿轮"),
              QStringLiteral("回零"), QStringLiteral("内部速度")}},
            {31, QStringLiteral("电流环比例增益 KCP"), QString(), QString(), 1.0, 0, 1000.0},
            {32, QStringLiteral("电流环积分增益 KCI"), QString(), QString(), 1.0, 0, 300.0},
            {33, QStringLiteral("速度环比例增益 KVP"), QString(), QString(), 1.0, 0, 500.0},
            {34, QStringLiteral("速度环积分增益 KVI"), QString(), QString(), 1.0, 0, 1000.0},
            {65, QStringLiteral("位置环比例增益1 KPP1"), QString(), QString(), 1.0, 0, 21.0},
            {63, QStringLiteral("位置环比例增益2 KPP2"), QString(), QString(), 1.0, 0, 2000.0},
            {64, QStringLiteral("位置前馈 KPVFR"), QStringLiteral("%"), QString(), 1.0, 0, 100.0},
            {66, QStringLiteral("平滑位置指令滤波 PCMDF"), QStringLiteral("%"), QString(), 1.0, 0, 0.0},
            {67, QStringLiteral("位置环输出滤波 POUTF"), QStringLiteral("%"), QString(), 1.0, 0, 0.0},
            {8, QStringLiteral("脉冲输入类型 Pn008"), QString(),
             QStringLiteral("修改后通常需要重新上电。"),
             1.0, 0, 0.0,
             {QStringLiteral("脉冲+方向（Pulse+Dir）"),
              QStringLiteral("双脉冲（CW+CCW）"),
              QStringLiteral("AB 正交（A+B）")}},
            {58, QStringLiteral("每转指令脉冲数1（高四位）"), QString(), QString(), 1.0, 0, 0.0},
            {59, QStringLiteral("每转指令脉冲数2（低四位）"), QString(), QString(), 1.0, 0, 2000.0},
            {120, QStringLiteral("从站节点地址"), QString(), QString(), 1.0, 0, 1.0},
        },
        m_stack);
    m_stack->addWidget(m_dashboard);
    m_stack->addWidget(m_parameterPage);
    m_stack->addWidget(m_monitorPage);
    m_stack->addWidget(m_communicationPage);
    m_stack->addWidget(m_driveInfoPage);
    m_stack->addWidget(m_motorPage);
    m_stack->addWidget(m_brakePage);
    m_stack->addWidget(m_limitsPage);
    m_stack->addWidget(m_resistorPage);
    m_stack->addWidget(m_homingPage);
    m_stack->addWidget(m_faultPage);
    m_stack->addWidget(m_digitalIoPage);
    m_stack->addWidget(m_feedbackPage);
    m_stack->addWidget(m_currentLoopPage);
    m_stack->addWidget(m_velocityLoopPage);
    m_stack->addWidget(m_positionLoopPage);
    m_stack->addWidget(m_motionModePage);

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
    addPageShortcut(Qt::Key_F6, 5);
    addPageShortcut(Qt::Key_F7, 6);
    addPageShortcut(Qt::Key_F8, 7);
    addPageShortcut(Qt::Key_F9, 8);
    addPageShortcut(Qt::Key_F10, 9);
    addPageShortcut(Qt::Key_F11, 10);
    addPageShortcut(Qt::Key_F12, 11);
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
    navLayout->addWidget(createNavButton(QStringLiteral("电机"), 5));
    navLayout->addWidget(createNavButton(QStringLiteral("反馈"), 12));
    navLayout->addWidget(createNavButton(QStringLiteral("制动器"), 6));
    navLayout->addWidget(createNavButton(QStringLiteral("限定"), 7));
    navLayout->addWidget(createNavButton(QStringLiteral("再生电阻"), 8));
    navLayout->addWidget(createNavButton(QStringLiteral("数字 I/O"), 11));
    navLayout->addWidget(createNavButton(QStringLiteral("控制与标定"), 1,
                                         QStringLiteral("控制与标定")));
    navLayout->addWidget(createNavButton(QStringLiteral("电流环"), 13));
    navLayout->addWidget(createNavButton(QStringLiteral("速度环"), 14));
    navLayout->addWidget(createNavButton(QStringLiteral("位置环"), 15));
    navLayout->addWidget(createNavButton(QStringLiteral("驱动模式"), 16));
    navLayout->addWidget(createNavButton(QStringLiteral("参数复位"), 1,
                                         QStringLiteral("系统"),
                                         QStringLiteral("参数复位与系统功能")));
    navLayout->addWidget(createNavButton(QStringLiteral("系统"), 1,
                                         QStringLiteral("系统")));
    navLayout->addWidget(createNavButton(QStringLiteral("回零"), 9));
    navLayout->addWidget(createNavButton(QStringLiteral("备份与恢复"), 1,
                                         QStringLiteral("全部参数"),
                                         QStringLiteral("参数备份与恢复")));
    navLayout->addWidget(createNavButton(QStringLiteral("使能与故障"), 10));
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
        QLabel#fieldHint {
            color: #64748B;
            background: #F8FAFC;
            border-left: 3px solid #CBD5E1;
            padding: 6px 9px;
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
        QTableWidget#faultTable {
            background: #FFFFFF;
            alternate-background-color: #F8FAFC;
            border: 1px solid #E2E8F0;
            border-radius: 9px;
            gridline-color: #E2E8F0;
            selection-background-color: #FEF3C7;
            selection-color: #78350F;
        }
        QTableWidget#faultTable::item {
            padding: 5px 7px;
        }
        QTableWidget#ioTable {
            background: #FFFFFF;
            alternate-background-color: #F8FAFC;
            border: 1px solid #E2E8F0;
            border-radius: 7px;
            gridline-color: #EEF2F7;
            selection-background-color: #DDF7F1;
            selection-color: #0F172A;
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
