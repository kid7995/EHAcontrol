#include "monitorpage.h"

#include <QChart>
#include <QChartView>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineSeries>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QTextStream>
#include <QValueAxis>
#include <QVBoxLayout>

using namespace QtCharts;

namespace {

const QStringList channelNames {
    QStringLiteral("实际位置"),
    QStringLiteral("多圈位置"),
    QStringLiteral("单圈位置"),
    QStringLiteral("速度"),
    QStringLiteral("电流"),
    QStringLiteral("母线电压"),
    QStringLiteral("力/转矩"),
};

const QColor channelColors[] {
    QColor(QStringLiteral("#2563EB")),
    QColor(QStringLiteral("#0F766E")),
    QColor(QStringLiteral("#7C3AED")),
    QColor(QStringLiteral("#EA580C")),
    QColor(QStringLiteral("#0891B2")),
    QColor(QStringLiteral("#4F46E5")),
    QColor(QStringLiteral("#DC2626")),
};

} // namespace

MonitorPage::MonitorPage(QWidget *parent)
    : QWidget(parent)
{
    m_samples.resize(7);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(22, 18, 22, 18);
    root->setSpacing(12);

    auto *headingRow = new QHBoxLayout;
    auto *heading = new QVBoxLayout;
    auto *title = new QLabel(QStringLiteral("实时示波器"), this);
    title->setObjectName(QStringLiteral("pageHeading"));
    auto *subtitle = new QLabel(
        QStringLiteral("多通道观察位置、速度、电流、电压与力反馈"), this);
    subtitle->setObjectName(QStringLiteral("pageSubtitle"));
    heading->addWidget(title);
    heading->addWidget(subtitle);
    headingRow->addLayout(heading);
    headingRow->addStretch();

    m_runButton = new QPushButton(QStringLiteral("暂停"), this);
    m_runButton->setObjectName(QStringLiteral("primaryButton"));
    auto *clearButton = new QPushButton(QStringLiteral("清空"), this);
    clearButton->setObjectName(QStringLiteral("secondaryButton"));
    auto *exportButton = new QPushButton(QStringLiteral("导出数据"), this);
    exportButton->setObjectName(QStringLiteral("ghostButton"));
    headingRow->addWidget(m_runButton);
    headingRow->addWidget(clearButton);
    headingRow->addWidget(exportButton);
    root->addLayout(headingRow);

    auto *channelRow = new QHBoxLayout;
    channelRow->setSpacing(10);
    auto *channelLabel = new QLabel(QStringLiteral("显示通道"), this);
    channelLabel->setObjectName(QStringLiteral("sectionTitle"));
    channelRow->addWidget(channelLabel);
    for (int channel = 0; channel < channelNames.size(); ++channel) {
        auto *check = new QCheckBox(channelNames.at(channel), this);
        check->setChecked(channel == PositionMetric || channel == SpeedMetric
                          || channel == CurrentMetric || channel == BusVoltageMetric
                          || channel == ForceMetric);
        check->setStyleSheet(QStringLiteral("color: %1; font-weight: 600;")
                                 .arg(channelColors[channel].name()));
        m_channelChecks.append(check);
        channelRow->addWidget(check);
        connect(check, &QCheckBox::toggled, this, [this]() {
            for (int i = 0; i < m_series.size(); ++i) {
                m_series.at(i)->setVisible(m_channelChecks.at(i)->isChecked());
            }
            updateAxisRange();
        });
    }
    channelRow->addStretch();
    auto *statsLabel = new QLabel(QStringLiteral("统计通道"), this);
    statsLabel->setObjectName(QStringLiteral("mutedText"));
    m_metricCombo = new QComboBox(this);
    for (int channel = 0; channel < channelNames.size(); ++channel) {
        m_metricCombo->addItem(channelNames.at(channel), channel);
    }
    m_metricCombo->setCurrentIndex(m_metricCombo->findData(SpeedMetric));
    channelRow->addWidget(statsLabel);
    channelRow->addWidget(m_metricCombo);
    root->addLayout(channelRow);

    auto *statsRow = new QHBoxLayout;
    statsRow->setSpacing(10);
    const auto addStat = [this, statsRow](const QString &caption, QLabel **valueLabel) {
        auto *card = new QFrame(this);
        card->setObjectName(QStringLiteral("smallStatCard"));
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(12, 7, 12, 7);
        auto *captionLabel = new QLabel(caption, card);
        captionLabel->setObjectName(QStringLiteral("metricTitle"));
        *valueLabel = new QLabel(QStringLiteral("--"), card);
        (*valueLabel)->setObjectName(QStringLiteral("smallStatValue"));
        layout->addWidget(captionLabel);
        layout->addWidget(*valueLabel);
        statsRow->addWidget(card, 1);
    };
    addStat(QStringLiteral("当前值"), &m_currentValue);
    addStat(QStringLiteral("窗口最小值"), &m_minValue);
    addStat(QStringLiteral("窗口最大值"), &m_maxValue);
    addStat(QStringLiteral("采样点数"), &m_sampleCount);
    root->addLayout(statsRow);

    m_chart = new QChart;
    m_chart->setAnimationOptions(QChart::NoAnimation);
    m_chart->setBackgroundRoundness(0);
    m_chart->setBackgroundBrush(QColor(QStringLiteral("#FFFFFF")));
    m_chart->setPlotAreaBackgroundVisible(true);
    m_chart->setPlotAreaBackgroundBrush(QColor(QStringLiteral("#F8FAFC")));
    m_chart->legend()->setVisible(true);
    m_chart->legend()->setAlignment(Qt::AlignBottom);

    for (int channel = 0; channel < channelNames.size(); ++channel) {
        auto *series = new QLineSeries(m_chart);
        series->setName(channelNames.at(channel));
        QPen pen(channelColors[channel]);
        pen.setWidthF(channel == m_metric ? 2.4 : 1.5);
        series->setPen(pen);
        series->setVisible(m_channelChecks.at(channel)->isChecked());
        m_series.append(series);
        m_chart->addSeries(series);
    }

    m_axisX = new QValueAxis;
    m_axisX->setRange(0, 30);
    m_axisX->setLabelFormat(QStringLiteral("%.1f"));
    m_axisX->setTitleText(QStringLiteral("时间 (s)"));
    m_axisY = new QValueAxis;
    m_axisY->setRange(-100, 100);
    m_axisY->setLabelFormat(QStringLiteral("%.1f"));
    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_chart->addAxis(m_axisY, Qt::AlignLeft);
    for (QLineSeries *series : m_series) {
        series->attachAxis(m_axisX);
        series->attachAxis(m_axisY);
    }

    m_chartView = new QChartView(m_chart, this);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setObjectName(QStringLiteral("chartView"));
    root->addWidget(m_chartView, 1);

    connect(m_runButton, &QPushButton::clicked, this, &MonitorPage::toggleRunning);
    connect(clearButton, &QPushButton::clicked, this, &MonitorPage::clearSeries);
    connect(exportButton, &QPushButton::clicked, this, &MonitorPage::exportSamples);
    connect(m_metricCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MonitorPage::metricChanged);

    setConnected(false);
}

