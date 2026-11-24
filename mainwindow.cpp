#include "mainwindow.h"
#include "calibrationsettingsdialog.h"
#include "./ui_mainwindow.h"
#include <QDebug>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QAxObject>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QInputDialog>
#include <QKeySequence>
#include <QLineEdit>
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QDataStream>
#include <QDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , serial(new QSerialPort(this))
    , tcpSocket(new QTcpSocket(this)) //报文
    , m_readTimer(new QTimer(this))  //报文
    , m_heartbeatTimer(new QTimer(this))

{
    ui->setupUi(this);
    ui->tabWidget->setCurrentIndex(0);
    centralWidget()->setFixedSize(1280, 720);
    setWindowIcon(QIcon(":/pic/image/logo.ico"));
    applyFadeAnimation(ui->tabWidget, ui->tabWidget->currentIndex(), 0);
    setWindowTitle("赛威德EHA自主标定系统");

    showImage(":/pic/image/Connection status.png");
    ui->STEP1->setStyleSheet("background-color: #D3D3D3;color:#000000;");
    // ui->tabWidget->tabBar()->hide();
    m_tabAnimation = new QPropertyAnimation(ui->tabWidget, "currentIndex", this);
    m_tabAnimation->setDuration(300);
    m_tabAnimation->setEasingCurve(QEasingCurve::InOutQuad);

    // 初始化时禁用相关按钮
    ui->EHAreset->setEnabled(false);
    ui->EHAcalibration->setEnabled(false);
    ui->EHAzero->setEnabled(false);
    ui->EHAmassconfirm->setEnabled(false);
    // ui->EHAtest_2->setEnabled(false);
    // ui->EHAtest->setEnabled(false);

    // 启动力传感器连接线程

    initializeCalibrationProfiles();
    initActions();
    createMenus();

    connect(tcpSocket, &QTcpSocket::readyRead, this, &MainWindow::onSocketReadyRead);
    connect(m_readTimer, &QTimer::timeout, this, &MainWindow::readModbusData);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &MainWindow::sendHeartbeat);
    connect(ui->tabWidget,&QTabWidget::currentChanged,this,&MainWindow::ontabwidgetindexchanged);
    connect(tcpSocket, &QTcpSocket::disconnected, this, [this]() {
        m_readTimer->stop();
        m_sensorSampleValid = false;
    });
}

MainWindow::~MainWindow()
{
    // 确保线程安全退出

    delete ui;
}

void MainWindow::initActions()
{
    connect(ui->EHAconnect, &QPushButton::clicked,
            this, &MainWindow::onehaconnectButtonClicked);
    connect(ui->EHAreset, &QPushButton::clicked,
            this, &MainWindow::oneharesetButtonClicked);
    connect(ui->EHAcalibration, &QPushButton::clicked,
            this, &MainWindow::onehacalibrationButtonClicked);
    connect(ui->serialportrefresh, &QPushButton::clicked,
            this, &MainWindow::onserialportrefreshButtonClicked);
    connect(ui->EHAzero, &QPushButton::clicked,
            this, &MainWindow::onEHAzeroButtonClicked);
    connect(ui->EHAmassconfirm, &QPushButton::clicked,
            this, &MainWindow::onEHAmassconfirmButtonClicked);
    connect(ui->EHAtest, &QPushButton::clicked,
            this, &MainWindow::onEHAtestButtonClicked);
    connect(ui->EHAtest_2, &QPushButton::clicked,
            this, &MainWindow::onEHAtestButtonClicked);
    connect(serial, &QSerialPort::readyRead,
            this, &MainWindow::onSerialReadyRead);
    connect(ui->openCalibrationSettingsButton, &QPushButton::clicked,
            this, &MainWindow::openCalibrationSettings);
    connect(ui->writeKbButton, &QPushButton::clicked,
            this, &MainWindow::writeKbCalibration);
}

