#include "deviceserialservice.h"

#include <QSerialPortInfo>
#include <QTimer>

namespace {

quint16 readUInt16(const QByteArray &data, int offset)
{
    return static_cast<quint16>(
        (static_cast<quint32>(static_cast<quint8>(data.at(offset))) << 8)
        | static_cast<quint8>(data.at(offset + 1)));
}

quint32 readUInt32(const QByteArray &data, int offset)
{
    return (static_cast<quint32>(static_cast<quint8>(data.at(offset))) << 24)
        | (static_cast<quint32>(static_cast<quint8>(data.at(offset + 1))) << 16)
        | (static_cast<quint32>(static_cast<quint8>(data.at(offset + 2))) << 8)
        | static_cast<quint32>(static_cast<quint8>(data.at(offset + 3)));
}

} // namespace

QString DeviceSerialService::PortDescription::displayName() const
{
    if (description.isEmpty()) {
        return portName;
    }
    return QStringLiteral("%1 · %2").arg(portName, description);
}

DeviceSerialService::DeviceSerialService(QObject *parent)
    : QObject(parent)
    , m_heartbeatTimer(new QTimer(this))
    , m_parameterDrainTimer(new QTimer(this))
{
    qRegisterMetaType<DeviceSerialService::Telemetry>("DeviceSerialService::Telemetry");

    m_heartbeatTimer->setInterval(100);
    m_parameterDrainTimer->setInterval(180);
    m_parameterDrainTimer->setSingleShot(true);

    connect(&m_serial, &QSerialPort::readyRead,
            this, &DeviceSerialService::handleReadyRead);
    connect(&m_serial, &QSerialPort::errorOccurred,
            this, &DeviceSerialService::handleSerialError);
    connect(m_heartbeatTimer, &QTimer::timeout,
            this, &DeviceSerialService::sendHeartbeat);
    connect(m_parameterDrainTimer, &QTimer::timeout,
            this, &DeviceSerialService::flushParameterQueue);
}

DeviceSerialService::~DeviceSerialService()
{
    close();
}

QList<DeviceSerialService::PortDescription> DeviceSerialService::availablePorts()
{
    QList<PortDescription> result;
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        PortDescription description;
        description.portName = info.portName();
        description.description = info.description();
        description.manufacturer = info.manufacturer();
        description.serialNumber = info.serialNumber();
        result.append(description);
    }
    return result;
}

bool DeviceSerialService::open(const Options &options)
{
    if (isOpen()) {
        close();
    }
    if (options.portName.trimmed().isEmpty()) {
        m_lastError = tr("未指定串口");
        emit errorOccurred(m_lastError);
        return false;
    }

    m_serial.setPortName(options.portName.trimmed());
    m_serial.setBaudRate(options.baudRate);
    m_serial.setDataBits(static_cast<QSerialPort::DataBits>(options.dataBits));
    m_serial.setParity(static_cast<QSerialPort::Parity>(options.parity));
    m_serial.setStopBits(static_cast<QSerialPort::StopBits>(options.stopBits));
    m_serial.setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serial.open(QIODevice::ReadWrite)) {
        m_lastError = m_serial.errorString();
        emit errorOccurred(tr("无法打开串口 %1：%2").arg(options.portName, m_lastError));
        return false;
    }

    m_lastError.clear();
    m_receiveBuffer.clear();
    m_parameterQueue.clear();
    m_readingParameters = false;
    m_readParameterCount = 0;
    m_telemetry = Telemetry();

    emit connectionChanged(true, m_serial.portName());

    const QByteArray handshake = frameFromHex(QStringLiteral("55 55 B9 9B 9B B9 AA AA"));
    writeFrame(handshake);

    QTimer::singleShot(50, this, [this]() {
        if (!isOpen()) {
            return;
        }
        writeFrame(parameterFrame(2, 79));
        m_heartbeatTimer->start();
    });
    return true;
}

void DeviceSerialService::close()
{
    m_heartbeatTimer->stop();
    m_parameterDrainTimer->stop();
    m_parameterQueue.clear();
    if (m_readingParameters) {
        m_readingParameters = false;
        emit parameterReadFinished(false, m_readParameterCount);
    }
    if (m_serial.isOpen()) {
        const QString previousPort = m_serial.portName();
        m_serial.close();
        emit connectionChanged(false, previousPort);
    }
    m_receiveBuffer.clear();
}

