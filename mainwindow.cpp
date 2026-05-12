#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QDebug>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , serial(new QSerialPort(this))
    , tcpSocket(new QTcpSocket(this)) //报文
    , m_readTimer(new QTimer(this))  //报文
    , m_heartbeatTimer(new QTimer(this))

{
    ui->setupUi(this);
    centralWidget()->setFixedSize(1280, 720);
    setWindowIcon(QIcon("D:/Saiwider/Code/Qt/EHAcontrol/image/logo.ico"));
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
    ui->EHAmassconfirm->setEnabled(true);
    // ui->EHAtest_2->setEnabled(false);
    // ui->EHAtest->setEnabled(false);

    // 启动力传感器连接线程

    initActions();

    connect(tcpSocket, &QTcpSocket::readyRead, this, &MainWindow::onSocketReadyRead);
    connect(m_readTimer, &QTimer::timeout, this, &MainWindow::readModbusData);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &MainWindow::sendHeartbeat);
    connect(ui->tabWidget,&QTabWidget::currentChanged,this,&MainWindow::ontabwidgetindexchanged);
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
    if (!serial->isOpen()) {
        ui->textEdit->append("错误：请先连接串口！");
        return;
    }

    ui->textEdit->append("开始EHA检测流程...");

    // 清空之前的结果
    m_testResults.clear();

    // 定义测试力值序列
    //浮动模块测试
    const QVector<quint16> testForces = {20, 40, 60, 80, 100, 120, 140, 160, 180, 200};
    const QVector<quint16> testForcesReverse = {200, 180, 160, 140, 120, 100, 80, 60, 40, 20};
    //滚压设备测试
    // const QVector<quint16> testForces = {200, 400, 600, 800, 1000, 1200, 1400, 1600, 1800, 2000};
    // const QVector<quint16> testForcesReverse = {2000, 1800, 1600, 1400, 1200, 1000, 800, 600, 400, 200};
    // 存储测量结果(设定力, 实际力)
    QVector<QPair<quint16, quint16>> measurements;

    // 正向测试(从小到大)
    ui->textEdit->append("\n正向测试:");
    for (quint16 force : testForces) {
        settleForce(force);
        if(force==200){
            delayMS(4000);
        }else{
            delayMS(2000);
        }

        if (!sensorforceValue) {
            ui->textEdit->append("错误：无法读取到传感器数值!");
            continue;
        }

        quint16 actualForce = sensorforceValue/10.0;
        measurements.append({force, actualForce});
        ui->textEdit->append(QString("设定力: %1, 实际力: %2").arg(force).arg(actualForce));
    }

    // 反向测试(从大到小)
    ui->textEdit->append("\n反向测试:");
    for (quint16 force : testForcesReverse) {
        settleForce(force);
        delayMS(2000);

        if (!sensorforceValue) {
            ui->textEdit->append("错误：无法读取到传感器数值!");
            continue;
        }

        quint16 actualForce = sensorforceValue/10.0;
        measurements.append({force, actualForce});
        ui->textEdit->append(QString("设定力: %1, 实际力: %2").arg(force).arg(actualForce));
    }

    // 计算平均值和偏差
    ui->textEdit->append("\n计算结果:");
    qreal maxDeviation = 0.0; // 用于存储最大偏差值
    for (quint16 force : testForces) {
        QVector<quint16> actualForces;
        for (const auto &measurement : measurements) {
            if (measurement.first == force) {
                actualForces.append(measurement.second);
            }
        }

        if (!actualForces.isEmpty()) {
            quint16 sum = std::accumulate(actualForces.begin(), actualForces.end(), 0);
            quint16 average = sum / actualForces.size();
            qreal deviation = average - force;
            m_testResults.append({force, deviation});

            // 更新最大偏差值
            if (qAbs(deviation) > qAbs(maxDeviation)) {
                maxDeviation = deviation;
            }

            // ui->textEdit->append(QString("设定力: %1, 平均实际力: %2, 偏差: %3")
            //                          .arg(force).arg(average).arg(deviation));
        }
    }

    // 绘制偏差曲线图，并传递最大偏差值
    showDeviationChart(maxDeviation);

    // 测试完成后力值归零
    settleForce(0);
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
    qreal rangeLimit = qAbs(maxDeviation) + 10;
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
    if (qAbs(maxDeviation) < 50) {
        resultMessage = "<font size='5' color='#4CAF50'>✓ 测试通过 (最大偏差: %1)</font>";
        resultLabel->setStyleSheet(
            "background-color: #E8F5E9;"
            "border: 1px solid #C8E6C9;"
            "border-radius: 6px;"
            "padding: 8px;"
            );
    } else {
        resultMessage = "<font size='5' color='#F44336'>✗ 测试未通过 (最大偏差: %1)</font>";
        resultLabel->setStyleSheet(
            "background-color: #FFEBEE;"
            "border: 1px solid #FFCDD2;"
            "border-radius: 6px;"
            "padding: 8px;"
            );
    }
    resultLabel->setText(resultMessage.arg(QString::number(maxDeviation, 'f', 2)));
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

    chartDialog->exec();
}

