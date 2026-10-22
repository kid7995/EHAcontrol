#ifndef MONITORPAGE_H
#define MONITORPAGE_H

#include "deviceserialservice.h"

#include <QPointF>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <QtCharts/QChartGlobal>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QLabel;
class QPushButton;
class QComboBox;
QT_END_NAMESPACE

QT_CHARTS_BEGIN_NAMESPACE
class QChart;
class QChartView;
class QLineSeries;
class QValueAxis;
QT_CHARTS_END_NAMESPACE

class MonitorPage : public QWidget
{
    Q_OBJECT

public:
    explicit MonitorPage(QWidget *parent = nullptr);

public slots:
    void updateTelemetry(const DeviceSerialService::Telemetry &telemetry);
    void setConnected(bool connected);

signals:
    void statusMessage(const QString &message);

private slots:
    void appendSample();
    void toggleRunning();
    void clearSeries();
    void exportSamples();
    void metricChanged();

private:
    enum Metric {
        PositionMetric = 0,
        MultiTurnMetric,
        SingleTurnMetric,
        SpeedMetric,
        CurrentMetric,
        BusVoltageMetric,
        ForceMetric
    };

    double metricValue(const DeviceSerialService::Telemetry &telemetry, int channel) const;
    QString metricName() const;
    void updateStatistics();
    void updateAxisRange();

    DeviceSerialService::Telemetry m_latest;
    bool m_hasLatest = false;
    bool m_running = true;
    Metric m_metric = SpeedMetric;
    double m_elapsedSeconds = 0.0;
    qint64 m_firstTimestampMs = 0;
    QVector<QVector<QPointF>> m_samples;

    QtCharts::QChart *m_chart = nullptr;
    QtCharts::QChartView *m_chartView = nullptr;
    QVector<QtCharts::QLineSeries *> m_series;
    QtCharts::QValueAxis *m_axisX = nullptr;
    QtCharts::QValueAxis *m_axisY = nullptr;
    QVector<QCheckBox *> m_channelChecks;
    QComboBox *m_metricCombo = nullptr;
    QPushButton *m_runButton = nullptr;
    QLabel *m_currentValue = nullptr;
    QLabel *m_minValue = nullptr;
    QLabel *m_maxValue = nullptr;
    QLabel *m_sampleCount = nullptr;
};

#endif // MONITORPAGE_H