void MainWindow::createMenus()
{
    menuBar()->setNativeMenuBar(false);
    QMenu *settingsMenu = menuBar()->addMenu(QStringLiteral("设置(&S)"));
    m_calibrationSettingsAction =
        settingsMenu->addAction(QStringLiteral("标定设置..."));
    m_calibrationSettingsAction->setObjectName(
        QStringLiteral("calibrationSettingsAction"));
    m_calibrationSettingsAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+,")));
    connect(m_calibrationSettingsAction, &QAction::triggered,
            this, &MainWindow::openCalibrationSettings);

    menuBar()->setStyleSheet(QStringLiteral(R"(
        QMenuBar {
            background: #FFFFFF;
            color: #303844;
            border-bottom: 1px solid #D8DDE4;
            padding: 1px 8px;
            font-family: "Microsoft YaHei UI";
            font-size: 12px;
        }
        QMenuBar::item {
            background: transparent;
            padding: 5px 10px;
            border-radius: 5px;
        }
        QMenuBar::item:selected {
            background: #EEF2F6;
        }
        QMenu {
            background: #FFFFFF;
            color: #303844;
            border: 1px solid #CFD6DF;
            padding: 5px;
            font-family: "Microsoft YaHei UI";
            font-size: 12px;
        }
        QMenu::item {
            padding: 6px 28px 6px 12px;
            border-radius: 4px;
        }
        QMenu::item:selected {
            background: #E8F2F0;
            color: #0F5F58;
        }
    )"));
}

void MainWindow::initializeCalibrationProfiles()
{
    QSettings settings;
    m_calibrationProfile = calibrationProfileById(
        settings.value(QStringLiteral("calibration/model"),
                       QStringLiteral("ECM1A2000")).toString());
    const int savedMaximum = settings.value(
        QStringLiteral("calibration/maxForce"),
        m_calibrationProfile.ratedMaxForceN).toInt();
    m_calibrationProfile.calibrationMaxForceN =
        qBound(1, savedMaximum, m_calibrationProfile.ratedMaxForceN);
    const int savedStep = settings.value(
        QStringLiteral("calibration/step"),
        m_calibrationProfile.stepN).toInt();
    m_calibrationProfile.stepN =
        qBound(1, savedStep, m_calibrationProfile.calibrationMaxForceN);
    m_calibrationProfile.tolerancePercent =
        qBound(0.1, settings.value(QStringLiteral("calibration/tolerancePercent"),
                                   m_calibrationProfile.tolerancePercent).toDouble(),
               10.0);
    if (!m_calibrationProfile.allowsNarrowRange) {
        m_calibrationProfile.calibrationMaxForceN =
            m_calibrationProfile.ratedMaxForceN;
        m_calibrationProfile.stepN = 200;
    }
    m_activeForcePoints = buildCalibrationForcePoints(
        m_calibrationProfile.calibrationMaxForceN, m_calibrationProfile.stepN);
    updateCalibrationSummary();
    updateKbResultUi();
}

void MainWindow::applyCalibrationProfile(const CalibrationProfile &profile)
{
    if (m_operationRunning) {
        return;
    }

    m_calibrationProfile = profile;
    m_activeForcePoints = buildCalibrationForcePoints(
        m_calibrationProfile.calibrationMaxForceN, m_calibrationProfile.stepN);

    m_kbFitValid = false;
    m_calibrationWritten = false;
    m_testComplete = false;
    m_lookupCalibrationComplete = false;

    updateKbResultUi();
    updateCalibrationSummary();
    setOperationRunning(false);

    QSettings settings;
    settings.setValue(QStringLiteral("calibration/model"), profile.id);
    settings.setValue(QStringLiteral("calibration/maxForce"),
                      profile.calibrationMaxForceN);
    settings.setValue(QStringLiteral("calibration/step"), profile.stepN);
    settings.setValue(QStringLiteral("calibration/tolerancePercent"),
                      profile.tolerancePercent);
}

void MainWindow::openCalibrationSettings()
{
    if (m_operationRunning || serial->isOpen()) {
        QMessageBox::information(
            this, QStringLiteral("标定设置"),
            QStringLiteral("设备已连接或正在执行流程，不能修改标定方案。"));
        return;
    }

    CalibrationSettingsDialog dialog(m_calibrationProfile, this);
    if (dialog.exec() == QDialog::Accepted) {
        applyCalibrationProfile(dialog.calibrationProfile());
        ui->textEdit->append(QStringLiteral("已更新标定方案：%1，0–%2 N，%3 N 分段。")
                                 .arg(m_calibrationProfile.displayName)
                                 .arg(m_calibrationProfile.calibrationMaxForceN)
                                 .arg(m_calibrationProfile.stepN));
    }
}

void MainWindow::updateCalibrationSummary()
{
    QStringList allPoints;
    for (int point : m_activeForcePoints) {
        allPoints.append(QString::number(point));
    }
    ui->calibrationSummaryLabel->setToolTip(
        QStringLiteral("测试点：%1 N").arg(allPoints.join(QStringLiteral(" / "))));

    const bool lookup =
        m_calibrationProfile.algorithm() == CalibrationAlgorithm::LookupTable;
    ui->EHAcalibration->setText(
        lookup ? QStringLiteral("执行查表标定")
               : QStringLiteral("采集并计算 K/B"));
    ui->calibrationSummaryLabel->setText(
        QStringLiteral("%1 · 0–%2 N")
            .arg(m_calibrationProfile.displayName)
            .arg(m_calibrationProfile.calibrationMaxForceN));
}

void MainWindow::updateKbResultUi()
{
    const bool lookup =
        m_calibrationProfile.algorithm() == CalibrationAlgorithm::LookupTable;
    ui->calibrationResultGroup->setTitle(
        lookup ? QStringLiteral("查表标定结果")
               : QStringLiteral("K/B 拟合结果"));
    for (QWidget *widget : {
             static_cast<QWidget *>(ui->fitKCaption),
             static_cast<QWidget *>(ui->fitKValue),
             static_cast<QWidget *>(ui->pn168Caption),
             static_cast<QWidget *>(ui->pn168Value),
             static_cast<QWidget *>(ui->fitBCaption),
             static_cast<QWidget *>(ui->fitBValue),
             static_cast<QWidget *>(ui->pn169Caption),
             static_cast<QWidget *>(ui->pn169Spin),
             static_cast<QWidget *>(ui->fitQualityCaption),
             static_cast<QWidget *>(ui->fitQualityValue),
             static_cast<QWidget *>(ui->writeKbButton),
         }) {
        widget->setVisible(!lookup);
    }
    ui->calibrationMethodValue->setText(
        lookup ? QStringLiteral("查表（PN140–189）")
               : QStringLiteral("线性拟合 Pn168 / Pn169"));
    ui->fitKValue->setText(m_kbFitValid ? QString::number(m_fitK, 'f', 4)
                                       : QStringLiteral("--"));
    ui->pn168Value->setText(m_kbFitValid ? QString::number(m_pn168Candidate)
                                        : QStringLiteral("--"));
    ui->fitBValue->setText(m_kbFitValid ? QString::number(m_fitB, 'f', 4)
                                       : QStringLiteral("--"));
    ui->fitQualityValue->setText(
        m_kbFitValid
            ? QStringLiteral("R²=%1  Max=%2 N  RMS=%3 N")
                  .arg(m_fitRSquared, 0, 'f', 5)
                  .arg(m_fitMaxErrorN, 0, 'f', 3)
                  .arg(m_fitRmsErrorN, 0, 'f', 3)
            : QStringLiteral("--"));
    ui->toleranceValue->setText(
        QStringLiteral("±%1 N").arg(m_calibrationProfile.toleranceN(), 0, 'f', 1));
    ui->pn169Spin->setEnabled(!lookup && !m_operationRunning);
    ui->writeKbButton->setEnabled(
        !lookup && m_kbFitValid && serial->isOpen() && !m_operationRunning);
}

void MainWindow::setOperationRunning(bool running)
{
    m_operationRunning = running;
    const bool connected = serial->isOpen();
    const bool settingsEnabled = !running && !connected;
    ui->openCalibrationSettingsButton->setEnabled(settingsEnabled);
    if (m_calibrationSettingsAction) {
        m_calibrationSettingsAction->setEnabled(settingsEnabled);
    }
    ui->EHAconnect->setEnabled(!running);
    ui->EHAreset->setEnabled(!running && connected);
    ui->EHAtest->setEnabled(!running && connected);
    ui->EHAtest_2->setEnabled(!running && connected);
    ui->EHAcalibration->setEnabled(
        !running && connected && m_activeForcePoints.size() >= 2);
    ui->EHAzero->setEnabled(!running && connected && m_calibrationWritten);
    ui->EHAmassconfirm->setEnabled(!running && connected && m_calibrationWritten);
    ui->pn169Spin->setEnabled(
        !running && connected
        && m_calibrationProfile.algorithm() == CalibrationAlgorithm::LinearFit);
    ui->writeKbButton->setEnabled(
        !running && connected && m_kbFitValid
        && m_calibrationProfile.algorithm() == CalibrationAlgorithm::LinearFit);
}

bool MainWindow::ensureReadyForCalibration(bool requireSensor)
{
    if (!serial->isOpen()) {
        ui->textEdit->append(QStringLiteral("错误：请先连接 EHA 串口。"));
        return false;
    }
    if (m_activeForcePoints.size() < 2
        || m_calibrationProfile.calibrationMaxForceN <= 0
        || m_calibrationProfile.stepN <= 0) {
        ui->textEdit->append(QStringLiteral("错误：标定范围或分段无效。"));
        return false;
    }
    if (requireSensor
        && (tcpSocket->state() != QTcpSocket::ConnectedState
            || !m_sensorSampleValid)) {
        ui->textEdit->append(
            QStringLiteral("错误：力传感器未连接或尚未收到有效采样。"));
        return false;
    }
    return true;
}

bool MainWindow::acquireForcePoint(int forceN, quint16 *actualForceN,
                                   QDateTime *sampleTime)
{
    if (!actualForceN) {
        return false;
    }

    const QDateTime commandTime = QDateTime::currentDateTime();
    settleForce(static_cast<quint16>(qBound(0, forceN, 65535)));
    delayMS(2000);

    if (!m_sensorSampleValid || !m_sensorSampleTime.isValid()
        || m_sensorSampleTime < commandTime) {
        ui->textEdit->append(
            QStringLiteral("错误：设定 %1 N 后未收到新的力传感器数据。").arg(forceN));
        return false;
    }

    *actualForceN = static_cast<quint16>(qRound(sensorforceValue / 10.0));
    if (sampleTime) {
        *sampleTime = m_sensorSampleTime;
    }
    return true;
}

bool MainWindow::runLookupCalibration()
{
    if (m_calibrationProfile.id != QStringLiteral("ECM1A2000")) {
        return false;
    }

    QVector<int> lookupPoints;
    for (int point : m_activeForcePoints) {
        if (point > 0) {
            lookupPoints.append(point);
        }
    }
    if (lookupPoints.size() != 10) {
        ui->textEdit->append(
            QStringLiteral("错误：ECM1A2000 查表必须包含 200–2000 N 的 10 个点。"));
        return false;
    }

    if (QMessageBox::question(
            this, QStringLiteral("开始查表标定"),
            QStringLiteral("将按 200–2000 N 执行正反向采样，并写入 "
                           "Pn140–Pn149、Pn180–Pn189。\n\n"
                           "0 N 仅参与检测，不写入查表参数。是否继续？"))
        != QMessageBox::Yes) {
        return false;
    }

    m_calibForwardValues.fill(0, 10);
    m_calibReverseValues.fill(0, 10);
    m_maxForwardDeviationN = 0.0;
    m_maxReverseDeviationN = 0.0;
    m_calibrationWritten = false;

    for (int index = 0; index < lookupPoints.size(); ++index) {
        quint16 actual = 0;
        if (!acquireForcePoint(lookupPoints.at(index), &actual)) {
            settleForce(0);
            return false;
        }
        m_calibForwardValues[index] = actual;
        m_maxForwardDeviationN =
            qMax(m_maxForwardDeviationN,
                 qAbs(static_cast<double>(actual) - lookupPoints.at(index)));
        sendData(static_cast<quint16>(140 + index), actual);
    }

    for (int index = lookupPoints.size() - 1; index >= 0; --index) {
        quint16 actual = 0;
        if (!acquireForcePoint(lookupPoints.at(index), &actual)) {
            settleForce(0);
            return false;
        }
        m_calibReverseValues[index] = actual;
        m_maxReverseDeviationN =
            qMax(m_maxReverseDeviationN,
                 qAbs(static_cast<double>(actual) - lookupPoints.at(index)));
        sendData(static_cast<quint16>(180 + index), actual);
    }

    settleForce(0);
    sendData(152, 1);
    sendData(2, 79);
    m_lookupCalibrationComplete = true;
    m_calibrationWritten = true;
    ui->textEdit->append(
        QStringLiteral("查表标定完成并已发送 Pn152=1。正向最大偏差 %1 N，"
                       "允许 ±%2 N。")
            .arg(m_maxForwardDeviationN, 0, 'f', 1)
            .arg(m_calibrationProfile.toleranceN(), 0, 'f', 1));
    return true;
}

bool MainWindow::runKbCalibration()
{
    if (QMessageBox::question(
            this, QStringLiteral("采集并计算 K/B"),
            QStringLiteral("将按 0–%1 N、每 %2 N 执行正向采样。\n"
                           "反向数据仅作参考，不参与 K/B 拟合。\n\n"
                           "是否开始？")
                .arg(m_calibrationProfile.calibrationMaxForceN)
                .arg(m_calibrationProfile.stepN))
        != QMessageBox::Yes) {
        return false;
    }

    m_testForwardValues.fill(0, m_activeForcePoints.size());
    m_testReverseValues.fill(0, m_activeForcePoints.size());
    m_testResults.clear();
    QVector<QPointF> forwardFitPoints;
    forwardFitPoints.reserve(m_activeForcePoints.size());
    m_maxForwardDeviationN = 0.0;
    m_maxReverseDeviationN = 0.0;
    m_kbFitValid = false;
    m_calibrationWritten = false;

    for (int index = 0; index < m_activeForcePoints.size(); ++index) {
        const int setForce = m_activeForcePoints.at(index);
        quint16 actual = 0;
        if (!acquireForcePoint(setForce, &actual)) {
            settleForce(0);
            return false;
        }
        m_testForwardValues[index] = actual;
        forwardFitPoints.append(QPointF(setForce, actual));
        const double deviation = static_cast<double>(actual) - setForce;
        m_testResults.append({static_cast<quint16>(setForce), deviation});
        m_maxForwardDeviationN =
            qMax(m_maxForwardDeviationN, qAbs(deviation));
    }

    for (int index = m_activeForcePoints.size() - 1; index >= 0; --index) {
        const int setForce = m_activeForcePoints.at(index);
        quint16 actual = 0;
        if (!acquireForcePoint(setForce, &actual)) {
            ui->textEdit->append(
                QStringLiteral("警告：反向参考点 %1 N 未取得有效数据。").arg(setForce));
            continue;
        }
        m_testReverseValues[index] = actual;
        m_maxReverseDeviationN =
            qMax(m_maxReverseDeviationN,
                 qAbs(static_cast<double>(actual) - setForce));
    }

    settleForce(0);
    const LinearFitResult fit = fitLinearForward(forwardFitPoints);
    if (!fit.valid) {
        ui->textEdit->append(QStringLiteral("错误：K/B 线性拟合失败。"));
        return false;
    }

    const int pn168 = calibrationPn168FromK(fit.k);
    if (pn168 < 0 || pn168 > 65535) {
        ui->textEdit->append(
            QStringLiteral("错误：Pn168 计算结果超出 0–65535：%1。")
                .arg(pn168));
        return false;
    }

    m_fitK = fit.k;
    m_fitB = fit.b;
    m_fitRSquared = fit.rSquared;
    m_fitMaxErrorN = fit.maxErrorN;
    m_fitRmsErrorN = fit.rmsErrorN;
    m_pn168Candidate = pn168;
    m_pn169Candidate = ui->pn169Spin->value();
    m_kbFitValid = true;
    m_testComplete = true;
    updateKbResultUi();

    ui->textEdit->append(
        QStringLiteral("K/B 拟合完成：K=%1，Pn168=%2，B=%3。\n"
                       "正向最大偏差 %4 N，允许 ±%5 N；反向参考最大偏差 %6 N。")
            .arg(m_fitK, 0, 'f', 5)
            .arg(m_pn168Candidate)
            .arg(m_fitB, 0, 'f', 5)
            .arg(m_maxForwardDeviationN, 0, 'f', 2)
            .arg(m_calibrationProfile.toleranceN(), 0, 'f', 1)
            .arg(m_maxReverseDeviationN, 0, 'f', 2));
    return true;
}

void MainWindow::writeKbCalibration()
{
    if (!m_kbFitValid || !serial->isOpen()) {
        ui->textEdit->append(QStringLiteral("错误：当前没有可写入的 K/B 结果。"));
        return;
    }

    m_pn169Candidate = ui->pn169Spin->value();
    if (QMessageBox::question(
            this, QStringLiteral("确认写入 K/B"),
            QStringLiteral("拟合 K：%1\nPn168：%2\n\n"
                           "拟合 B：%3\nPn169 原始值：%4\n\n"
                           "写入后将发送 Pn152=1 保存到 EEPROM。是否继续？")
                .arg(m_fitK, 0, 'f', 5)
                .arg(m_pn168Candidate)
                .arg(m_fitB, 0, 'f', 5)
                .arg(m_pn169Candidate),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
        != QMessageBox::Yes) {
        return;
    }

    setOperationRunning(true);
    sendData(168, static_cast<quint16>(qBound(0, m_pn168Candidate, 65535)));
    sendData(169, static_cast<quint16>(qBound(0, m_pn169Candidate, 65535)));
    sendData(152, 1);
    sendData(2, 79);
    m_calibrationWritten = true;
    setOperationRunning(false);
    ui->textEdit->append(
        QStringLiteral("Pn168=%1、Pn169=%2 和 Pn152=1 已发送。")
            .arg(m_pn168Candidate)
            .arg(m_pn169Candidate));
}

void MainWindow::onserialportrefreshButtonClicked()
{
    ui->serialportcomboBox->clear();

    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        QString portInfo = info.portName();
        if (!info.description().isEmpty()) {
            portInfo += " - " + info.description();
        }
        ui->serialportcomboBox->addItem(portInfo);
        // 将调试信息输出到 textEdit
        QString message = QString("发现端口: %1").arg(info.portName());
        ui->textEdit->append(message);
    }

    // 如果没有找到任何端口
    if (ui->serialportcomboBox->count() == 0) {
        ui->textEdit->append("未发现任何串口设备！");
    }
}

void MainWindow::ontabwidgetindexchanged(int index) {
    // 重置所有步骤的样式和图标
    ui->STEP1->setStyleSheet("");
    ui->STEP1->setIcon(QIcon(":/new/prefix1/icon/confirm.png"));
    ui->STEP2->setStyleSheet("");
    ui->STEP2->setIcon(QIcon(":/new/prefix1/icon/confirm.png"));
    ui->STEP3->setStyleSheet("");
    ui->STEP3->setIcon(QIcon(":/new/prefix1/icon/confirm.png"));
    ui->STEP4->setStyleSheet("");
    ui->STEP4->setIcon(QIcon(":/new/prefix1/icon/confirm.png"));
    ui->STEP5->setStyleSheet("");
    ui->STEP5->setIcon(QIcon(":/new/prefix1/icon/confirm.png"));

    // 已完成步骤：深色背景 + 白色文字 + 绿色图标
    for (int i = 0; i < index; ++i) {
        switch(i) {
        case 0:
            ui->STEP1->setStyleSheet("background-color: #43425D; color:#FFFFFF;");
            ui->STEP1->setIcon(QIcon(":/new/prefix1/icon/confirm_green.png"));
            break;
        case 1:
            ui->STEP2->setStyleSheet("background-color: #43425D; color:#FFFFFF;");
            ui->STEP2->setIcon(QIcon(":/new/prefix1/icon/confirm_green.png"));
            break;
        case 2:
            ui->STEP3->setStyleSheet("background-color: #43425D; color:#FFFFFF;");
            ui->STEP3->setIcon(QIcon(":/new/prefix1/icon/confirm_green.png"));
            break;
        case 3:
            ui->STEP4->setStyleSheet("background-color: #43425D; color:#FFFFFF;");
            ui->STEP4->setIcon(QIcon(":/new/prefix1/icon/confirm_green.png"));
            break;
        }
    }

    // 当前步骤样式：灰色背景 + 黑色文字
    switch(index) {
    case 0:
        ui->STEP1->setStyleSheet("background-color: #D3D3D3; color:#000000;");
        // showImage("D:/Saiwider/Code/Qt/EHAcontrol/fig/连接状态_改.png");
        showImage(":/pic/image/Connection status.png");
        break;
    case 1:
        ui->STEP2->setStyleSheet("background-color: #D3D3D3; color:#000000;");
        // showImage("D:/Saiwider/Code/Qt/EHAcontrol/fig/初始化姿态.png");
        showImage(":/pic/image/Initial pose.png");
        break;
    case 2:
        ui->STEP3->setStyleSheet("background-color: #D3D3D3; color:#000000;");
        // showImage("D:/Saiwider/Code/Qt/EHAcontrol/fig/标定姿态.png");
        showImage(":/pic/image/Calibration pose.png");
        break;
    case 3:
        ui->STEP4->setStyleSheet("background-color: #D3D3D3; color:#000000;");
        // showImage("D:/Saiwider/Code/Qt/EHAcontrol/fig/水平姿态.png");
        showImage(":/pic/image/Level pose.png");
        break;
    case 4:
        ui->STEP5->setStyleSheet("background-color: #D3D3D3; color:#000000;");
        // showImage("D:/Saiwider/Code/Qt/EHAcontrol/fig/初始化姿态.png");
        showImage(":/pic/image/Initial pose.png");
        break;
    }
}


void MainWindow::showImage(const QString &imagePath)
{
    qDebug() << "尝试加载图片: " << imagePath;
    QPixmap pixmap(imagePath);
    if (pixmap.isNull()) {
        ui->imageDisplayLabel->setText("图片加载失败: " + imagePath);
        return;
    }

    ui->imageDisplayLabel->setPixmap(pixmap.scaled(ui->imageDisplayLabel->size(),
                                                   Qt::KeepAspectRatio,
                                                   Qt::SmoothTransformation));
}

void MainWindow::onEHAtestButtonClicked()
{
    if (!ensureReadyForCalibration(true)) {
        return;
    }

    if (QMessageBox::question(
            this, QStringLiteral("开始 EHA 检测"),
            QStringLiteral("型号：%1\n标定范围：0–%2 N\n分段：%3 N\n"
                           "允许误差：±%4 N\n\n确认现场安全并开始检测？")
                .arg(m_calibrationProfile.displayName)
                .arg(m_calibrationProfile.calibrationMaxForceN)
                .arg(m_calibrationProfile.stepN)
                .arg(m_calibrationProfile.toleranceN(), 0, 'f', 1))
        != QMessageBox::Yes) {
        return;
    }

    setOperationRunning(true);
    ui->textEdit->append(
        QStringLiteral("开始 EHA 检测：%1，0–%2 N，%3 N 分段。")
            .arg(m_calibrationProfile.displayName)
            .arg(m_calibrationProfile.calibrationMaxForceN)
            .arg(m_calibrationProfile.stepN));

    m_testResults.clear();
    m_testForwardValues.fill(0, m_activeForcePoints.size());
    m_testReverseValues.fill(0, m_activeForcePoints.size());
    m_maxForwardDeviationN = 0.0;
    m_maxReverseDeviationN = 0.0;
    m_testComplete = true;

    ui->textEdit->append(QStringLiteral("\n正向测试（决定合格判定）："));
    for (int index = 0; index < m_activeForcePoints.size(); ++index) {
        const int setForce = m_activeForcePoints.at(index);
        quint16 actualForce = 0;
        if (!acquireForcePoint(setForce, &actualForce)) {
            m_testComplete = false;
            break;
        }

        m_testForwardValues[index] = actualForce;
        const double deviation = static_cast<double>(actualForce) - setForce;
        m_testResults.append({static_cast<quint16>(setForce), deviation});
        m_maxForwardDeviationN =
            qMax(m_maxForwardDeviationN, qAbs(deviation));
        ui->textEdit->append(
            QStringLiteral("设定 %1 N，实际 %2 N，偏差 %3 N")
                .arg(setForce)
                .arg(actualForce)
                .arg(deviation, 0, 'f', 2));
    }

    ui->textEdit->append(QStringLiteral("\n反向测试（仅作参考）："));
    for (int index = m_activeForcePoints.size() - 1; index >= 0; --index) {
        const int setForce = m_activeForcePoints.at(index);
        quint16 actualForce = 0;
        if (!acquireForcePoint(setForce, &actualForce)) {
            ui->textEdit->append(
                QStringLiteral("警告：反向参考点 %1 N 未取得有效数据。").arg(setForce));
            continue;
        }
        m_testReverseValues[index] = actualForce;
        const double deviation = static_cast<double>(actualForce) - setForce;
        m_maxReverseDeviationN =
            qMax(m_maxReverseDeviationN, qAbs(deviation));
        ui->textEdit->append(
            QStringLiteral("设定 %1 N，实际 %2 N，参考偏差 %3 N")
                .arg(setForce)
                .arg(actualForce)
                .arg(deviation, 0, 'f', 2));
    }

    settleForce(0);
    const bool passed =
        m_testComplete
        && m_maxForwardDeviationN <= m_calibrationProfile.toleranceN();
    ui->textEdit->append(
        QStringLiteral("\n正向最大偏差：%1 N；允许范围：±%2 N；结果：%3")
            .arg(m_maxForwardDeviationN, 0, 'f', 2)
            .arg(m_calibrationProfile.toleranceN(), 0, 'f', 1)
            .arg(passed ? QStringLiteral("合格") : QStringLiteral("不合格")));

    showDeviationChart(m_maxForwardDeviationN);
    setOperationRunning(false);
}

// 修改 showDeviationChart 函数，接收 maxDeviation 参数
void MainWindow::showDeviationChart(qreal maxDeviation)
{
    // 创建图表
    m_chart = new QChart();
    m_chart->setTitle("<font size='5' color='#333333'>输出力偏差曲线</font>");
    m_chart->setTitleBrush(QBrush(QColor("#333333")));
    m_chart->setAnimationOptions(QChart::SeriesAnimations);
    m_chart->setBackgroundBrush(QBrush(QColor("#f5f5f5")));
    m_chart->setBackgroundRoundness(10);
    m_chart->setMargins(QMargins(10, 15, 10, 10));

    // 创建曲线系列
    m_series = new QLineSeries();
    m_series->setName("偏差值");
    m_series->setColor(QColor("#3a7bd5"));  // 蓝色曲线
    m_series->setPen(QPen(QBrush(QColor("#3a7bd5")), 3));

    // 添加数据点
    for (const auto &result : qAsConst(m_testResults)) {
        m_series->append(result.first, result.second);
    }

    m_chart->addSeries(m_series);

    // 创建坐标轴
    QValueAxis *axisX = new QValueAxis();
    axisX->setTitleText("<font color='#555555'>设定力</font>");
    axisX->setTitleBrush(QBrush(QColor("#555555")));
    axisX->setLabelFormat("%d");
    axisX->setTickCount(11);
    axisX->setGridLineColor(QColor("#dddddd"));
    axisX->setLabelsColor(QColor("#555555"));
    axisX->setLinePenColor(QColor("#aaaaaa"));

    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("<font color='#555555'>偏差值</font>");
    axisY->setTitleBrush(QBrush(QColor("#555555")));
    axisY->setLabelFormat("%.1f");
    axisY->setGridLineColor(QColor("#dddddd"));
    axisY->setLabelsColor(QColor("#555555"));
    axisY->setLinePenColor(QColor("#aaaaaa"));

    // 设置纵坐标范围
    qreal rangeLimit = qMax(qAbs(maxDeviation),
                            static_cast<qreal>(m_calibrationProfile.toleranceN()))
        + 10;
    if (rangeLimit < 50) {
        axisY->setRange(-rangeLimit, rangeLimit);
    } else {
        axisY->setRange(-rangeLimit, rangeLimit);
    }

    m_chart->addAxis(axisX, Qt::AlignBottom);
    m_chart->addAxis(axisY, Qt::AlignLeft);
    m_series->attachAxis(axisX);
    m_series->attachAxis(axisY);

    // 创建图表视图窗口
    QDialog *chartDialog = new QDialog(this);
    chartDialog->setWindowTitle("力值偏差分析");
    chartDialog->resize(900, 500);  // 调整窗口高度（因为移除了表格）
    chartDialog->setStyleSheet(
        "QDialog {"
        "   background: #f5f5f5;"
        "   border: 1px solid #cccccc;"
        "   border-radius: 8px;"
        "}"
        );

    QVBoxLayout *layout = new QVBoxLayout(chartDialog);
    layout->setContentsMargins(15, 15, 15, 15);
    layout->setSpacing(15);

    // 图表视图
    m_chartView = new QChartView(m_chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setStyleSheet(
        "background: white;"
        "border: 1px solid #dddddd;"
        "border-radius: 6px;"
        );
    layout->addWidget(m_chartView, 1);  // 曲线图占据主要空间

    // 结果判断区域
    QHBoxLayout *resultLayout = new QHBoxLayout();

    // 偏差结果标签
    QLabel *resultLabel = new QLabel();
    resultLabel->setAlignment(Qt::AlignCenter);
    resultLabel->setMinimumHeight(40);

    QString resultMessage;
    const bool passed =
        m_testComplete
        && qAbs(maxDeviation) <= m_calibrationProfile.toleranceN();
    if (passed) {
        resultMessage =
            "<font size='5' color='#4CAF50'>✓ 测试通过 "
            "(正向最大偏差: %1 N，允许 ±%2 N)</font>";
        resultLabel->setStyleSheet(
            "background-color: #E8F5E9;"
            "border: 1px solid #C8E6C9;"
            "border-radius: 6px;"
            "padding: 8px;"
            );
    } else {
        resultMessage =
            "<font size='5' color='#F44336'>✗ 测试未通过 "
            "(正向最大偏差: %1 N，允许 ±%2 N)</font>";
        resultLabel->setStyleSheet(
            "background-color: #FFEBEE;"
            "border: 1px solid #FFCDD2;"
            "border-radius: 6px;"
            "padding: 8px;"
            );
    }
    resultLabel->setText(resultMessage
                             .arg(QString::number(qAbs(maxDeviation), 'f', 2))
                             .arg(m_calibrationProfile.toleranceN(), 0, 'f', 1));
    resultLayout->addWidget(resultLabel, 1);  // 结果标签占据大部分宽度

    // 保存按钮
    QPushButton *saveButton = new QPushButton("保存图表");
    saveButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #3a7bd5;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 4px;"
        "   padding: 8px 16px;"
        "   font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #2c5fb3;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #1e4a9a;"
        "}"
        );
    saveButton->setCursor(Qt::PointingHandCursor);
    resultLayout->addWidget(saveButton);  // 保存按钮靠右

    // 输出标定报告按钮
    QPushButton *exportReportButton = new QPushButton("输出标定报告");
    exportReportButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #4CAF50;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 4px;"
        "   padding: 8px 16px;"
        "   font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #388E3C;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #2E7D32;"
        "}"
        );
    exportReportButton->setCursor(Qt::PointingHandCursor);
    resultLayout->addWidget(exportReportButton);

    layout->addLayout(resultLayout);  // 添加结果区域到底部

    // 保存按钮连接
    connect(saveButton, &QPushButton::clicked, this, [this]() {
        QString fileName = QFileDialog::getSaveFileName(
            this,
            "保存图表",
            QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
            "PNG 图片 (*.png);;JPEG 图片 (*.jpg *.jpeg)"
            );
        if (!fileName.isEmpty()) {
            QPixmap p = m_chartView->grab();
            if (p.save(fileName)) {
                ui->textEdit->append(QString("图表已保存到: %1").arg(fileName));
            } else {
                ui->textEdit->append("保存失败，请检查路径和权限");
            }
        }
    });

    // 输出标定报告按钮连接
    connect(exportReportButton, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        QString deviceId = QInputDialog::getText(
            this,
            "设备编号",
            "请输入本设备编号(如 D00025):",
            QLineEdit::Normal,
            "",
            &ok
            );
        if (!ok) {
            return;
        }

        if (!readCalibParamsFromDevice()) {
            return;
        }
        QString fileName = QFileDialog::getSaveFileName(
            this,
            "保存标定报告",
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/标定报告.xlsx",
            "Excel 工作簿 (*.xlsx)"
            );
        if (!fileName.isEmpty()) {
            exportCalibrationReport(fileName, deviceId);
        }
    });

    chartDialog->exec();
}

