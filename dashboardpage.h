#ifndef DASHBOARDPAGE_H
#define DASHBOARDPAGE_H

#include "deviceserialservice.h"

#include <QWidget>

class QLabel;
class QPushButton;

class DashboardPage : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardPage(QWidget *parent = nullptr);

public slots:
    void setConnectionState(bool connected, const QString &portName);
    void updateTelemetry(const DeviceSerialService::Telemetry &telemetry);
    void setMotorEnabled(bool enabled);
    void setFaultCode(quint32 faultCode);
    void setReadProgress(int received, int total);
    void setParameterReadFinished(bool complete, int received);

signals:
    void readAllRequested();
    void enableRequested();
    void disableRequested();
    void saveRequested();
    void openCommunicationRequested();

private:
    struct MetricWidgets {
        QLabel *value = nullptr;
        QLabel *note = nullptr;
    };

    MetricWidgets addMetric(class QGridLayout *layout, int row, int column,
                            const QString &title, const QString &unit,
                            const QString &accent);
    void setMetric(MetricWidgets &metric, double value, int decimals = 0);
    void setMetric(MetricWidgets &metric, const QString &value);

    MetricWidgets m_position;
    MetricWidgets m_multiTurn;
    MetricWidgets m_singleTurn;
    MetricWidgets m_speed;
    MetricWidgets m_current;
    MetricWidgets m_busVoltage;
    MetricWidgets m_force;
    MetricWidgets m_state;
    QLabel *m_connectionLabel = nullptr;
    QLabel *m_motorLabel = nullptr;
    QLabel *m_faultLabel = nullptr;
    QLabel *m_lastUpdateLabel = nullptr;
    QLabel *m_readStatusLabel = nullptr;
    QPushButton *m_enableButton = nullptr;
    QPushButton *m_disableButton = nullptr;
    QPushButton *m_readButton = nullptr;
};

#endif // DASHBOARDPAGE_H