void MainWindow::onehaconnectButtonClicked()
{
    if (serial->isOpen()) {
        m_heartbeatTimer->stop();
        serial->close();
        ui->EHAconnect->setText("连接");
        ui->textEdit->append("已断开串口连接");
        ui->EHAreset->setEnabled(false);
        ui->EHAcalibration->setEnabled(false);
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
    tcpSocket->connectToHost(ipAddress, 502);  // 端口固定为502

    // tcpSocket->connectToHost("192.168.1.12", 502);
    // tcpSocket->connectToHost("127.0.0.1", 502);
    m_readTimer->start(1000);
    if(tcpSocket->waitForConnected(1000)) {  // 等待1秒连接
        ui->textEdit->append("力传感器连接成功");
    } else {
        ui->textEdit->append("力传感器连接失败: " + tcpSocket->errorString());
    }



    ui->EHAreset->setEnabled(true);   // 测试用 之后删掉
    ui->EHAcalibration->setEnabled(true); //测试用 之后删掉


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
        serial->write(reinterpret_cast<const char*>(handshake), sizeof(handshake));
        serial->flush();
        qDebug() << "[连接] 已发送握手包";

        delayMS(50);                // 等设备就绪
        sendData(2, 79);            // Pn002=66：参数监视=转矩反馈
        m_heartbeatTimer->start(100);
        qDebug() << "[连接] 已发送 Pn002=79，心跳已启动";

        ui->EHAconnect->setText("断开");
        ui->textEdit->append(QString("已连接到 %1，请根据需求选择下列功能").arg(portName));
        ui->textEdit->append("EHA标定，点击《EHA初始化》按钮，初始化需要设备姿态调整为竖直 不与标定工具接触");
        ui->textEdit->append("EHA检测，点击《EHA检测》按钮，检测需要设备姿态调整为竖直，接触标定工具，下压量保持在滚压设备零点附近");
        ui->EHAreset->setEnabled(true);  // 连接成功后启用 reset 按钮
        //EHA连接正常 步骤栏变色 跳到下一步
        applyFadeAnimation(ui->tabWidget, ui->tabWidget->currentIndex(), 1);
        ui->STEP1->setIcon(QIcon(":/new/prefix1/icon/confirm_green.png"));
        ui->STEP1->setStyleSheet("background-color: #43425D;color:#FFFFFF;");

        ui->STEP2->setStyleSheet("background-color: #D3D3D3;color:#000000;");

        // ui->EHAtest->setEnabled(true);
    } else {
        ui->textEdit->append("错误：无法打开串口！");
        // ui->textEdit->append(QString("错误详情: %1").arg(serial->errorString()));
    }
    ui->EHAreset->setEnabled(true);  // 连接成功后启用 reset 按钮

}

