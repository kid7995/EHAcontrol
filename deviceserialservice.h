#ifndef DEVICESERIALSERVICE_H
#define DEVICESERIALSERVICE_H

#include <QDateTime>
#include <QObject>
#include <QQueue>
#include <QSerialPort>

class QTimer;

class DeviceSerialService : public QObject
{
    Q_OBJECT

public:
    struct Options {
        QString portName;
        qint32 baudRate = 115200;
        int dataBits = 8;
        int parity = 0;
        int stopBits = 1;
    };

    struct PortDescription {
        QString portName;
        QString description;
        QString manufacturer;
        QString serialNumber;

        QString displayName() const;
    };

    struct Telemetry {
        double force = 0.0;
        double current = 0.0;
        double speed = 0.0;
        double position = 0.0;
        double multiTurnPosition = 0.0;
        double singleTurnPosition = 0.0;
        double busVoltage = 0.0;
        quint32 statusWord = 0;
        quint32 faultCode = 0;
        bool motorEnabled = false;
        bool statusKnown = false;
        bool valid = false;
        QDateTime timestamp;
    };

    explicit DeviceSerialService(QObject *parent = nullptr);
    ~DeviceSerialService() override;

    static QList<PortDescription> availablePorts();
    bool open(const Options &options);
    void close();
    bool isOpen() const;
    QString portName() const;
    QString errorString() const;
    Telemetry telemetry() const;

    bool writeParameter(quint16 parameter, quint16 value);
    bool readAllParameters();
    bool sendFrame(const QByteArray &frame);
    bool requestMotorEnable();
    bool requestMotorDisable();
    bool requestFaultReset();
    bool requestParameterSave();

signals:
    void connectionChanged(bool connected, const QString &portName);
    void telemetryUpdated(const DeviceSerialService::Telemetry &telemetry);
    void parameterValueReceived(quint16 parameter, quint16 value);
    void parameterReadProgress(int received, int total);
    void parameterReadFinished(bool complete, int received);
    void frameLogged(bool transmitted, const QByteArray &frame);
    void errorOccurred(const QString &message);

private slots:
    void handleReadyRead();
    void handleSerialError(QSerialPort::SerialPortError error);
    void sendHeartbeat();
    void flushParameterQueue();
    void finishParameterRead(bool complete);

private:
    bool writeFrame(const QByteArray &frame);
    void parseBuffer();
    void applyTelemetryByte(quint8 type, quint32 rawValue);
    static QByteArray parameterFrame(quint16 parameter, quint16 value);
    static QByteArray frameFromHex(const QString &hex);

    QSerialPort m_serial;
    QTimer *m_heartbeatTimer = nullptr;
    QTimer *m_parameterDrainTimer = nullptr;
    QByteArray m_receiveBuffer;
    QQueue<QByteArray> m_parameterQueue;
    Telemetry m_telemetry;
    QString m_lastError;
    bool m_readingParameters = false;
    int m_readParameterCount = 0;
};

Q_DECLARE_METATYPE(DeviceSerialService::Telemetry)

#endif // DEVICESERIALSERVICE_H