bool MainWindow::readCalibParamsFromDevice()
{
    if (!serial->isOpen()) {
        ui->textEdit->append("错误：串口未连接，无法读取PN参数！");
        return false;
    }

    m_calibForwardValues.fill(0, 10);  // PN140~PN149
    m_calibReverseValues.fill(0, 10);  // PN180~PN189
    m_pn170Value = 0;                  // PN170
    m_calibParametersSeen.clear();
    m_calibReadComplete = false;
    m_serialRxBuffer.clear();
    m_waitingForCalibParams = true;
    m_heartbeatTimer->stop();  // 读取参数期间停止心跳，避免干扰设备推送

    const quint8 readAllCmd[] = { 0x55, 0x55, 0xFE, 0xFE, 0xEF, 0xEF, 0xAA, 0xAA };
    serial->write(reinterpret_cast<const char*>(readAllCmd), sizeof(readAllCmd));
    ui->textEdit->append("正在读取设备 PN140~149 / PN180~189 / PN170 参数，请稍候...");

    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(this, &MainWindow::calibParamsReadFinished, &loop, &QEventLoop::quit);
    timeoutTimer.start(5000);  // 5秒超时
    loop.exec();

    m_heartbeatTimer->start(100);

    if (m_waitingForCalibParams || !m_calibReadComplete) {
        m_waitingForCalibParams = false;
        ui->textEdit->append(
            QStringLiteral("错误：PN140–149、PN180–189、Pn170 未完整读取，"
                           "请检查设备连接和回包。"));
        return false;
    }
    ui->textEdit->append("PN140~149 / PN180~189 参数读取完毕");
    return true;
}

