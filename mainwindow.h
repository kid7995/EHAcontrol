#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QTimer>
#include <QDateTime>
#include <QIcon>
#include <QThread> // 新增
#include <QTcpSocket>
//检测绘图
#include <QChartView>
#include <QLineSeries>
#include <QValueAxis>
#include <QFileDialog>
#include <QPixmap>
#include <QtCharts>
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
    bool Compensation=false;

    //力传感报文
    QTcpSocket *tcpSocket;
    QTimer *m_readTimer;
    QTimer *m_heartbeatTimer;
    quint16 sensorforceValue;

//检测绘图
    QChart *m_chart;
    QChartView *m_chartView;
    QLineSeries *m_series;
    QVector<QPair<quint16, qreal>> m_testResults; // 存储测试结果(设定力,偏差)

    //UI动画
    QPropertyAnimation *m_tabAnimation;
    bool m_fadeAnimationActive = false; // 用于防止动画重叠
    void applyFadeAnimation(QTabWidget* tabWidget, int fromIndex, int toIndex);

    // 串口接收缓冲 & 读取到的 PN168 原始K值
    QByteArray m_serialRxBuffer;
    double m_pn168K = 0.0;
    bool m_captureOnce = false;
    bool m_waitingForPn168 = false;
    qint64 m_pn168StartMs = 0;

private slots:
    //标定相关
    void onehaconnectButtonClicked();
    void oneharesetButtonClicked();
    void onehacalibrationButtonClicked();
    void onserialportrefreshButtonClicked();
    void settleForce(quint16 forceValue);
    void onEHAzeroButtonClicked();
    void onEHAmassconfirmButtonClicked();
    //EHA检测功能
    void onEHAtestButtonClicked();
    //步骤图片
    void showImage(const QString &imagePath);
    void showDeviationChart(qreal maxDeviation);
    //报文监控
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
    void sendHeartbeat();
    void onSwitchEthercatButtonClicked();
};
#endif // MAINWINDOW_H
