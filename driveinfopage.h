#ifndef DRIVEINFOPAGE_H
#define DRIVEINFOPAGE_H

#include "deviceserialservice.h"

#include <QMap>
#include <QWidget>

class QLabel;

class DriveInfoPage : public QWidget
{
    Q_OBJECT

public:
    explicit DriveInfoPage(QWidget *parent = nullptr);

public slots:
    void setConnected(bool connected, const QString &portName);
    void updateTelemetry(const DeviceSerialService::Telemetry &telemetry);
    void setParameterValue(quint16 parameter, quint16 value);
    void setParameterReadCount(int count);

signals:
    void readAllRequested();

private:
    void refreshSummary();

    QMap<quint16, quint16> m_parameters;
    QLabel *m_connection = nullptr;
    QLabel *m_enableState = nullptr;
    QLabel *m_controlMode = nullptr;
    QLabel *m_displayMode = nullptr;
    QLabel *m_kValue = nullptr;
    QLabel *m_bValue = nullptr;
    QLabel *m_massCompensation = nullptr;
    QLabel *m_parameterCount = nullptr;
    QLabel *m_lastPosition = nullptr;
    QLabel *m_lastVoltage = nullptr;
    bool m_connected = false;
};

#endif // DRIVEINFOPAGE_H