void MainWindow::exportCalibrationReport(const QString &savePath, const QString &deviceId)
{
    if (!m_testComplete
        || m_testForwardValues.size() != m_activeForcePoints.size()
        || m_testReverseValues.size() != m_activeForcePoints.size()) {
        ui->textEdit->append("错误：EHA检测数据不完整，请先完成一次EHA检测！");
        return;
    }

    const bool lookup =
        m_calibrationProfile.algorithm() == CalibrationAlgorithm::LookupTable;
    if (lookup
        && (!m_lookupCalibrationComplete
            || m_calibForwardValues.size() < 10
            || m_calibReverseValues.size() < 10)) {
        ui->textEdit->append("错误：ECM1A2000 查表标定数据不完整！");
        return;
    }
    if (!lookup && (!m_kbFitValid || !m_calibrationWritten)) {
        ui->textEdit->append("错误：K/B 尚未计算或尚未写入设备！");
        return;
    }

    const QString templatePath = QCoreApplication::applicationDirPath() + "/ECM1B2000标定报告.xlsx";
    if (!QFile::exists(templatePath)) {
        ui->textEdit->append("错误：未找到标定报告模板：" + templatePath);
        return;
    }

    QAxObject excel("Excel.Application");
    if (excel.isNull()) {
        ui->textEdit->append("错误：无法启动 Excel，请确认本机已安装 Microsoft Excel！");
        return;
    }
    excel.setProperty("Visible", false);
    excel.setProperty("DisplayAlerts", false);

    QAxObject *workbooks = excel.querySubObject("Workbooks");
    QAxObject *workbook = workbooks->querySubObject("Open(const QString&)", QDir::toNativeSeparators(templatePath));
    if (!workbook) {
        ui->textEdit->append("错误：无法打开标定报告模板！");
        excel.dynamicCall("Quit()");
        return;
    }

    QAxObject *sheets = workbook->querySubObject("Worksheets");
    QAxObject *sheet = sheets->querySubObject("Item(int)", 1); // 第一张表"耐久测试"
    if (!sheets || !sheet) {
        ui->textEdit->append("错误：标定报告模板缺少预期工作表！");
        workbook->dynamicCall("Close()");
        excel.dynamicCall("Quit()");
        delete sheet;
        delete sheets;
        delete workbook;
        delete workbooks;
        return;
    }

    const auto setCell = [](QAxObject *worksheet, int row, int column,
                            const QVariant &value) {
        QAxObject *cell = worksheet->querySubObject("Cells(int,int)", row, column);
        if (cell) {
            cell->setProperty("Value", value);
            delete cell;
        }
    };

    // 型号和设备编号 → B2(与F2合并)
    setCell(sheet, 2, 2,
            QStringLiteral("%1 / %2")
                .arg(m_calibrationProfile.displayName,
                     deviceId.trimmed().isEmpty()
                         ? QStringLiteral("未填写设备编号")
                         : deviceId.trimmed()));

    // PN170 → E6
    setCell(sheet, 6, 5, m_pn170Value);

    QAxObject *testRange = sheet->querySubObject("Range(const QString&)", "C8:D18");
    if (testRange) {
        testRange->dynamicCall("ClearContents()");
        delete testRange;
    }

    // 正向实际力 → C8~C18
    const int firstSheetCount = qMin(11, m_testForwardValues.size());
    for (int i = 0; i < firstSheetCount; ++i) {
        setCell(sheet, 8 + i, 3, m_testForwardValues.at(i));
    }

    // 反向实际力 → D8~D18（同一行对应同一设定力档位）
    for (int i = 0; i < firstSheetCount; ++i) {
        setCell(sheet, 8 + i, 4, m_testReverseValues.at(i));
    }

    QAxObject *lookupRange = sheet->querySubObject("Range(const QString&)", "H8:J17");
    if (lookupRange) {
        lookupRange->dynamicCall("ClearContents()");
        delete lookupRange;
    }
    if (lookup) {
        // PN140~PN149 → H8~H17
        for (int i = 0; i < qMin(10, m_calibForwardValues.size()); ++i) {
            setCell(sheet, 8 + i, 8, m_calibForwardValues.at(i));
        }
        // PN180~PN189 → J8~J17
        for (int i = 0; i < qMin(10, m_calibReverseValues.size()); ++i) {
            setCell(sheet, 8 + i, 10, m_calibReverseValues.at(i));
        }
    }

    const bool passed =
        m_testComplete
        && m_maxForwardDeviationN <= m_calibrationProfile.toleranceN();
    setCell(sheet, 21, 2, deviceId.trimmed());
    setCell(sheet, 22, 2, m_calibrationProfile.displayName);
    setCell(sheet, 23, 2, m_calibrationProfile.ratedMaxForceN);
    setCell(sheet, 24, 2, m_calibrationProfile.calibrationMaxForceN);
    setCell(sheet, 25, 2, m_calibrationProfile.stepN);
    setCell(sheet, 26, 2,
            lookup ? QStringLiteral("查表") : QStringLiteral("K/B 拟合"));
    setCell(sheet, 27, 2, m_calibrationProfile.tolerancePercent / 100.0);
    setCell(sheet, 28, 2, m_calibrationProfile.toleranceN());
    setCell(sheet, 29, 2,
            lookup ? QVariant(QStringLiteral("不适用"))
                   : QVariant(m_fitK));
    setCell(sheet, 30, 2,
            lookup ? QVariant(QStringLiteral("不适用"))
                   : QVariant(m_pn168Candidate));
    setCell(sheet, 31, 2,
            lookup ? QVariant(QStringLiteral("不适用"))
                   : QVariant(m_fitB));
    setCell(sheet, 32, 2,
            lookup ? QVariant(QStringLiteral("不适用"))
                   : QVariant(m_pn169Candidate));
    setCell(sheet, 33, 2,
            lookup ? QVariant(QStringLiteral("不适用"))
                   : QVariant(m_fitRSquared));
    setCell(sheet, 34, 2, m_maxForwardDeviationN);
    setCell(sheet, 35, 2, m_maxReverseDeviationN);
    setCell(sheet, 36, 2,
            passed ? QStringLiteral("合格") : QStringLiteral("不合格"));

    QAxObject *detailSheet =
        sheets->querySubObject("Item(const QVariant&)",
                               QVariant(QStringLiteral("标定明细")));
    if (detailSheet) {
        QAxObject *detailRange =
            detailSheet->querySubObject("Range(const QString&)", "A2:F300");
        if (detailRange) {
            detailRange->dynamicCall("ClearContents()");
            delete detailRange;
        }

        for (int index = 0; index < m_activeForcePoints.size(); ++index) {
            const int setForce = m_activeForcePoints.at(index);
            const int forwardActual = m_testForwardValues.at(index);
            const int reverseActual = m_testReverseValues.at(index);
            const double deviation = static_cast<double>(forwardActual) - setForce;
            setCell(detailSheet, index + 2, 1, index + 1);
            setCell(detailSheet, index + 2, 2, setForce);
            setCell(detailSheet, index + 2, 3, forwardActual);
            setCell(detailSheet, index + 2, 4, deviation);
            setCell(detailSheet, index + 2, 5, reverseActual);
            setCell(detailSheet, index + 2, 6,
                    qAbs(deviation) <= m_calibrationProfile.toleranceN()
                        ? QStringLiteral("合格")
                        : QStringLiteral("不合格"));
        }
        delete detailSheet;
    }

    workbook->dynamicCall("SaveAs(const QString&)", QDir::toNativeSeparators(savePath));
    workbook->dynamicCall("Close()");
    excel.dynamicCall("Quit()");

    delete sheet;
    delete sheets;
    delete workbook;
    delete workbooks;

    ui->textEdit->append("标定报告已保存至：" + savePath);
}

