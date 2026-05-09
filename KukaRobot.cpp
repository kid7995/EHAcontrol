#include "KukaRobot.h"
#include <QDebug>

// ═══════════════════════════════════════════════════════════════════════════════
// KukaTcpServer
// ═══════════════════════════════════════════════════════════════════════════════

KukaTcpServer::KukaTcpServer(QObject *parent)
    : QObject(parent)
    , m_receiveServer(new QTcpServer(this))
    , m_sendServer(new QTcpServer(this))
{
    connect(m_receiveServer, &QTcpServer::newConnection,
            this, &KukaTcpServer::onReceiveNewConnection);
    connect(m_sendServer, &QTcpServer::newConnection,
            this, &KukaTcpServer::onSendNewConnection);
}

void KukaTcpServer::startServers(quint16 receivePort, quint16 sendPort)
{
    if (!m_receiveServer->isListening()) {
        if (!m_receiveServer->listen(QHostAddress::Any, receivePort))
            qWarning() << "KUKA receive server failed on port" << receivePort;
        else
            qDebug() << "KUKA receive server listening on port" << receivePort;
    }
    if (!m_sendServer->isListening()) {
        if (!m_sendServer->listen(QHostAddress::Any, sendPort))
            qWarning() << "KUKA send server failed on port" << sendPort;
        else
            qDebug() << "KUKA send server listening on port" << sendPort;
    }
}

void KukaTcpServer::stopServers()
{
    if (m_receiveSocket) {
        m_receiveSocket->disconnectFromHost();
        m_receiveSocket = nullptr;
    }
    if (m_sendSocket) {
        m_sendSocket->disconnectFromHost();
        m_sendSocket = nullptr;
    }
    m_receiveServer->close();
    m_sendServer->close();
}

void KukaTcpServer::onReceiveNewConnection()
{
    QTcpSocket *sock = m_receiveServer->nextPendingConnection();
    if (m_receiveSocket) {
        sock->disconnectFromHost();
        sock->deleteLater();
        return;
    }
    m_receiveSocket = sock;
    connect(m_receiveSocket, &QTcpSocket::readyRead,
            this, &KukaTcpServer::onReceiveSocketReadyRead);
    connect(m_receiveSocket, &QTcpSocket::disconnected,
            this, &KukaTcpServer::onReceiveSocketDisconnected);
    qDebug() << "KUKA receive channel connected from" << sock->peerAddress().toString();
    emit receiveConnected();
}

void KukaTcpServer::onSendNewConnection()
{
    QTcpSocket *sock = m_sendServer->nextPendingConnection();
    if (m_sendSocket) {
        sock->disconnectFromHost();
        sock->deleteLater();
        return;
    }
    m_sendSocket = sock;
    connect(m_sendSocket, &QTcpSocket::readyRead,
            this, &KukaTcpServer::onSendSocketReadyRead);
    connect(m_sendSocket, &QTcpSocket::disconnected,
            this, &KukaTcpServer::onSendSocketDisconnected);
    qDebug() << "KUKA send channel connected from" << sock->peerAddress().toString();
    emit sendConnected();
}

void KukaTcpServer::onReceiveSocketReadyRead()
{
    m_receiveBuffer.append(m_receiveSocket->readAll());
    while (m_receiveBuffer.size() >= 64) {
        emit dataReceived(m_receiveBuffer.left(64));
        m_receiveBuffer.remove(0, 64);
    }
}

void KukaTcpServer::onReceiveSocketDisconnected()
{
    m_receiveSocket->deleteLater();
    m_receiveSocket = nullptr;
    m_receiveBuffer.clear();
    emit receiveDisconnected();
}

void KukaTcpServer::onSendSocketDisconnected()
{
    m_sendSocket->deleteLater();
    m_sendSocket = nullptr;
    emit sendDisconnected();
}

void KukaTcpServer::onSendSocketReadyRead()
{
    // 读取并丢弃 ACK（机器人解析命令后回发原 64 字节）
    if (m_sendSocket)
        m_sendSocket->readAll();
}

void KukaTcpServer::sendCommand(const QByteArray &data)
{
    if (!m_sendSocket || m_sendSocket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "KUKA send socket not connected, command dropped";
        return;
    }
    m_sendSocket->write(data);
}

// ═══════════════════════════════════════════════════════════════════════════════
// KukaRobot
// ═══════════════════════════════════════════════════════════════════════════════