void MainWindow::oneharesetButtonClicked()
{
    //pn168=150，pn169=0；pn046=150，pn047=0；pn052=150，pn053=0
    ui->textEdit->append("步骤二:EHA初始化。。。");
    sendData(0,9999);
    sendData(169, 0);
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
    sendData(2,81); //数显屏设置为显示位移量
    ui->textEdit->append("设定力初始化完毕\n标定时设备需接触力传感器，并下压至数显屏数值在0(位移零点)左右");

    // 发送全参数读取命令，自动获取 PN168 原始K值
    ui->textEdit->append("正在读取 PN168...");
    m_serialRxBuffer.clear();
    QByteArray readCmd;
    readCmd.append((char)0x55);
    readCmd.append((char)0x55);
    readCmd.append((char)0xB9);
    readCmd.append((char)0x9B);
    readCmd.append((char)0x9B);
    readCmd.append((char)0xB9);
    readCmd.append((char)0xAA);
    readCmd.append((char)0xAA);
    serial->write(readCmd);
    delayMS(300); // 等待设备返回全量参数（200×8字节，115200波特约需111ms）

    // 解析回包，查找 Pn168 帧格式: 55 55 00 A8 ValH ValL AA AA
    m_pn168K = 0.0;
    for (int i = 0; i <= m_serialRxBuffer.size() - 8; ++i) {
        if ((quint8)m_serialRxBuffer[i]   == 0x55 &&
            (quint8)m_serialRxBuffer[i+1] == 0x55 &&
            (quint8)m_serialRxBuffer[i+2] == 0x00 &&
            (quint8)m_serialRxBuffer[i+3] == 0xA8 &&
            (quint8)m_serialRxBuffer[i+6] == 0xAA &&
            (quint8)m_serialRxBuffer[i+7] == 0xAA) {
            m_pn168K = ((quint8)m_serialRxBuffer[i+4] << 8)
                     |  (quint8)m_serialRxBuffer[i+5];
            ui->textEdit->append(QString("PN168 读取成功，原始K值: %1").arg(m_pn168K));
            break;
        }
    }
    if (m_pn168K < 1.0) {
        ui->textEdit->append(QString("警告：PN168 自动读取失败（收到 %1 字节），标定将无法执行")
                                 .arg(m_serialRxBuffer.size()));
    }

    // 启用标定按钮
    ui->EHAcalibration->setEnabled(true);
}
void MainWindow::onehacalibrationButtonClicked()
{
    if (!serial->isOpen()) {
        ui->textEdit->append("错误：请先连接串口！");
        return;
    }

    // 使用初始化阶段读取到的 PN168 原始K值
    if (m_pn168K < 1.0) {
        ui->textEdit->append("错误：PN168 原始K值未获取，请先执行步骤二（EHA初始化）！");
        return;
    }
    double K_original = m_pn168K;

    settleForce(0);
    ui->textEdit->clear();
    ui->textEdit->append("步骤三:启动线性标定流程...");
    ui->textEdit->append(QString("原始K值(PN168): %1").arg(K_original));

    // 标定力值序列 10N ~ 100N，步进 10N
    const QVector<quint16> forces = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};

    // 正向采集 10N → 100N
    QVector<double> forwardActual(forces.size(), 0.0);
    ui->textEdit->append("\n[正向采集] 10N → 100N:");
    for (int i = 0; i < forces.size(); ++i) {
        settleForce(forces[i]);
        MainWindow::delayMS(2000);
        forwardActual[i] = sensorforceValue / 10.0;
        ui->textEdit->append(QString("  设定 %1N → 实际 %2N")
                                 .arg(forces[i])
                                 .arg(forwardActual[i], 0, 'f', 1));
    }

    // 反向采集 100N → 10N，结果按正序（对应 forces[i]）存储
    QVector<double> reverseActual(forces.size(), 0.0);
    ui->textEdit->append("\n[反向采集] 100N → 10N:");
    for (int i = forces.size() - 1; i >= 0; --i) {
        settleForce(forces[i]);
        MainWindow::delayMS(2000);
        reverseActual[i] = sensorforceValue / 10.0;
        ui->textEdit->append(QString("  设定 %1N → 实际 %2N")
                                 .arg(forces[i])
                                 .arg(reverseActual[i], 0, 'f', 1));
    }

    // 构建拟合点集
    // x = 正反向平均实际力
    // y = 模块内部识别数值 = K_original / 10 * 设定力
    ui->textEdit->append("\n[拟合数据]  设定力 | 平均实际力 | 内部识别值");
    QVector<double> xVec, yVec;
    for (int i = 0; i < forces.size(); ++i) {
        double avgForce    = (forwardActual[i] + reverseActual[i]) / 2.0;
        double internalVal = K_original / 10.0 * forces[i];
        xVec.append(avgForce);
        yVec.append(internalVal);
        ui->textEdit->append(QString("  %1N | %2N | %3")
                                 .arg(forces[i],    4)
                                 .arg(avgForce,     7, 'f', 1)
                                 .arg(internalVal,  9, 'f', 1));
    }

    // 最小二乘线性拟合: y = new_K * x + new_B
    int    n     = xVec.size();
    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;
    for (int i = 0; i < n; ++i) {
        sum_x  += xVec[i];
        sum_y  += yVec[i];
        sum_xy += xVec[i] * yVec[i];
        sum_x2 += xVec[i] * xVec[i];
    }
    double denom = n * sum_x2 - sum_x * sum_x;
    if (qAbs(denom) < 1e-9) {
        ui->textEdit->append("错误：线性拟合失败，数据点无效！");
        settleForce(0);
        return;
    }
    double new_K = (n * sum_xy - sum_x * sum_y) / denom;
    double new_B = (sum_y - new_K * sum_x) / n;

    ui->textEdit->append("\n[拟合结果]");
    ui->textEdit->append(QString("  新K值 = %1  (写入PN168)").arg(new_K, 0, 'f', 4));
    ui->textEdit->append(QString("  新B值 = %1  (写入PN169)").arg(new_B, 0, 'f', 4));

    // 写入设备：K四舍五入取整写PN168，B按有符号16位写PN169
    quint16 K_write = static_cast<quint16>(qRound(new_K));
    quint16 B_write = static_cast<quint16>(static_cast<int16_t>(qRound(new_B)));
    sendData(168, K_write);
    // sendData(169, B_write);
    ui->textEdit->append(QString("  PN168 ← %1").arg(K_write));
    ui->textEdit->append(QString("  PN169 ← %1").arg(static_cast<int16_t>(B_write)));

    settleForce(0);
    sendData(152, 1);
    sendData(2, 79);
    ui->textEdit->append("\n数据录入完毕，设备调节为力控模式");
    ui->textEdit->append("步骤四：操作机器人，滚压设备调整为水平姿态（数显屏数值为0），执行力矩清零");
    ui->EHAzero->setEnabled(true);
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
    ui->EHAmassconfirm->setEnabled(true);
    sendData(2, 79);
    MainWindow::delayMS(2000);
    settleForce(0);

}