void MainWindow::onehaconnectButtonClicked()
{
    if (serial->isOpen()) {
        m_heartbeatTimer->stop();
        m_readTimer->stop();
        tcpSocket->abort();
        m_sensorSampleValid = false;
        m_modbusRxBuffer.clear();
        serial->close();
        ui->EHAconnect->setText("连接");
        ui->textEdit->append("已断开串口连接");
        m_calibrationWritten = false;
        m_kbFitValid = false;
        m_testComplete = false;
        updateKbResultUi();
        setOperationRunning(false);
        return;
    }
    QString ipAddress = ui->IPaddress->text().trimmed();  // 去除前后空格


    // 验证IP地址格式（简单校验）
    QRegularExpression ipRegex("^\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}$");
    if (!ipRegex.match(ipAddress).hasMatch()) {
        ui->textEdit->append("错误：IP地址格式无效！");
        return;
    }

    // 固定端口号为502，连接TCP Socket
    m_sensorSampleValid = false;
    m_modbusRxBuffer.clear();
    m_calibrationWritten = false;
    m_kbFitValid = false;
    m_testComplete = false;
    tcpSocket->connectToHost(ipAddress, 502);
    if (tcpSocket->waitForConnected(1000)) {
        ui->textEdit->append("力传感器连接成功");
        m_readTimer->start(1000);
    } else {
        ui->textEdit->append("力传感器连接失败: " + tcpSocket->errorString());
        ui->textEdit->append("请检查力传感器电源、IP 地址和网络后重新连接。");
        tcpSocket->abort();
        return;
    }

    // showImage("D:/Saiwider/Code/Qt/EHAcontrol/fig/初始化姿态.png");
    showImage(":/pic/image/Initial pose.png");
    QString portName = ui->serialportcomboBox->currentText().split(" ").first();
    serial->setPortName(portName);

    // 设置串口参数
    serial->setBaudRate(QSerialPort::Baud115200);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);

    if (serial->open(QIODevice::ReadWrite)) {
        qDebug() << "[连接] 串口已打开：" << portName;

        // 协议规定：连接后先发握手包，设备才开始通信
        const quint8 handshake[] = { 0x55, 0x55, 0xB9, 0x9B, 0x9B, 0xB9, 0xAA, 0xAA };
        m_serialRxBuffer.clear();
        m_captureOnce = true;
        serial->write(reinterpret_cast<const char*>(handshake), sizeof(handshake));
        serial->flush();
        // qDebug() << "[连接] 已发送握手包";
        // QTimer::singleShot(1000, this, [this]() {
        //     m_captureOnce = false;
        //     qDebug() << "[握手响应结束] 缓冲区共" << m_serialRxBuffer.size() << "字节:" << m_serialRxBuffer.toHex(' ');
        // });

        delayMS(50);                // 等设备就绪
        sendData(2, 79);            // Pn002=66：参数监视=转矩反馈
        m_heartbeatTimer->start(100);
        qDebug() << "[连接] 已发送 Pn002=79，心跳已启动";

        ui->EHAconnect->setText("断开");
        ui->textEdit->append(QString("已连接到 %1，请根据需求选择下列功能").arg(portName));
        ui->textEdit->append(
            QStringLiteral("标定配置：%1，%2，0–%3 N，%4 N 分段，允许误差 ±%5 N。")
                .arg(m_calibrationProfile.displayName,
                     m_calibrationProfile.algorithm() == CalibrationAlgorithm::LookupTable
                         ? QStringLiteral("查表")
                         : QStringLiteral("K/B 拟合"))
                .arg(m_calibrationProfile.calibrationMaxForceN)
                .arg(m_calibrationProfile.stepN)
                .arg(m_calibrationProfile.toleranceN(), 0, 'f', 1));
        ui->textEdit->append("EHA标定，点击《EHA初始化》按钮，初始化需要设备姿态调整为竖直 不与标定工具接触");
        ui->textEdit->append("EHA检测，点击《EHA检测》按钮，检测需要设备姿态调整为竖直，接触标定工具，下压量保持在滚压设备零点附近");
        setOperationRunning(false);
        //EHA连接正常 步骤栏变色 跳到下一步
        applyFadeAnimation(ui->tabWidget, ui->tabWidget->currentIndex(), 1);
        ui->STEP1->setIcon(QIcon(":/new/prefix1/icon/confirm_green.png"));
        ui->STEP1->setStyleSheet("background-color: #43425D;color:#FFFFFF;");

        ui->STEP2->setStyleSheet("background-color: #D3D3D3;color:#000000;");

        // ui->EHAtest->setEnabled(true);
    } else {
        ui->textEdit->append("错误：无法打开串口！");
        ui->textEdit->append(QString("错误详情: %1").arg(serial->errorString()));
        m_readTimer->stop();
        tcpSocket->abort();
    }
}