KukaRobot::KukaRobot(QObject *parent)
    : QObject(parent)
    , m_server(new KukaTcpServer(this))
    , m_cartPos(6, 0.0)
    , m_axisPos(6, 0.0)
{
    connect(m_server, &KukaTcpServer::dataReceived,
            this, &KukaRobot::onDataReceived);
    connect(m_server, &KukaTcpServer::sendConnected,
            this, &KukaRobot::onSendConnected);
    connect(m_server, &KukaTcpServer::receiveConnected,
            this, &KukaRobot::onReceiveConnected);
    connect(m_server, &KukaTcpServer::receiveDisconnected,
            this, &KukaRobot::onAnyDisconnected);
    connect(m_server, &KukaTcpServer::sendDisconnected,
            this, &KukaRobot::onAnyDisconnected);
}

void KukaRobot::init(quint16 receivePort, quint16 sendPort)
{
    m_server->startServers(receivePort, sendPort);
}

void KukaRobot::disconnectRobot()
{
    m_server->stopServers();
    m_sendReady    = false;
    m_receiveReady = false;
}

bool KukaRobot::isConnected() const
{
    return m_sendReady && m_receiveReady;
}

void KukaRobot::executePathJob(int pathNum, int ovPro)
{
    // control_type=0 (kConfig) → KRL 调用 PathJob()；ControlValue=-1 → 跳过 PCJob()
    buildAndSendCommand(KUKA_CTRL_CONFIG, pathNum, -1, ovPro);
}

void KukaRobot::onSendConnected()
{
    m_sendReady = true;
    if (m_receiveReady) emit connected();
}

void KukaRobot::onReceiveConnected()
{
    m_receiveReady = true;
    if (m_sendReady) emit connected();
}

void KukaRobot::onAnyDisconnected()
{
    bool wasConnected = isConnected();
    m_sendReady    = false;
    m_receiveReady = false;
    if (wasConnected) emit disconnected();
}

void KukaRobot::onDataReceived(const QByteArray &data)
{
    if (data.size() < 64) return;
    parseStatusPacket(data);
    emit stateUpdated();

    if (m_prevMoving && !m_isMoving)
        emit motionStopped();
    m_prevMoving = m_isMoving;
}

void KukaRobot::parseStatusPacket(const QByteArray &data)
{
    const auto *buf = reinterpret_cast<const unsigned char *>(data.data());

    // 解析笛卡尔位置 [0~23]：6 个 little-endian float
    for (int i = 0; i < 6; ++i) {
        uint32_t u = static_cast<uint32_t>(buf[i*4])
                   | (static_cast<uint32_t>(buf[i*4+1]) << 8)
                   | (static_cast<uint32_t>(buf[i*4+2]) << 16)
                   | (static_cast<uint32_t>(buf[i*4+3]) << 24);
        float f;
        memcpy(&f, &u, sizeof(float));
        m_cartPos[i] = static_cast<double>(f);
    }

    // 解析关节角度 [24~47]
    for (int i = 0; i < 6; ++i) {
        uint32_t u = static_cast<uint32_t>(buf[i*4+24])
                   | (static_cast<uint32_t>(buf[i*4+25]) << 8)
                   | (static_cast<uint32_t>(buf[i*4+26]) << 16)
                   | (static_cast<uint32_t>(buf[i*4+27]) << 24);
        float f;
        memcpy(&f, &u, sizeof(float));
        m_axisPos[i] = static_cast<double>(f);
    }

    // 状态字节 [48~52]
    m_isMoving = (buf[48] != 0);
    m_actBase  = buf[51];
    m_actTool  = buf[52];
}

void KukaRobot::buildAndSendCommand(int controlType, int pathNum,
                                    int controlValue, int ovPro)
{
    unsigned char buf[64] = {0};

    // [0~23]：目标位置全 0（PathJob 不需要位置参数）
    // [24~31]：控制字节
    buf[24] = static_cast<unsigned char>(controlType);
    buf[25] = static_cast<unsigned char>(KUKA_MOVE_PTP);        // moving_type
    buf[26] = static_cast<unsigned char>(KUKA_COORD_CARTESIAN); // coordinate_type
    buf[27] = 5;                                                 // Advance
    buf[28] = static_cast<unsigned char>(ovPro);                 // OvPro %
    buf[29] = 1;                                                 // ToolNum（工具 1）
    buf[30] = static_cast<unsigned char>(pathNum);               // PathNum
    buf[31] = static_cast<unsigned char>(static_cast<int8_t>(controlValue)); // ControlValue

    // [32~35]：VelCP = 0.1 m/s
    float velCP = 0.1f;
    memcpy(buf + 32, &velCP, sizeof(float));

    // [36~39]：PointNum = 0
    int pointNum = 0;
    memcpy(buf + 36, &pointNum, sizeof(int));

    m_server->sendCommand(QByteArray(reinterpret_cast<const char *>(buf), 64));
}
