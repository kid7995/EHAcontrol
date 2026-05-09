#ifndef KUKAROBOT_H
#define KUKAROBOT_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QByteArray>
#include <QVector>
#include <cstring>
#include <cstdint>

// control_type byte[24]
constexpr int KUKA_CTRL_CONFIG = 0;  // triggers PathJob / PCJob
constexpr int KUKA_CTRL_MOVING = 1;  // triggers BINKKRMOVE

// moving_type byte[25]
constexpr int KUKA_MOVE_LIN = 0;
constexpr int KUKA_MOVE_PTP = 1;

// coordinate_type byte[26]
constexpr int KUKA_COORD_CARTESIAN = 0;
constexpr int KUKA_COORD_AXIS      = 1;

// ─── 双端口 TCP 服务器 ─────────────────────────────────────────────────────────
class KukaTcpServer : public QObject
{
    Q_OBJECT
public:
    explicit KukaTcpServer(QObject *parent = nullptr);

    void startServers(quint16 receivePort, quint16 sendPort);
    void stopServers();

    bool isReceiveConnected() const { return m_receiveSocket != nullptr; }
    bool isSendConnected()    const { return m_sendSocket    != nullptr; }

    // 向机器人发送 64 字节命令（通过 sendPort 连接）
    void sendCommand(const QByteArray &data);

signals:
    void receiveConnected();
    void sendConnected();
    void receiveDisconnected();
    void sendDisconnected();
    void dataReceived(QByteArray data);   // 收到机器人状态包（64 字节）

private slots:
    void onReceiveNewConnection();
    void onSendNewConnection();
    void onReceiveSocketReadyRead();
    void onReceiveSocketDisconnected();
    void onSendSocketDisconnected();
    void onSendSocketReadyRead();   // 读取并丢弃 ACK

private:
    QTcpServer *m_receiveServer = nullptr;
    QTcpServer *m_sendServer    = nullptr;
    QTcpSocket *m_receiveSocket = nullptr;
    QTcpSocket *m_sendSocket    = nullptr;
    QByteArray  m_receiveBuffer;
};

// ─── KUKA 机器人控制类 ─────────────────────────────────────────────────────────
class KukaRobot : public QObject
{
    Q_OBJECT
public:
    explicit KukaRobot(QObject *parent = nullptr);

    // pcIP 与 EKI XML 中 <EXTERNAL><IP> 保持一致；调用后立即开始监听
    void init(quint16 receivePort = 56000, quint16 sendPort = 56001);
    void disconnectRobot();

    bool isConnected() const;
    bool isMoving()    const { return m_isMoving; }

    // 触发机器人执行 KRL PathNumX() 子程序
    // pathNum：1-6，ovPro：速度百分比（1-100）
    void executePathJob(int pathNum, int ovPro = 100);

    QVector<double> cartesianPosition() const { return m_cartPos; }
    QVector<double> axisPosition()      const { return m_axisPos; }

signals:
    void connected();
    void disconnected();
    void stateUpdated();    // 每帧状态包解析后发出
    void motionStopped();   // PRO_MOVE 1→0 时发出

private slots:
    void onDataReceived(const QByteArray &data);
    void onSendConnected();
    void onReceiveConnected();
    void onAnyDisconnected();

private:
    void parseStatusPacket(const QByteArray &data);
    void buildAndSendCommand(int controlType, int pathNum,
                             int controlValue = -1, int ovPro = 100);

    KukaTcpServer  *m_server        = nullptr;
    QVector<double> m_cartPos;   // X,Y,Z,A,B,C（毫米/度）
    QVector<double> m_axisPos;   // A1-A6（度）
    bool m_isMoving       = false;
    bool m_prevMoving     = false;
    bool m_sendReady      = false;
    bool m_receiveReady   = false;
    int  m_actBase        = 0;
    int  m_actTool        = 1;
};

#endif // KUKAROBOT_H