void MainWindow::oneharesetButtonClicked()
{
    if (!ensureReadyForCalibration(false)) {
        return;
    }
    if (QMessageBox::question(
            this, QStringLiteral("EHA 初始化"),
            QStringLiteral("该操作会复位内部参数、清零质量和力矩基准，并写入初始化参数。\n"
                           "请确认设备处于安全姿态。是否继续？"))
        != QMessageBox::Yes) {
        return;
    }

    setOperationRunning(true);
    //pn168=150，pn169=0；pn046=150，pn047=0；pn052=150，pn053=0
    ui->textEdit->append("步骤二:EHA初始化。。。");
    sendData(0,9999);
    sendData(169, 0);
    sendData(172, 3);
    ui->textEdit->append("内参初始化完毕");
    //质量清零
    sendData(48, 0);
    sendData(170, 0);
    ui->textEdit->append("质量初始化完毕");
    //pn102清零
    sendData(44, 200);
    MainWindow::delayMS(2000);
    sendData(102, 1);
    sendData(102, 0);
    sendData(44, 0);
    sendData(152, 0);// 切换到标定模式
    sendData(2,81); //数显屏设置为显示位移量
    ui->textEdit->append("设定力初始化完毕\n标定时设备需接触力传感器，并下压至数显屏数值在0(位移零点)左右");

    m_calibrationWritten = false;
    m_kbFitValid = false;
    m_testComplete = false;
    updateKbResultUi();
    setOperationRunning(false);
    ui->textEdit->append("初始化完成，机器人下压到位后点击《EHA标定》按钮开始标定");
}
void MainWindow::onehacalibrationButtonClicked()
{
    if (!ensureReadyForCalibration(true)) {
        return;
    }

    setOperationRunning(true);
    ui->textEdit->clear();

    const bool lookup =
        m_calibrationProfile.algorithm() == CalibrationAlgorithm::LookupTable;
    const bool ok = lookup ? runLookupCalibration() : runKbCalibration();
    if (!ok) {
        settleForce(0);
        if (lookup) {
            ui->textEdit->append(
                QStringLiteral("警告：查表流程可能已经写入部分 Pn140–149 或 "
                               "Pn180–189，请修复通信后重新执行完整标定。"));
        }
    }

    setOperationRunning(false);
    ui->textEdit->append(
        ok ? (lookup
                  ? QStringLiteral("查表标定流程结束。")
                  : QStringLiteral("K/B 采集完成，请核对结果后手动确认写入。"))
           : QStringLiteral("标定流程未完整结束。"));
}