bool DeviceSerialService::isOpen() const
{
    return m_serial.isOpen();
}

QString DeviceSerialService::portName() const
{
    return m_serial.portName();
}

QString DeviceSerialService::errorString() const
{
    return m_lastError;
}

DeviceSerialService::Telemetry DeviceSerialService::telemetry() const
{
    return m_telemetry;
}

bool DeviceSerialService::writeParameter(quint16 parameter, quint16 value)
{
    if (!isOpen()) {
        emit errorOccurred(tr("串口未连接，无法写入 Pn%1").arg(parameter, 3, 10, QLatin1Char('0')));
        return false;
    }
    m_parameterQueue.enqueue(parameterFrame(parameter, value));
    if (!m_parameterDrainTimer->isActive()) {
        flushParameterQueue();
    }
    return true;
}

bool DeviceSerialService::readAllParameters()
{
    if (!isOpen()) {
        emit errorOccurred(tr("串口未连接，无法读取参数"));
        return false;
    }
    if (m_readingParameters) {
        return false;
    }

    m_heartbeatTimer->stop();
    m_parameterQueue.clear();
    m_readingParameters = true;
    m_readParameterCount = 0;
    m_receiveBuffer.clear();
    emit parameterReadProgress(0, 200);

    const bool sent = writeFrame(frameFromHex(QStringLiteral("55 55 FE FE EF EF AA AA")));
    if (!sent) {
        finishParameterRead(false);
        return false;
    }

    QTimer::singleShot(5000, this, [this]() {
        if (m_readingParameters) {
            finishParameterRead(false);
        }
    });
    return true;
}

bool DeviceSerialService::sendFrame(const QByteArray &frame)
{
    if (!isOpen()) {
        emit errorOccurred(tr("串口未连接"));
        return false;
    }
    return writeFrame(frame);
}

bool DeviceSerialService::requestMotorEnable()
{
    return sendFrame(frameFromHex(QStringLiteral("55 55 D7 D7 7D 7D AA AA")));
}

bool DeviceSerialService::requestMotorDisable()
{
    return sendFrame(frameFromHex(QStringLiteral("55 55 D8 D8 8D 8D AA AA")));
}

bool DeviceSerialService::requestFaultReset()
{
    return sendFrame(frameFromHex(QStringLiteral("55 55 00 65 00 01 AA AA")));
}

bool DeviceSerialService::requestParameterSave()
{
    return writeParameter(152, 1);
}

void DeviceSerialService::handleReadyRead()
{
    const QByteArray bytes = m_serial.readAll();
    if (bytes.isEmpty()) {
        return;
    }
    m_receiveBuffer.append(bytes);
    emit frameLogged(false, bytes);
    parseBuffer();
}

void DeviceSerialService::handleSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) {
        return;
    }
    m_lastError = m_serial.errorString();
    emit errorOccurred(m_lastError);
    if (error == QSerialPort::ResourceError || error == QSerialPort::DeviceNotFoundError) {
        close();
    }
}

void DeviceSerialService::sendHeartbeat()
{
    if (isOpen() && !m_readingParameters) {
        writeFrame(frameFromHex(QStringLiteral("55 55 B6 B6 7B 7B AA AA")));
    }
}

void DeviceSerialService::flushParameterQueue()
{
    if (m_parameterQueue.isEmpty() || !isOpen()) {
        return;
    }
    const QByteArray frame = m_parameterQueue.dequeue();
    writeFrame(frame);
    if (!m_parameterQueue.isEmpty()) {
        m_parameterDrainTimer->start();
    }
}

void DeviceSerialService::finishParameterRead(bool complete)
{
    if (!m_readingParameters) {
        return;
    }
    m_readingParameters = false;
    emit parameterReadFinished(complete, m_readParameterCount);
    if (isOpen()) {
        m_heartbeatTimer->start();
    }
}

bool DeviceSerialService::writeFrame(const QByteArray &frame)
{
    if (!m_serial.isOpen()) {
        return false;
    }
    const qint64 written = m_serial.write(frame);
    if (written != frame.size()) {
        m_lastError = m_serial.errorString();
        emit errorOccurred(tr("串口发送不完整：%1").arg(m_lastError));
        return false;
    }
    m_serial.flush();
    emit frameLogged(true, frame);
    return true;
}

