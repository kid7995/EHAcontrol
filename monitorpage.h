#ifndef MONITORPAGE_H
#define MONITORPAGE_H

#include "deviceserialservice.h"

#include <QElapsedTimer>
#include <QPointF>
#include <QPoint>
#include <QQueue>
#include <QVector>
#include <QWidget>

#include <QtCharts/QChartGlobal>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;
QT_END_NAMESPACE

QT_CHARTS_BEGIN_NAMESPACE
class QChart;
class QChartView;
class QLineSeries;
class QValueAxis;
QT_CHARTS_END_NAMESPACE

class ParameterModel;

class MonitorPage : public QWidget
{
    Q_OBJECT

public:
    explicit MonitorPage(ParameterModel *parameterModel, QWidget *parent = nullptr);

public slots:
    void updateTelemetry(const DeviceSerialService::Telemetry &telemetry);
    void setConnected(bool connected);

signals:
    void statusMessage(const QString &message);

private slots:
    void appendSample();
    void toggleRunning();
    void clearSeries();
    void metricChanged();
    void processQueuedTelemetry();
    void saveWaveform();
    void loadWaveform();

private:
    enum Metric {
        PositionMetric = 0,
        PositionCommandMetric,
        SpeedMetric,
        SpeedCommandMetric,
        CurrentMetric,
        CurrentCommandMetric,
        PositionErrorMetric,
        ParameterMonitorMetric,
        MetricCount
    };

    double metricValue(const DeviceSerialService::Telemetry &telemetry, int channel) const;
    static bool isCommandChannel(int channel);
    void updateAxisRange();
    void updateParameterSummary();
    void updateLiveSummary(const DeviceSerialService::Telemetry &telemetry);
    void applyTimeWindow();
    void applyYScale(double factor);
    void applyWheelZoom(int wheelDelta, double anchorX);
    void panXWaveform(int deltaPixels);
    void resetWaveformView();
    void applyChannelTransform(int channel);
    void enqueueTelemetry(const DeviceSerialService::Telemetry &telemetry);
    void processTelemetryFrame(const DeviceSerialService::Telemetry &telemetry);
    double transformedValue(int channel, double rawValue) const;
    int channelDisplayDecimals(int channel) const;
    QString formattedChannelValue(int channel, double value) const;
    void showHoverInfo(const QPointF &chartValue, const QPoint &globalPosition);
    void hideHoverIndicator();
    void flushPendingSamples();
    void updateTimeAxisRange();

    ParameterModel *m_parameterModel = nullptr;
    DeviceSerialService::Telemetry m_latest;
    bool m_hasLatest = false;
    bool m_running = true;
    double m_elapsedSeconds = 0.0;
    double m_firstTimestampMs = 0.0;
    double m_timeWindowSeconds = 10.0;
    double m_yScaleFactor = 1.0;
    bool m_showFullWaveform = false;
    bool m_manualXView = false;
    double m_manualXStart = 0.0;
    double m_manualXEnd = 0.0;
    int m_zoomMode = 0;
    QVector<QVector<QPointF>> m_samples;
    QVector<bool> m_channelDirty;
    QQueue<DeviceSerialService::Telemetry> m_telemetryQueue;
    QElapsedTimer m_playbackClock;
    qint64 m_playbackFrameCount = 0;
    bool m_seriesDirty = false;

    QtCharts::QChart *m_chart = nullptr;
    QtCharts::QChartView *m_chartView = nullptr;
    QVector<QtCharts::QLineSeries *> m_series;
    QVector<QVector<int>> m_metricGroups;
    QVector<double> m_channelOffsets;
    QVector<double> m_channelGains;
    QtCharts::QValueAxis *m_axisX = nullptr;
    QtCharts::QValueAxis *m_axisY = nullptr;
    QVector<QCheckBox *> m_channelChecks;
    QVector<QDoubleSpinBox *> m_channelOffsetSpins;
    QVector<QDoubleSpinBox *> m_channelGainSpins;
    QVector<QLineEdit *> m_channelUnitEdits;
    QtCharts::QLineSeries *m_cursorLine = nullptr;
    QTimer *m_renderTimer = nullptr;
    QTimer *m_playbackTimer = nullptr;
    QComboBox *m_metricCombo = nullptr;
    QPushButton *m_runButton = nullptr;
    QDoubleSpinBox *m_timeWindowSpin = nullptr;
    QComboBox *m_yModeCombo = nullptr;
    QDoubleSpinBox *m_yMinSpin = nullptr;
    QDoubleSpinBox *m_yMaxSpin = nullptr;
    QVector<QLabel *> m_motionValues;
    QVector<QLabel *> m_performanceValues;
};

#endif // MONITORPAGE_H