void MainWindow::sendData(quint16 pn, quint16 value)
{
    if (!serial->isOpen()) {
        ui->textEdit->append("错误：串口未连接！");
        return;
    }

    // 构造数据：55 55 PnH PnL ValueH ValueL AA AA
    QByteArray data;
    data.append(0x55);  // 起始字节1
    data.append(0x55);  // 起始字节2

    // Pn参数 (高位在前)
    data.append(static_cast<char>((pn >> 8) & 0xFF));  // Pn高字节
    data.append(static_cast<char>(pn & 0xFF));         // Pn低字节

    // 值 (高位在前)
    data.append(static_cast<char>((value >> 8) & 0xFF));  // 值高字节
    data.append(static_cast<char>(value & 0xFF));         // 值低字节

    data.append(0xAA);  // 结束字节1
    data.append(0xAA);  // 结束字节2

    // 发送数据
    qint64 bytesWritten = serial->write(data);
    if (bytesWritten == -1) {
        ui->textEdit->append("发送数据失败！");
    } else if (bytesWritten != data.size()) {
        ui->textEdit->append("发送数据不完整！");
    } else {
        // ui->textEdit->append(QString("已发送数据: %1").arg(QString(data.toHex(' '))));
    }

    if (bytesWritten == data.size()) {
        // 纯延时 100ms（非阻塞）
        MainWindow::delayMS(200); // 等待 200ms，期间不阻塞事件循环
    }
}

void MainWindow::settleForce(quint16 forceValue)
{
    // Pn044
    const quint16 pn = 0x002C;

    sendData(pn, forceValue);

    ui->textEdit->append(QString("设置值: %1")
                             .arg(forceValue));
}

void MainWindow::onEHAzeroButtonClicked()
{
    if (!ensureReadyForCalibration(false)) {
        return;
    }
    if (QMessageBox::question(
            this, QStringLiteral("力矩清零"),
            QStringLiteral("将施加 200 N 预紧力后执行 Pn102 力矩清零。\n"
                           "请确认机械姿态和现场安全。是否继续？"))
        != QMessageBox::Yes) {
        return;
    }

    setOperationRunning(true);
    // showImage("D:/Saiwider/Code/Qt/EHAcontrol/fig/水平姿态.png");
    showImage(":/pic/image/Level pose.png");
    // 1. 力值归零初始化
    settleForce(0);
    ui->textEdit->append("步骤四:力矩清零...");
    settleForce(200);
    MainWindow::delayMS(2000);
    sendData(102, 1);
    sendData(102, 0);
    ui->textEdit->append("步骤四:力矩清零完成 调整滚压设备为竖直状态 观察数显屏数值");
    ui->textEdit->append("步骤五:数显屏为实时力数值 通过填写质量补偿数值 将数显屏数值补偿到0");
    sendData(2, 79);
    MainWindow::delayMS(2000);
    settleForce(0);
    setOperationRunning(false);
}

void MainWindow::onEHAmassconfirmButtonClicked()
{
    if (!ensureReadyForCalibration(false)) {
        return;
    }

    bool ok = false;
    const int EHAmass = ui->EHAmass->text().trimmed().toInt(&ok);
    if (!ok || EHAmass < 0 || EHAmass > 65535) {
        ui->textEdit->append(
            QStringLiteral("错误：质量补偿必须是 0–65535 的整数。"));
        return;
    }
    if (QMessageBox::question(
            this, QStringLiteral("写入质量补偿"),
            QStringLiteral("将写入 Pn160=0、Pn170=%1，并执行 Pn152=1 保存。"
                           "是否继续？").arg(EHAmass))
        != QMessageBox::Yes) {
        return;
    }

    setOperationRunning(true);
    sendData(160,0);
    sendData(170,EHAmass);
    sendData(152,1);
    setOperationRunning(false);
    ui->textEdit->append(
        QStringLiteral("步骤五:质量补偿已录入并发送保存命令。"
                       "观察数显屏数值，显示值为0则标定完毕。"));
}

void MainWindow::delayMS(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}