void DeviceSerialService::parseBuffer()
{
    while (m_receiveBuffer.size() >= 7) {
        int start = -1;
        for (int i = 0; i <= m_receiveBuffer.size() - 2; ++i) {
            if (static_cast<quint8>(m_receiveBuffer.at(i)) == 0xEF
                && static_cast<quint8>(m_receiveBuffer.at(i + 1)) == 0xAD) {
                start = i;
                break;
            }
        }

        if (start < 0) {
            m_receiveBuffer.remove(0, qMax(0, m_receiveBuffer.size() - 1));
            break;
        }
        if (start > 0) {
            m_receiveBuffer.remove(0, start);
        }
        if (m_receiveBuffer.size() < 7) {
            break;
        }

        const quint8 frameType = static_cast<quint8>(m_receiveBuffer.at(2));
        if (frameType == 0xAA) {
            const quint16 parameter = readUInt16(m_receiveBuffer, 3);
            const quint16 value = readUInt16(m_receiveBuffer, 5);
            m_receiveBuffer.remove(0, 7);

            emit parameterValueReceived(parameter, value);
            if (m_readingParameters) {
                ++m_readParameterCount;
                emit parameterReadProgress(m_readParameterCount, 200);
                if (parameter == 199 || m_readParameterCount >= 200) {
                    finishParameterRead(true);
                }
            }
            continue;
        }

        if (frameType == 0xAB) {
            if (m_receiveBuffer.size() < 8) {
                break;
            }
            const quint8 dataType = static_cast<quint8>(m_receiveBuffer.at(3));
            const quint32 rawValue = readUInt32(m_receiveBuffer, 4);
            m_receiveBuffer.remove(0, 8);
            applyTelemetryByte(dataType, rawValue);
            continue;
        }

        m_receiveBuffer.remove(0, 1);
    }

    if (m_receiveBuffer.size() > 8192) {
        m_receiveBuffer.remove(0, m_receiveBuffer.size() - 4096);
    }
}

void DeviceSerialService::applyTelemetryByte(quint8 type, quint32 rawValue)
{
    const qint32 signedValue = static_cast<qint32>(rawValue);
    switch (type) {
    case 0x07:
        m_telemetry.force = static_cast<double>(rawValue);
        break;
    case 0x08:
        m_telemetry.current = static_cast<double>(signedValue) * 0.01;
        break;
    case 0x09:
        m_telemetry.speed = static_cast<double>(signedValue);
        break;
    case 0x0A:
        m_telemetry.multiTurnPosition = static_cast<double>(rawValue);
        break;
    case 0x0B:
        m_telemetry.singleTurnPosition = static_cast<double>(rawValue);
        break;
    case 0x0C:
        m_telemetry.position = static_cast<double>(signedValue);
        break;
    case 0x1A:
        m_telemetry.busVoltage = static_cast<double>(rawValue) * 0.01;
        break;
    case 0x05:
        m_telemetry.statusWord = rawValue;
        m_telemetry.motorEnabled = (rawValue & 0x01U) != 0;
        m_telemetry.statusKnown = true;
        break;
    default:
        return;
    }

    m_telemetry.valid = true;
    m_telemetry.timestamp = QDateTime::currentDateTime();
    emit telemetryUpdated(m_telemetry);
}

QByteArray DeviceSerialService::parameterFrame(quint16 parameter, quint16 value)
{
    QByteArray frame;
    frame.reserve(8);
    frame.append(static_cast<char>(0x55));
    frame.append(static_cast<char>(0x55));
    frame.append(static_cast<char>((parameter >> 8) & 0xFF));
    frame.append(static_cast<char>(parameter & 0xFF));
    frame.append(static_cast<char>((value >> 8) & 0xFF));
    frame.append(static_cast<char>(value & 0xFF));
    frame.append(static_cast<char>(0xAA));
    frame.append(static_cast<char>(0xAA));
    return frame;
}

QByteArray DeviceSerialService::frameFromHex(const QString &hex)
{
    return QByteArray::fromHex(hex.toLatin1());
}