void MonitorPage::updateTelemetry(const DeviceSerialService::Telemetry &telemetry)
{
    m_latest = telemetry;
    m_hasLatest = true;
    if (m_running) {
        appendSample();
    }
}

void MonitorPage::setConnected(bool connected)
{
    m_runButton->setEnabled(connected);
    if (!connected) {
        m_running = false;
        m_runButton->setText(QStringLiteral("开始"));
    }
}

void MonitorPage::appendSample()
{
    if (!m_hasLatest) {
        return;
    }
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_firstTimestampMs <= 0) {
        m_firstTimestampMs = nowMs;
    }
    m_elapsedSeconds = (nowMs - m_firstTimestampMs) / 1000.0;

    for (int channel = 0; channel < m_samples.size(); ++channel) {
        if (!m_channelChecks.at(channel)->isChecked()) {
            continue;
        }
        QVector<QPointF> &samples = m_samples[channel];
        while (!samples.isEmpty() && m_elapsedSeconds - samples.first().x() > 30.0) {
            samples.removeFirst();
        }
        samples.append(QPointF(m_elapsedSeconds, metricValue(m_latest, channel)));
        m_series.at(channel)->replace(samples);
    }

    m_axisX->setRange(qMax(0.0, m_elapsedSeconds - 30.0),
                      qMax(30.0, m_elapsedSeconds));
    updateAxisRange();
    updateStatistics();
}

void MonitorPage::toggleRunning()
{
    m_running = !m_running;
    m_runButton->setText(m_running ? QStringLiteral("暂停") : QStringLiteral("开始"));
}