void MainWindow::readModbusData()
{
    if (tcpSocket->state() != QTcpSocket::ConnectedState) return;

    ++m_modbusTransactionId;

    QByteArray request;
    QDataStream stream(&request, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    // Modbus TCP请求报文
    stream << m_modbusTransactionId << (quint16)0x0000 << (quint16)0x0006;
    stream << (quint8)0x01 << (quint8)0x03 << (quint16)1542 << (quint16)2;

    tcpSocket->write(request);
}

void MainWindow::onSocketReadyRead()
{
    m_modbusRxBuffer.append(tcpSocket->readAll());

    while (m_modbusRxBuffer.size() >= 6) {
        const quint16 length =
            (static_cast<quint8>(m_modbusRxBuffer.at(4)) << 8)
            | static_cast<quint8>(m_modbusRxBuffer.at(5));
        const int frameSize = 6 + length;
        if (length < 2 || length > 260) {
            m_modbusRxBuffer.remove(0, 1);
            continue;
        }
        if (m_modbusRxBuffer.size() < frameSize) {
            break;
        }

        const QByteArray frame = m_modbusRxBuffer.left(frameSize);
        m_modbusRxBuffer.remove(0, frameSize);
        parseModbusResponse(frame);
    }

    if (m_modbusRxBuffer.size() > 1024) {
        m_modbusRxBuffer.clear();
        m_sensorSampleValid = false;
    }
}

void MainWindow::parseModbusResponse(const QByteArray &data)
{
    if (data.length() < 9) return;

    QDataStream stream(data);
    stream.setByteOrder(QDataStream::BigEndian);

    quint16 transactionId, protocolId, length;
    quint8 unitId, functionCode;
    stream >> transactionId >> protocolId >> length >> unitId >> functionCode;

    if (protocolId != 0x0000 || unitId != 0x01 || length != 0x0007) {
        return;
    }

    if (functionCode == 0x03 || functionCode == 0x04) {
        quint8 byteCount;
        stream >> byteCount;

        if (byteCount == 4 && data.length() >= 11) {
            quint8 lastByte = data[data.length()-1];
            quint8 secondLastByte = data[data.length()-2];
            sensorforceValue = (quint16)((secondLastByte << 8) | lastByte);
            m_sensorSampleTime = QDateTime::currentDateTime();
            m_sensorSampleValid = true;
        }
    } else if (functionCode & 0x80) {
        ui->textEdit->append(
            QStringLiteral("力传感器 Modbus 异常响应，功能码 0x%1。")
                .arg(functionCode, 2, 16, QLatin1Char('0')).toUpper());
        m_sensorSampleValid = false;
    }
}

void MainWindow::on_NextStep1_clicked(bool) {
    applyFadeAnimation(ui->tabWidget, ui->tabWidget->currentIndex(), 1);
    // ui->STEP1->setIcon(QIcon(":/new/prefix1/icon/confirm_green.png"));
    // ui->STEP1->setStyleSheet("background-color: #43425D;color:#FFFFFF;");
    // ui->STEP2->setStyleSheet("background-color: #D3D3D3;color:#000000;");
}

void MainWindow::on_NextStep2_clicked(bool) {
    applyFadeAnimation(ui->tabWidget, ui->tabWidget->currentIndex(), 2);
    // ui->STEP2->setIcon(QIcon(":/new/prefix1/icon/confirm_green.png"));
    // ui->STEP2->setStyleSheet("background-color: #43425D;color:#FFFFFF;");
    // ui->STEP3->setStyleSheet("background-color: #D3D3D3;color:#000000;");
}

void MainWindow::on_ForwardStep1_clicked(bool) {
    applyFadeAnimation(ui->tabWidget, ui->tabWidget->currentIndex(), 0);
}

void MainWindow::on_NextStep3_clicked(bool) {
    applyFadeAnimation(ui->tabWidget, ui->tabWidget->currentIndex(), 3);
    // ui->STEP3->setIcon(QIcon(":/new/prefix1/icon/confirm_green.png"));
    // ui->STEP3->setStyleSheet("background-color: #43425D;color:#FFFFFF;");
    // ui->STEP4->setStyleSheet("background-color: #D3D3D3;color:#000000;");
}

void MainWindow::on_ForwardStep2_clicked(bool) {
    applyFadeAnimation(ui->tabWidget, ui->tabWidget->currentIndex(), 1);
}

void MainWindow::on_NextStep4_clicked(bool) {
    applyFadeAnimation(ui->tabWidget, ui->tabWidget->currentIndex(), 4);
    // ui->STEP4->setIcon(QIcon(":/new/prefix1/icon/confirm_green.png"));
    // ui->STEP4->setStyleSheet("background-color: #43425D;color:#FFFFFF;");
    // ui->STEP5->setStyleSheet("background-color: #D3D3D3;color:#000000;");
}

void MainWindow::on_ForwardStep3_clicked(bool) {
    applyFadeAnimation(ui->tabWidget, ui->tabWidget->currentIndex(), 2);
}

void MainWindow::on_ForwardStep4_clicked(bool) {
    applyFadeAnimation(ui->tabWidget, ui->tabWidget->currentIndex(), 3);
}

void MainWindow::onSerialReadyRead()
{
    QByteArray newBytes = serial->readAll();

    // if (m_captureOnce && !newBytes.isEmpty()) {
    //     qDebug() << "[握手响应帧]" << newBytes.size() << "字节:" << newBytes.toHex(' ');
    // }

    static qint64 lastRxPrint = 0;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - lastRxPrint >= 1000) {
        // ui->textEdit->append(QString("[串口 RX] %1").arg(QString(newBytes.toHex(' '))));
        lastRxPrint = now;
    }
    m_serialRxBuffer.append(newBytes);

    // 异步等待 PN140~149 / PN180~189：只扫描新增区域（含跨包边界的6字节重叠）
    if (m_waitingForCalibParams) {
        int scanFrom = qMax(0, m_serialRxBuffer.size() - newBytes.size() - 6);
        for (int i = scanFrom; i <= m_serialRxBuffer.size() - 7; ++i) {
            if ((quint8)m_serialRxBuffer[i]   == 0xEF &&
                (quint8)m_serialRxBuffer[i+1] == 0xAD &&
                (quint8)m_serialRxBuffer[i+2] == 0xAA) {
                quint16 pn  = ((quint8)m_serialRxBuffer[i+3] << 8) | (quint8)m_serialRxBuffer[i+4];
                quint16 val = ((quint8)m_serialRxBuffer[i+5] << 8) | (quint8)m_serialRxBuffer[i+6];
                if (pn >= 140 && pn <= 149) {
                    m_calibForwardValues[pn - 140] = val;
                    m_calibParametersSeen.insert(pn);
                } else if (pn >= 180 && pn <= 189) {
                    m_calibReverseValues[pn - 180] = val;
                    m_calibParametersSeen.insert(pn);
                } else if (pn == 170) {
                    m_pn170Value = val;
                    m_calibParametersSeen.insert(pn);
                }
                if (pn == 0x00C7) {  // PN199，最后一个参数
                    m_waitingForCalibParams = false;
                    m_calibReadComplete = m_calibParametersSeen.size() == 21;
                    emit calibParamsReadFinished();
                    break;
                }
            }
        }
    }

    for (int i = 0; i <= m_serialRxBuffer.size() - 8; ++i) {
        if ((quint8)m_serialRxBuffer[i]   != 0xEF ||
            (quint8)m_serialRxBuffer[i+1] != 0xAD ||
            (quint8)m_serialRxBuffer[i+2] != 0xAB)
            continue;

        quint8 type = (quint8)m_serialRxBuffer[i+3];
        quint8 b4   = (quint8)m_serialRxBuffer[i+4];
        quint8 b5   = (quint8)m_serialRxBuffer[i+5];
        quint8 b6   = (quint8)m_serialRxBuffer[i+6];
        quint8 b7   = (quint8)m_serialRxBuffer[i+7];
        quint32 rawU = ((quint32)b4 << 24) | ((quint32)b5 << 16) | ((quint32)b6 << 8) | b7;
        switch (type) {
        case 0x07:
            ui->EHAforceR->setText(QString("%1").arg(rawU));
            // ui->textEdit->append(QString("[参数监视] uint32=%1  int32=%2").arg(rawU).arg(rawS));
            break;
        case 0x1A:
            // qDebug() << QString("[母线电压] uint32=%1").arg(rawU);
            break;
        case 0x08:
            // qDebug() << QString("[电流]     int32=%1").arg(rawS);
            break;
        case 0x09:
            // qDebug() << QString("[速度]     int32=%1").arg(rawS);
            break;
        case 0x0B:
            // qDebug() << QString("[单圈位置] uint32=%1").arg(rawU);
            break;
        case 0x0A:
            // qDebug() << QString("[多圈位置] uint32=%1").arg(rawU);
            break;
        case 0x0C:
            // qDebug() << QString("[位置]     int32=%1").arg(rawS);
            break;
        default:
            break;
        }
    }

    if (m_serialRxBuffer.size() > 4096)
        m_serialRxBuffer.remove(0, m_serialRxBuffer.size() - 2048);
}


// 通用的淡入淡出动画函数
void MainWindow::applyFadeAnimation(QTabWidget* tabWidget, int fromIndex, int toIndex) {
    // 如果已经在目标页面或动画正在进行，则直接返回
    if (fromIndex == toIndex || m_fadeAnimationActive) {
        return;
    }

    m_fadeAnimationActive = true;

    // 当前页淡出
    QGraphicsOpacityEffect* fadeOut = new QGraphicsOpacityEffect(tabWidget->widget(fromIndex));
    tabWidget->widget(fromIndex)->setGraphicsEffect(fadeOut);
    QPropertyAnimation* animOut = new QPropertyAnimation(fadeOut, "opacity");
    animOut->setDuration(200);
    animOut->setStartValue(1.0);
    animOut->setEndValue(0.0);

    // 切换后新页淡入
    connect(animOut, &QPropertyAnimation::finished, [this, tabWidget, toIndex, fadeOut]() {
        tabWidget->setCurrentIndex(toIndex);

        QGraphicsOpacityEffect* fadeIn = new QGraphicsOpacityEffect(tabWidget->widget(toIndex));
        tabWidget->widget(toIndex)->setGraphicsEffect(fadeIn);
        QPropertyAnimation* animIn = new QPropertyAnimation(fadeIn, "opacity");
        animIn->setDuration(200);
        animIn->setStartValue(0.0);
        animIn->setEndValue(1.0);

        connect(animIn, &QPropertyAnimation::finished, [this, fadeOut, fadeIn]() {
            // 清理效果对象
            if (fadeOut) fadeOut->deleteLater();
            if (fadeIn) fadeIn->deleteLater();
            m_fadeAnimationActive = false;
        });

        animIn->start(QAbstractAnimation::DeleteWhenStopped);
    });

    animOut->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::sendHeartbeat()
{
    if (!serial->isOpen()) return;
    const quint8 hb[] = { 0x55, 0x55, 0xB6, 0xB6, 0x7B, 0x7B, 0xAA, 0xAA };
    serial->write(reinterpret_cast<const char*>(hb), sizeof(hb));
}

void MainWindow::onSwitchEthercatButtonClicked()

{
    if (!serial->isOpen()) {
        ui->textEdit->append("错误：请先连接串口！");
        return;
    }
    sendData(160, 1);
    ui->textEdit->append("已切换为 EtherCAT 控制模式 (PN160 ← 1)");
}