void MainWindow::onEHAmassconfirmButtonClicked()
{
    int EHAmass = ui->EHAmass->text().toInt();
    sendData(170,EHAmass);
    ui->textEdit->append("步骤五:质量补偿已录入 观察数显屏数值 显示值为0则标定完毕\n(注:质量补偿可多次录入调试)");

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

    static quint16 transactionId = 0;
    transactionId++;

    QByteArray request;
    QDataStream stream(&request, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    // Modbus TCP请求报文
    stream << transactionId << (quint16)0x0000 << (quint16)0x0006;
    stream << (quint8)0x01 << (quint8)0x03 << (quint16)1542 << (quint16)2;

    tcpSocket->write(request);
}

void MainWindow::onSocketReadyRead()
{
    QByteArray data = tcpSocket->readAll();
    // ui->textEdit->append(QString("[TCP RX] %1").arg(QString(data.toHex(' '))));
    parseModbusResponse(data);
}

void MainWindow::parseModbusResponse(const QByteArray &data)
{
    if (data.length() < 9) return;

    QDataStream stream(data);
    stream.setByteOrder(QDataStream::BigEndian);

    quint16 transactionId, protocolId, length;
    quint8 unitId, functionCode;
    stream >> transactionId >> protocolId >> length >> unitId >> functionCode;

    if (protocolId != 0x0000 || unitId != 0x01 || length != 0x0007) return;

    if (functionCode == 0x03 || functionCode == 0x04) {
        quint8 byteCount;
        stream >> byteCount;

        // 确保有足够的数据
        if (data.length() >= 11) {  // 9字节头部 + 1字节byteCount + 至少2字节数据
            // 获取最后两个字节
            quint8 lastByte = data[data.length()-1];
            quint8 secondLastByte = data[data.length()-2];
            // sensorforceValue = (quint16)((secondLastByte << 8) | lastByte);
            sensorforceValue = (quint16)((secondLastByte << 8) | lastByte);
        }
    }
}

void MainWindow::on_NextStep1_clicked(bool checked) {
    applyFadeAnimation(ui->tabWidget, ui->tabWidget->currentIndex(), 1);
    // ui->STEP1->setIcon(QIcon(":/new/prefix1/icon/confirm_green.png"));
    // ui->STEP1->setStyleSheet("background-color: #43425D;color:#FFFFFF;");
    // ui->STEP2->setStyleSheet("background-color: #D3D3D3;color:#000000;");
}

void MainWindow::on_NextStep2_clicked(bool checked) {
    applyFadeAnimation(ui->tabWidget, ui->tabWidget->currentIndex(), 2);
    // ui->STEP2->setIcon(QIcon(":/new/prefix1/icon/confirm_green.png"));
    // ui->STEP2->setStyleSheet("background-color: #43425D;color:#FFFFFF;");
    // ui->STEP3->setStyleSheet("background-color: #D3D3D3;color:#000000;");
}

void MainWindow::on_ForwardStep1_clicked(bool checked) {
    applyFadeAnimation(ui->tabWidget, ui->tabWidget->currentIndex(), 0);
}

void MainWindow::on_NextStep3_clicked(bool checked) {
    applyFadeAnimation(ui->tabWidget, ui->tabWidget->currentIndex(), 3);
    // ui->STEP3->setIcon(QIcon(":/new/prefix1/icon/confirm_green.png"));
    // ui->STEP3->setStyleSheet("background-color: #43425D;color:#FFFFFF;");
    // ui->STEP4->setStyleSheet("background-color: #D3D3D3;color:#000000;");
}

void MainWindow::on_ForwardStep2_clicked(bool checked) {
    applyFadeAnimation(ui->tabWidget, ui->tabWidget->currentIndex(), 1);
}

void MainWindow::on_NextStep4_clicked(bool checked) {
    applyFadeAnimation(ui->tabWidget, ui->tabWidget->currentIndex(), 4);
    // ui->STEP4->setIcon(QIcon(":/new/prefix1/icon/confirm_green.png"));
    // ui->STEP4->setStyleSheet("background-color: #43425D;color:#FFFFFF;");
    // ui->STEP5->setStyleSheet("background-color: #D3D3D3;color:#000000;");
}

void MainWindow::on_ForwardStep3_clicked(bool checked) {
    applyFadeAnimation(ui->tabWidget, ui->tabWidget->currentIndex(), 2);
}

void MainWindow::on_ForwardStep4_clicked(bool checked) {
    applyFadeAnimation(ui->tabWidget, ui->tabWidget->currentIndex(), 3);
}

void MainWindow::onSerialReadyRead()
{
    qDebug() << "[串口 RX] readyRead 触发";
    QByteArray newBytes = serial->readAll();
    static qint64 lastRxPrint = 0;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - lastRxPrint >= 1000) {
        // ui->textEdit->append(QString("[串口 RX] %1").arg(QString(newBytes.toHex(' '))));
        lastRxPrint = now;
    }
    m_serialRxBuffer.append(newBytes);

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
        qint32  rawS = (qint32)rawU;

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