void MonitorPage::clearSeries()
{
    for (QVector<QPointF> &samples : m_samples) {
        samples.clear();
    }
    for (QLineSeries *series : m_series) {
        series->clear();
    }
    m_elapsedSeconds = 0.0;
    m_firstTimestampMs = 0;
    m_axisX->setRange(0, 30);
    m_axisY->setRange(-100, 100);
    m_currentValue->setText(QStringLiteral("--"));
    m_minValue->setText(QStringLiteral("--"));
    m_maxValue->setText(QStringLiteral("--"));
    m_sampleCount->setText(QStringLiteral("0"));
}

void MonitorPage::exportSamples()
{
    bool hasSamples = false;
    for (const QVector<QPointF> &samples : m_samples) {
        hasSamples = hasSamples || !samples.isEmpty();
    }
    if (!hasSamples) {
        QMessageBox::information(this, QStringLiteral("导出数据"),
                                 QStringLiteral("当前没有可导出的采样点"));
        return;
    }
    const QString fileName = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出监控数据"),
        QStringLiteral("KaiserStudio_Scope.csv"),
        QStringLiteral("CSV 文件 (*.csv)"));
    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), file.errorString());
        return;
    }
    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream << "TimeSeconds,Channel,Value\n";
    for (int channel = 0; channel < m_samples.size(); ++channel) {
        for (const QPointF &point : m_samples.at(channel)) {
            stream << QString::number(point.x(), 'f', 3) << ','
                   << channelNames.at(channel) << ','
                   << QString::number(point.y(), 'f', 3) << '\n';
        }
    }
    emit statusMessage(QStringLiteral("监控数据已导出：%1").arg(fileName));
}

void MonitorPage::metricChanged()
{
    m_metric = static_cast<Metric>(m_metricCombo->currentData().toInt());
    for (int channel = 0; channel < m_series.size(); ++channel) {
        QPen pen(channelColors[channel]);
        pen.setWidthF(channel == m_metric ? 2.4 : 1.5);
        m_series.at(channel)->setPen(pen);
    }
    updateStatistics();
}

double MonitorPage::metricValue(const DeviceSerialService::Telemetry &telemetry,
                                int channel) const
{
    switch (static_cast<Metric>(channel)) {
    case PositionMetric:
        return telemetry.position;
    case MultiTurnMetric:
        return telemetry.multiTurnPosition;
    case SingleTurnMetric:
        return telemetry.singleTurnPosition;
    case SpeedMetric:
        return telemetry.speed;
    case CurrentMetric:
        return telemetry.current;
    case BusVoltageMetric:
        return telemetry.busVoltage;
    case ForceMetric:
        return telemetry.force;
    }
    return 0.0;
}

QString MonitorPage::metricName() const
{
    return m_metricCombo ? m_metricCombo->currentText() : QStringLiteral("信号");
}

void MonitorPage::updateStatistics()
{
    const QVector<QPointF> &samples = m_samples.at(m_metric);
    if (samples.isEmpty()) {
        m_currentValue->setText(QStringLiteral("--"));
        m_minValue->setText(QStringLiteral("--"));
        m_maxValue->setText(QStringLiteral("--"));
        m_sampleCount->setText(QStringLiteral("0"));
        return;
    }
    double minValue = samples.first().y();
    double maxValue = samples.first().y();
    for (const QPointF &point : samples) {
        minValue = qMin(minValue, point.y());
        maxValue = qMax(maxValue, point.y());
    }
    m_currentValue->setText(QString::number(samples.last().y(), 'f', 2));
    m_minValue->setText(QString::number(minValue, 'f', 2));
    m_maxValue->setText(QString::number(maxValue, 'f', 2));
    m_sampleCount->setText(QString::number(samples.size()));
}

void MonitorPage::updateAxisRange()
{
    double minValue = 0.0;
    double maxValue = 0.0;
    bool hasValue = false;
    for (int channel = 0; channel < m_samples.size(); ++channel) {
        if (!m_channelChecks.at(channel)->isChecked()) {
            continue;
        }
        for (const QPointF &point : m_samples.at(channel)) {
            if (!hasValue) {
                minValue = maxValue = point.y();
                hasValue = true;
            } else {
                minValue = qMin(minValue, point.y());
                maxValue = qMax(maxValue, point.y());
            }
        }
    }
    if (!hasValue) {
        m_axisY->setRange(-100, 100);
        return;
    }
    if (qFuzzyCompare(minValue, maxValue)) {
        minValue -= 1.0;
        maxValue += 1.0;
    }
    const double margin = (maxValue - minValue) * 0.1;
    m_axisY->setRange(minValue - margin, maxValue + margin);
}
