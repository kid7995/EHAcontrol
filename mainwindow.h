#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QTimer>
#include <QIcon>
#include <QThread>
#include <QTcpSocket>
#include <QElapsedTimer>
//检测绘图
#include <QChartView>
#include <QLineSeries>
#include <QValueAxis>
#include <QFileDialog>
#include <QPixmap>
#include <QtCharts>
// KUKA 机器人通讯
#include "KukaRobot.h"

QT_CHARTS_USE_NAMESPACE

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    QSerialPort *serial;

    void initActions();
    void initSerialPort();
    void sendData(quint16 pn, quint16 value);
    static void delayMS(int ms);
    bool Compensation = false;

    // 力传感报文（Modbus/TCP）
    QTcpSocket *tcpSocket;
    QTimer     *m_readTimer;
    quint16     sensorforceValue = 0;

    // 检测绘图
    QChart    *m_chart;
    QChartView *m_chartView;
    QLineSeries *m_series;
    QVector<QPair<quint16, qreal>> m_testResults;

    // UI 动画
    QPropertyAnimation *m_tabAnimation;
    bool m_fadeAnimationActive = false;
    void applyFadeAnimation(QTabWidget *tabWidget, int fromIndex, int toIndex);

    // 串口接收缓冲 & PN168 原始K值
    QByteArray m_serialRxBuffer;
    double     m_pn168K      = 0.0;
    int16_t    m_serialForceRaw = 0;   // 串口实时力值（N，有符号）

    // ── KUKA 机器人 ──────────────────────────────────────────────────────────
    KukaRobot *m_kukaRobot = nullptr;
    int        m_autoPN170 = 0;        // 自动标定时累计写入的 PN170 值

    // 等待机器人运动完成（先等启动再等停止）
    void waitForRobotDone(int maxWaitMs = 60000);

    // 全流程自动标定主逻辑（阻塞，依赖 delayMS 保持事件循环）
    void runAutoCalibration();

private slots:
    // 标定相关
    void onehaconnectButtonClicked();
    void oneharesetButtonClicked();
    void onehacalibrationButtonClicked();
    void onserialportrefreshButtonClicked();
    void settleForce(quint16 forceValue);
    void onEHAzeroButtonClicked();
    void onEHAmassconfirmButtonClicked();
    // EHA 检测功能
    void onEHAtestButtonClicked();
    // 步骤图片
    void showImage(const QString &imagePath);
    void showDeviationChart(qreal maxDeviation);
    // 报文监控
    void onSocketReadyRead();
    void readModbusData();
    void parseModbusResponse(const QByteArray &data);
    void on_NextStep1_clicked(bool checked);
    void on_NextStep2_clicked(bool checked);
    void on_ForwardStep1_clicked(bool checked);
    void on_NextStep3_clicked(bool checked);
    void on_ForwardStep2_clicked(bool checked);
    void on_NextStep4_clicked(bool checked);
    void on_ForwardStep3_clicked(bool checked);
    void on_ForwardStep4_clicked(bool checked);
    void ontabwidgetindexchanged(int index);
    void onSerialReadyRead();
    // 机器人控制
    void onRobotConnectClicked();
    void onAutoCalibClicked();
    void onRobotConnected();
    void onRobotDisconnected();
};
#endif // MAINWINDOW_H
