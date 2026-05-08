#ifndef FORCESENSORWORKER_H
#define FORCESENSORWORKER_H

#include <QObject>
#include "AGP.h"

class ForceSensorWorker : public QObject
{
    Q_OBJECT
public:
    explicit ForceSensorWorker(AGP* sensor) : m_sensor(sensor) {}

public slots:
    void connectSensor() {
        m_sensor->AGP_connect();
        emit connectionResult(m_sensor->is_connected());
    }

signals:
    void connectionResult(bool connected);

private:
    AGP* m_sensor;
};

#endif // FORCESENSORWORKER_H
