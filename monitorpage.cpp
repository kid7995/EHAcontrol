#include "monitorpage.h"

#include "parametermodel.h"

#include <QChart>
#include <QChartView>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QFont>
#include <QGraphicsView>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLineSeries>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTextStream>
#include <QTimer>
#include <QToolTip>
#include <QValueAxis>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <functional>
#include <algorithm>
#include <cmath>

using namespace QtCharts;

namespace {

const QStringList channelNames {
    QStringLiteral("P · 实际位置"),
    QStringLiteral("PCMD · 位置指令"),
    QStringLiteral("V · 实际速度"),
    QStringLiteral("VCMD · 速度指令"),
    QStringLiteral("I · 实际电流"),
    QStringLiteral("ICMD · 电流指令"),
    QStringLiteral("PE · 位置误差"),
    QStringLiteral("参数监视"),
};

const QColor channelColors[] {
    QColor(QStringLiteral("#2563EB")),
    QColor(QStringLiteral("#60A5FA")),
    QColor(QStringLiteral("#0F766E")),
    QColor(QStringLiteral("#14B8A6")),
    QColor(QStringLiteral("#EA580C")),
    QColor(QStringLiteral("#F59E0B")),
    QColor(QStringLiteral("#7C3AED")),
    QColor(QStringLiteral("#DC2626")),
};

const int channelDisplayOrder[] {
    1,
    0,
    6,
    3,
    2,
    5,
    4,
    7,
};

const QStringList shortChannelNames {
    QStringLiteral("P"),
    QStringLiteral("PCMD"),
    QStringLiteral("V"),
    QStringLiteral("VCMD"),
    QStringLiteral("I"),
    QStringLiteral("ICMD"),
    QStringLiteral("PE"),
    QStringLiteral("Para"),
};

const QStringList channelUnits {
    QStringLiteral("count"),
    QStringLiteral("count"),
    QStringLiteral("rpm"),
    QStringLiteral("rpm"),
    QStringLiteral("A"),
    QStringLiteral("A"),
    QStringLiteral("count"),
    QStringLiteral("UU"),
};

const int channelDecimals[] {
    0,
    0,
    0,
    0,
    2,
    2,
    0,
    0,
};

int channelIndexForKey(const QString &key)
{
    for (int channel = 0; channel < shortChannelNames.size(); ++channel) {
        if (shortChannelNames.at(channel).compare(key, Qt::CaseInsensitive) == 0) {
            return channel;
        }
    }
    return -1;
}

QLabel *sectionLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("sectionTitle"));
    return label;
}

QFrame *separator(QWidget *parent)
{
    auto *line = new QFrame(parent);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Plain);
    line->setStyleSheet(QStringLiteral("color: #E2E8F0;"));
    return line;
}

class HoverChartView : public QChartView
{
public:
    explicit HoverChartView(QChart *chart, QWidget *parent = nullptr)
        : QChartView(chart, parent)
    {
        setMouseTracking(true);
        setCursor(Qt::CrossCursor);
    }

    std::function<void(const QPointF &, const QPoint &)> hovered;
    std::function<void()> hoverLeft;
    std::function<void(int, double)> wheelZoom;
    std::function<bool()> canPan;
    std::function<void(int)> panned;
    std::function<void()> resetView;

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && canPan && canPan()
            && chart() && chart()->plotArea().contains(event->pos())) {
            m_panning = true;
            m_lastPanPosition = event->pos();
            setCursor(Qt::ClosedHandCursor);
            QToolTip::hideText();
            event->accept();
            return;
        }
        QChartView::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_panning) {
            const int delta = event->pos().x() - m_lastPanPosition.x();
            m_lastPanPosition = event->pos();
            if (delta != 0 && panned) {
                panned(delta);
            }
            if (hoverLeft) {
                hoverLeft();
            }
            event->accept();
            return;
        }
        QChartView::mouseMoveEvent(event);
        if (!chart() || !chart()->plotArea().contains(event->pos())) {
            QToolTip::hideText();
            if (hoverLeft) {
                hoverLeft();
            }
            return;
        }
        if (hovered) {
            hovered(chart()->mapToValue(event->pos()), event->globalPos());
        }
    }

    void leaveEvent(QEvent *event) override
    {
        QToolTip::hideText();
        if (hoverLeft) {
            hoverLeft();
        }
        QChartView::leaveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (m_panning && event->button() == Qt::LeftButton) {
            m_panning = false;
            setCursor(canPan && canPan() ? Qt::OpenHandCursor : Qt::CrossCursor);
            event->accept();
            return;
        }
        QChartView::mouseReleaseEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && resetView
            && chart() && chart()->plotArea().contains(event->pos())) {
            resetView();
            event->accept();
            return;
        }
        QChartView::mouseDoubleClickEvent(event);
    }

    void wheelEvent(QWheelEvent *event) override
    {
        const int delta = event->angleDelta().y();
        if (delta != 0 && wheelZoom) {
            wheelZoom(delta, chart()->mapToValue(event->position()).x());
            event->accept();
            return;
        }
        QChartView::wheelEvent(event);
    }

private:
    bool m_panning = false;
    QPoint m_lastPanPosition;
};

} // namespace

MonitorPage::MonitorPage(ParameterModel *parameterModel, QWidget *parent)
    : QWidget(parent)
    , m_parameterModel(parameterModel)
{
    m_samples.resize(MetricCount);
    m_channelChecks.resize(MetricCount);
    m_channelOffsetSpins.resize(MetricCount);
    m_channelGainSpins.resize(MetricCount);
    m_channelUnitEdits.resize(MetricCount);
    m_channelDirty = QVector<bool>(MetricCount, false);
    m_channelOffsets = QVector<double>(MetricCount, 0.0);
    m_channelGains = QVector<double>(MetricCount, 1.0);
    m_metricGroups = {
        {PositionMetric, PositionCommandMetric},
        {SpeedMetric, SpeedCommandMetric},
        {CurrentMetric, CurrentCommandMetric},
        {PositionErrorMetric},
        {ParameterMonitorMetric},
    };

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(10, 8, 10, 8);
    root->setSpacing(8);

    auto *center = new QWidget(this);
    auto *centerLayout = new QVBoxLayout(center);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(6);

    auto *controlBar = new QFrame(center);
    controlBar->setObjectName(QStringLiteral("panel"));
    auto *controlLayout = new QHBoxLayout(controlBar);
    controlLayout->setContentsMargins(8, 4, 8, 4);
    controlLayout->setSpacing(6);
    auto *channelSelectionLabel = new QLabel(QStringLiteral("通道选择"), controlBar);
    channelSelectionLabel->setObjectName(QStringLiteral("sectionTitle"));
    controlLayout->addWidget(channelSelectionLabel);

    m_metricCombo = new QComboBox(controlBar);
    m_metricCombo->setMinimumWidth(118);
    const QStringList metricGroupNames {
        QStringLiteral("P + PCMD"),
        QStringLiteral("V + VCMD"),
        QStringLiteral("I + ICMD"),
        QStringLiteral("PE · 位置误差"),
        QStringLiteral("参数监视"),
    };
    for (int group = 0; group < m_metricGroups.size(); ++group) {
        m_metricCombo->addItem(metricGroupNames.at(group), group);
    }
    m_metricCombo->setCurrentIndex(0);
    controlLayout->addWidget(m_metricCombo);

    m_runButton = new QPushButton(QStringLiteral("开始"), controlBar);
    m_runButton->setObjectName(QStringLiteral("primaryButton"));
    controlLayout->addWidget(m_runButton);

    auto *xZoomButton = new QPushButton(QStringLiteral("X-Zoom"), controlBar);
    xZoomButton->setObjectName(QStringLiteral("modeButton"));
    xZoomButton->setCheckable(true);
    xZoomButton->setMinimumWidth(68);
    xZoomButton->setToolTip(QStringLiteral("选中后在波形图内滚轮缩放 X 轴"));
    auto *yZoomButton = new QPushButton(QStringLiteral("Y-Zoom"), controlBar);
    yZoomButton->setObjectName(QStringLiteral("modeButton"));
    yZoomButton->setCheckable(true);
    yZoomButton->setMinimumWidth(68);
    yZoomButton->setToolTip(QStringLiteral("选中后在波形图内滚轮缩放 Y 轴"));
    controlLayout->addWidget(xZoomButton);
    controlLayout->addWidget(yZoomButton);

    auto *resetScaleButton = new QPushButton(QStringLiteral("复位"), controlBar);
    resetScaleButton->setObjectName(QStringLiteral("secondaryButton"));
    resetScaleButton->setToolTip(QStringLiteral("全局放缩复位"));
    auto *clearButton = new QPushButton(QStringLiteral("清空"), controlBar);
    clearButton->setObjectName(QStringLiteral("ghostButton"));
    clearButton->setToolTip(QStringLiteral("清空所有曲线"));
    controlLayout->addWidget(resetScaleButton);
    controlLayout->addWidget(clearButton);
    controlLayout->addStretch();
    auto *saveWaveformButton = new QPushButton(QStringLiteral("保存波形"), controlBar);
    saveWaveformButton->setObjectName(QStringLiteral("secondaryButton"));
    auto *loadWaveformButton = new QPushButton(QStringLiteral("读取波形"), controlBar);
    loadWaveformButton->setObjectName(QStringLiteral("secondaryButton"));
    controlLayout->addWidget(saveWaveformButton);
    controlLayout->addWidget(loadWaveformButton);
    centerLayout->addWidget(controlBar);

    m_chart = new QChart;
    m_chart->setAnimationOptions(QChart::NoAnimation);
    m_chart->setBackgroundRoundness(0);
    m_chart->setBackgroundBrush(QColor(QStringLiteral("#FFFFFF")));
    m_chart->setPlotAreaBackgroundVisible(true);
    m_chart->setPlotAreaBackgroundBrush(QColor(QStringLiteral("#F8FAFC")));
    m_chart->setMargins(QMargins(2, 2, 2, 2));
    m_chart->legend()->setVisible(false);

    for (int channel = 0; channel < channelNames.size(); ++channel) {
        auto *series = new QLineSeries(m_chart);
        series->setName(channelNames.at(channel));
        QPen pen(channelColors[channel]);
        if (isCommandChannel(channel)) {
            pen.setStyle(Qt::DashLine);
        }
        pen.setWidthF(m_metricGroups.first().contains(channel) ? 2.4 : 1.5);
        series->setPen(pen);
        series->setUseOpenGL(true);
        series->setVisible(channel == PositionMetric || channel == PositionCommandMetric);
        m_series.append(series);
        m_chart->addSeries(series);
    }

    m_axisX = new QValueAxis;
    m_axisX->setRange(0, m_timeWindowSeconds);
    m_axisX->setLabelFormat(QStringLiteral("%.3f"));
    m_axisX->setTitleVisible(false);
    m_axisX->setLabelsFont(QFont(QStringLiteral("Microsoft YaHei UI"), 8));
    m_axisX->setTickCount(7);
    m_axisY = new QValueAxis;
    m_axisY->setRange(-100, 100);
    m_axisY->setLabelFormat(QStringLiteral("%.1f"));
    m_axisY->setTitleVisible(false);
    m_axisY->setLabelsFont(QFont(QStringLiteral("Microsoft YaHei UI"), 8));
    m_axisY->setTickCount(11);
    m_axisY->setGridLineVisible(true);
    m_axisY->setGridLinePen(QPen(QColor(QStringLiteral("#E2E8F0")), 1.0));
    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_chart->addAxis(m_axisY, Qt::AlignLeft);
    for (QLineSeries *series : m_series) {
        series->attachAxis(m_axisX);
        series->attachAxis(m_axisY);
    }
    m_cursorLine = new QLineSeries(m_chart);
    m_cursorLine->setName(QStringLiteral("鼠标指示线"));
    QPen cursorPen(QColor(QStringLiteral("#64748B")), 1.0, Qt::DashLine);
    m_cursorLine->setPen(cursorPen);
    m_cursorLine->setVisible(false);
    m_chart->addSeries(m_cursorLine);
    m_cursorLine->attachAxis(m_axisX);
    m_cursorLine->attachAxis(m_axisY);

    auto *hoverChartView = new HoverChartView(m_chart, this);
    hoverChartView->hovered =
        [this](const QPointF &value, const QPoint &globalPosition) {
            showHoverInfo(value, globalPosition);
        };
    hoverChartView->hoverLeft = [this]() { hideHoverIndicator(); };
    hoverChartView->wheelZoom =
        [this](int delta, double anchorX) { applyWheelZoom(delta, anchorX); };
    hoverChartView->canPan = [this]() { return m_zoomMode == 1; };
    hoverChartView->panned = [this](int delta) { panXWaveform(delta); };
    hoverChartView->resetView = [this]() { resetWaveformView(); };
    m_chartView = hoverChartView;
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setOptimizationFlag(QGraphicsView::DontSavePainterState, true);
    m_chartView->setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, true);
    m_chartView->setObjectName(QStringLiteral("chartView"));
    centerLayout->addWidget(m_chartView, 1);

    auto *tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("monitorTabs"));
    tabs->setMinimumHeight(140);
    tabs->setMaximumHeight(160);
    const auto createReadoutPage = [this](const QStringList &captions,
                                          QVector<QLabel *> &values) {
        auto *page = new QWidget;
        auto *layout = new QGridLayout(page);
        layout->setContentsMargins(10, 7, 10, 7);
        layout->setHorizontalSpacing(12);
        layout->setVerticalSpacing(5);
        for (int index = 0; index < captions.size(); ++index) {
            auto *cell = new QWidget(page);
            auto *cellLayout = new QHBoxLayout(cell);
            cellLayout->setContentsMargins(0, 0, 0, 0);
            cellLayout->setSpacing(6);
            auto *caption = new QLabel(captions.at(index), cell);
            caption->setObjectName(QStringLiteral("mutedText"));
            auto *value = new QLabel(QStringLiteral("--"), cell);
            value->setObjectName(QStringLiteral("infoValue"));
            value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            cellLayout->addWidget(caption);
            cellLayout->addStretch();
            cellLayout->addWidget(value);
            values.append(value);
            layout->addWidget(cell, index / 4, index % 4);
        }
        return page;
    };
    const QStringList motionCaptions {
        QStringLiteral("实际位置"),
        QStringLiteral("位置指令"),
        QStringLiteral("实际速度"),
        QStringLiteral("速度指令"),
        QStringLiteral("实际电流"),
        QStringLiteral("电流指令"),
        QStringLiteral("位置误差"),
        QStringLiteral("参数监视"),
    };
    const QStringList performanceCaptions {
        QStringLiteral("KCP · Pn031"),
        QStringLiteral("KCI · Pn032"),
        QStringLiteral("KVP · Pn033"),
        QStringLiteral("KVI · Pn034"),
        QStringLiteral("KPP1 · Pn065"),
        QStringLiteral("KPP2 · Pn063"),
        QStringLiteral("KPVFR · Pn064"),
    };
    tabs->addTab(createReadoutPage(motionCaptions, m_motionValues),
                 QStringLiteral("运动"));
    tabs->addTab(createReadoutPage(performanceCaptions, m_performanceValues),
                 QStringLiteral("性能参数"));
    centerLayout->addWidget(tabs);
    root->addWidget(center, 1);

    auto *rightPanel = new QFrame(this);
    rightPanel->setObjectName(QStringLiteral("panel"));
    rightPanel->setFixedWidth(286);
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(8, 8, 8, 8);
    rightLayout->setSpacing(5);
    rightLayout->addWidget(sectionLabel(QStringLiteral("信道显示"), rightPanel));

    auto *channelGrid = new QGridLayout;
    channelGrid->setHorizontalSpacing(5);
    channelGrid->setVerticalSpacing(4);
    channelGrid->setColumnStretch(0, 1);
    const QStringList coefficientHeaders {
        QStringLiteral("名称"),
        QStringLiteral("+"),
        QStringLiteral("×"),
        QStringLiteral("单位"),
    };
    for (int column = 0; column < coefficientHeaders.size(); ++column) {
        auto *header = new QLabel(coefficientHeaders.at(column), rightPanel);
        header->setObjectName(QStringLiteral("mutedText"));
        header->setAlignment((column == 0 || column == 3)
                                 ? Qt::AlignLeft | Qt::AlignVCenter
                                 : Qt::AlignCenter);
        QFont headerFont = header->font();
        headerFont.setBold(true);
        header->setFont(headerFont);
        channelGrid->addWidget(header, 0, column);
    }

    const auto createCoefficientSpin = [rightPanel](double value) {
        auto *spin = new QDoubleSpinBox(rightPanel);
        spin->setRange(-1000000.0, 1000000.0);
        spin->setDecimals(3);
        spin->setSingleStep(0.1);
        spin->setValue(value);
        spin->setFixedWidth(54);
        spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
        return spin;
    };

    for (int displayIndex = 0; displayIndex < MetricCount; ++displayIndex) {
        const int channel = channelDisplayOrder[displayIndex];
        const int row = displayIndex + 1;
        auto *check = new QCheckBox(shortChannelNames.at(channel), rightPanel);
        check->setChecked(channel == PositionMetric || channel == PositionCommandMetric);
        check->setStyleSheet(QStringLiteral("color: %1; font-weight: 600;")
                                 .arg(channelColors[channel].name()));
        m_channelChecks[channel] = check;

        auto *offsetSpin = createCoefficientSpin(0.0);
        offsetSpin->setToolTip(QStringLiteral("偏移 B，默认 0"));
        auto *gainSpin = createCoefficientSpin(1.0);
        gainSpin->setToolTip(QStringLiteral("增益 K，默认 1"));
        auto *unitEdit = new QLineEdit(channelUnits.at(channel), rightPanel);
        unitEdit->setFixedWidth(52);
        unitEdit->setToolTip(QStringLiteral("可编辑显示单位"));
        m_channelOffsetSpins[channel] = offsetSpin;
        m_channelGainSpins[channel] = gainSpin;
        m_channelUnitEdits[channel] = unitEdit;

        channelGrid->addWidget(check, row, 0);
        channelGrid->addWidget(offsetSpin, row, 1, Qt::AlignCenter);
        channelGrid->addWidget(gainSpin, row, 2, Qt::AlignCenter);
        channelGrid->addWidget(unitEdit, row, 3);
        connect(check, &QCheckBox::toggled, this, [this, channel](bool checked) {
            m_series.at(channel)->setVisible(checked);
            updateTimeAxisRange();
            updateAxisRange();
        });
        connect(offsetSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this, channel](double value) {
                    m_channelOffsets[channel] = value;
                    applyChannelTransform(channel);
                    updateAxisRange();
                });
        connect(gainSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this, channel](double value) {
                    m_channelGains[channel] = value;
                    applyChannelTransform(channel);
                    updateAxisRange();
                });
    }
    rightLayout->addLayout(channelGrid);
    rightLayout->addWidget(separator(rightPanel));

    rightLayout->addWidget(sectionLabel(QStringLiteral("X 轴时间"), rightPanel));
    auto *timeForm = new QFormLayout;
    timeForm->setContentsMargins(0, 0, 0, 0);
    timeForm->setSpacing(4);
    m_timeWindowSpin = new QDoubleSpinBox(rightPanel);
    m_timeWindowSpin->setRange(1.0, 300.0);
    m_timeWindowSpin->setDecimals(0);
    m_timeWindowSpin->setSingleStep(1.0);
    m_timeWindowSpin->setSuffix(QStringLiteral(" S"));
    m_timeWindowSpin->setValue(m_timeWindowSeconds);
    timeForm->addRow(QStringLiteral("时间长度"), m_timeWindowSpin);
    rightLayout->addLayout(timeForm);

    rightLayout->addWidget(separator(rightPanel));
    rightLayout->addWidget(sectionLabel(QStringLiteral("Y 轴设置"), rightPanel));
    auto *yForm = new QFormLayout;
    yForm->setContentsMargins(0, 0, 0, 0);
    yForm->setSpacing(4);
    m_yModeCombo = new QComboBox(rightPanel);
    m_yModeCombo->addItem(QStringLiteral("Auto"), true);
    m_yModeCombo->addItem(QStringLiteral("Manual"), false);
    m_yMinSpin = new QDoubleSpinBox(rightPanel);
    m_yMinSpin->setRange(-1000000000.0, 1000000000.0);
    m_yMinSpin->setDecimals(2);
    m_yMinSpin->setValue(-100.0);
    m_yMinSpin->setEnabled(false);
    m_yMaxSpin = new QDoubleSpinBox(rightPanel);
    m_yMaxSpin->setRange(-1000000000.0, 1000000000.0);
    m_yMaxSpin->setDecimals(2);
    m_yMaxSpin->setValue(100.0);
    m_yMaxSpin->setEnabled(false);
    yForm->addRow(QStringLiteral("模式"), m_yModeCombo);
    yForm->addRow(QStringLiteral("Min"), m_yMinSpin);
    yForm->addRow(QStringLiteral("Max"), m_yMaxSpin);
    rightLayout->addLayout(yForm);
    rightLayout->addStretch();
    root->addWidget(rightPanel);

    m_renderTimer = new QTimer(this);
    m_renderTimer->setInterval(5);
    m_renderTimer->setSingleShot(true);
    connect(m_renderTimer, &QTimer::timeout,
            this, &MonitorPage::flushPendingSamples);
    m_playbackTimer = new QTimer(this);
    m_playbackTimer->setInterval(5);
    m_playbackTimer->setTimerType(Qt::PreciseTimer);
    connect(m_playbackTimer, &QTimer::timeout,
            this, &MonitorPage::processQueuedTelemetry);
    connect(m_runButton, &QPushButton::clicked, this, &MonitorPage::toggleRunning);
    connect(clearButton, &QPushButton::clicked, this, &MonitorPage::clearSeries);
    connect(saveWaveformButton, &QPushButton::clicked,
            this, &MonitorPage::saveWaveform);
    connect(loadWaveformButton, &QPushButton::clicked,
            this, &MonitorPage::loadWaveform);
    connect(m_metricCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MonitorPage::metricChanged);
    connect(m_timeWindowSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double seconds) {
                m_timeWindowSeconds = seconds;
                m_showFullWaveform = false;
                m_manualXView = false;
                applyTimeWindow();
            });
    connect(m_yModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() {
                const bool automatic = m_yModeCombo->currentData().toBool();
                m_yMinSpin->setEnabled(!automatic);
                m_yMaxSpin->setEnabled(!automatic);
                updateAxisRange();
            });
    connect(m_yMinSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this]() {
                if (!m_yModeCombo->currentData().toBool()) {
                    updateAxisRange();
                }
            });
    connect(m_yMaxSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this]() {
                if (!m_yModeCombo->currentData().toBool()) {
                    updateAxisRange();
                }
            });
    connect(xZoomButton, &QPushButton::clicked, this,
            [this, xZoomButton, yZoomButton]() {
                if (xZoomButton->isChecked()) {
                    yZoomButton->setChecked(false);
                    m_zoomMode = 1;
                    m_chartView->setCursor(Qt::OpenHandCursor);
                } else {
                    m_zoomMode = 0;
                    m_chartView->setCursor(Qt::CrossCursor);
                }
            });
    connect(yZoomButton, &QPushButton::clicked, this,
            [this, xZoomButton, yZoomButton]() {
                if (yZoomButton->isChecked()) {
                    xZoomButton->setChecked(false);
                    m_zoomMode = 2;
                    m_chartView->setCursor(Qt::CrossCursor);
                } else {
                    m_zoomMode = 0;
                    m_chartView->setCursor(Qt::CrossCursor);
                }
            });
    connect(resetScaleButton, &QPushButton::clicked,
            this, &MonitorPage::resetWaveformView);

    if (m_parameterModel) {
        connect(m_parameterModel, &ParameterModel::dataChanged,
                this, &MonitorPage::updateParameterSummary);
        connect(m_parameterModel, &ParameterModel::modelReset,
                this, &MonitorPage::updateParameterSummary);
    }
    updateParameterSummary();
    metricChanged();
    setConnected(false);
}

void MonitorPage::updateTelemetry(const DeviceSerialService::Telemetry &telemetry)
{
    m_latest = telemetry;
    m_hasLatest = true;
    updateLiveSummary(telemetry);
    if (m_running) {
        enqueueTelemetry(telemetry);
    }
}

void MonitorPage::setConnected(bool connected)
{
    m_runButton->setEnabled(connected);
    if (!connected) {
        while (!m_telemetryQueue.isEmpty()) {
            processTelemetryFrame(m_telemetryQueue.dequeue());
        }
        flushPendingSamples();
        m_running = false;
        m_renderTimer->stop();
        m_playbackTimer->stop();
        m_playbackClock.invalidate();
        m_playbackFrameCount = 0;
        m_runButton->setText(QStringLiteral("开始"));
    }
}

void MonitorPage::enqueueTelemetry(
    const DeviceSerialService::Telemetry &telemetry)
{
    m_telemetryQueue.enqueue(telemetry);
    if (!m_playbackClock.isValid()) {
        m_playbackClock.start();
        m_playbackFrameCount = 0;
    }
    if (!m_playbackTimer->isActive()) {
        m_playbackTimer->start();
    }
}

void MonitorPage::processQueuedTelemetry()
{
    if (!m_playbackClock.isValid()) {
        m_playbackClock.start();
        m_playbackFrameCount = 0;
    }

    const qint64 framesDue =
        m_playbackClock.nsecsElapsed() / 2000000LL + 1;
    while (m_playbackFrameCount < framesDue && !m_telemetryQueue.isEmpty()) {
        processTelemetryFrame(m_telemetryQueue.dequeue());
        ++m_playbackFrameCount;
    }

    if (m_telemetryQueue.isEmpty()) {
        m_playbackTimer->stop();
        m_playbackClock.invalidate();
        m_playbackFrameCount = 0;
    }
}

void MonitorPage::processTelemetryFrame(
    const DeviceSerialService::Telemetry &telemetry)
{
    m_latest = telemetry;
    appendSample();
}

void MonitorPage::appendSample()
{
    if (!m_hasLatest || !m_latest.timestamp.isValid()) {
        return;
    }

    const double nowMs = m_latest.timestampMs;
    if (m_firstTimestampMs <= 0.0) {
        m_firstTimestampMs = nowMs;
    }
    m_elapsedSeconds =
        qMax(m_elapsedSeconds, (nowMs - m_firstTimestampMs) / 1000.0);

    QVector<int> updatedChannels;
    switch (m_latest.updatedType) {
    case 0x01:
        updatedChannels = {PositionCommandMetric};
        break;
    case 0x02:
    case 0x0C:
        updatedChannels = {PositionMetric};
        break;
    case 0x03:
        updatedChannels = {SpeedCommandMetric, SpeedMetric};
        break;
    case 0x04:
        updatedChannels = {CurrentCommandMetric, CurrentMetric};
        break;
    case 0x07:
        updatedChannels = {ParameterMonitorMetric};
        break;
    case 0x12:
        updatedChannels = {PositionErrorMetric};
        break;
    default:
        break;
    }

    for (int channel : updatedChannels) {
        if (!m_channelChecks.at(channel)->isChecked()) {
            continue;
        }
        const double value = metricValue(m_latest, channel);
        QVector<QPointF> &samples = m_samples[channel];
        samples.append(QPointF(m_elapsedSeconds, value));
        m_channelDirty[channel] = true;
        m_seriesDirty = true;
    }

    if (m_seriesDirty && !m_renderTimer->isActive()) {
        m_renderTimer->start();
    }
}

void MonitorPage::toggleRunning()
{
    m_running = !m_running;
    m_runButton->setText(m_running ? QStringLiteral("暂停") : QStringLiteral("开始"));
    if (!m_running) {
        while (!m_telemetryQueue.isEmpty()) {
            processTelemetryFrame(m_telemetryQueue.dequeue());
        }
        flushPendingSamples();
        m_renderTimer->stop();
        m_playbackTimer->stop();
        m_playbackClock.invalidate();
        m_playbackFrameCount = 0;
    }
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
    m_seriesDirty = false;
    m_showFullWaveform = false;
    m_manualXView = false;
    m_channelDirty.fill(false);
    m_renderTimer->stop();
    m_playbackTimer->stop();
    m_telemetryQueue.clear();
    m_playbackClock.invalidate();
    m_playbackFrameCount = 0;
    m_axisX->setRange(0, m_timeWindowSeconds);
    hideHoverIndicator();
    updateTimeAxisRange();
    updateAxisRange();
}

void MonitorPage::metricChanged()
{
    const int groupIndex = m_metricCombo->currentData().toInt();
    if (groupIndex < 0 || groupIndex >= m_metricGroups.size()) {
        return;
    }

    const QVector<int> &group = m_metricGroups.at(groupIndex);
    for (int channel = 0; channel < m_channelChecks.size(); ++channel) {
        const bool selected = group.contains(channel);
        m_channelChecks.at(channel)->setChecked(selected);
        m_series.at(channel)->setVisible(selected);
    }
    for (int channel = 0; channel < m_series.size(); ++channel) {
        QPen pen(channelColors[channel]);
        if (isCommandChannel(channel)) {
            pen.setStyle(Qt::DashLine);
        }
        pen.setWidthF(group.contains(channel) ? 2.4 : 1.5);
        m_series.at(channel)->setPen(pen);
    }
    updateTimeAxisRange();
    updateAxisRange();
}

void MonitorPage::saveWaveform()
{
    bool hasSamples = false;
    for (const QVector<QPointF> &samples : m_samples) {
        hasSamples = hasSamples || !samples.isEmpty();
    }
    if (!hasSamples) {
        QMessageBox::information(this, QStringLiteral("保存波形"),
                                 QStringLiteral("当前没有可保存的波形数据"));
        return;
    }

    const QString fileName = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存波形"),
        QStringLiteral("KaiserStudio_Waveform.csv"),
        QStringLiteral("波形 CSV (*.csv)"));
    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("保存波形失败"), file.errorString());
        return;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream << "# KaiserStudioScopeRecording,1\n";
    stream << "# TimeWindow," << QString::number(m_timeWindowSeconds, 'f', 0) << '\n';
    for (int channel = 0; channel < m_samples.size(); ++channel) {
        QString unit = m_channelUnitEdits.at(channel)->text().trimmed();
        unit.replace(QLatin1Char(','), QLatin1Char('_'));
        unit.replace(QLatin1Char('\n'), QLatin1Char(' '));
        stream << "# ChannelConfig," << shortChannelNames.at(channel) << ','
               << QString::number(m_channelGains.at(channel), 'g', 12) << ','
               << QString::number(m_channelOffsets.at(channel), 'g', 12) << ','
               << unit << ','
               << (m_channelChecks.at(channel)->isChecked() ? 1 : 0) << '\n';
    }
    stream << "TimeSeconds,Channel,RawValue\n";
    for (int channel = 0; channel < m_samples.size(); ++channel) {
        for (const QPointF &point : m_samples.at(channel)) {
            stream << QString::number(point.x(), 'f', 6) << ','
                   << shortChannelNames.at(channel) << ','
                   << QString::number(point.y(), 'g', 15) << '\n';
        }
    }
    emit statusMessage(QStringLiteral("波形已保存：%1").arg(fileName));
}

void MonitorPage::loadWaveform()
{
    const QString fileName = QFileDialog::getOpenFileName(
        this, QStringLiteral("读取波形"), QString(),
        QStringLiteral("波形 CSV (*.csv);;所有文件 (*.*)"));
    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("读取波形失败"), file.errorString());
        return;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    QString line = stream.readLine().trimmed();
    if (line != QStringLiteral("# KaiserStudioScopeRecording,1")) {
        QMessageBox::warning(
            this, QStringLiteral("读取波形失败"),
            QStringLiteral("不是 KaiserStudio 波形记录文件，或文件版本不受支持。"));
        return;
    }

    QVector<QVector<QPointF>> loaded(MetricCount);
    QVector<double> gains(MetricCount, 1.0);
    QVector<double> offsets(MetricCount, 0.0);
    QVector<QString> units(MetricCount);
    QVector<bool> visible(MetricCount, false);
    for (int channel = 0; channel < MetricCount; ++channel) {
        units[channel] = channelUnits.at(channel);
    }

    double loadedTimeWindow = m_timeWindowSeconds;
    bool hasDataHeader = false;
    int lineNumber = 1;
    double maximumTime = 0.0;
    while (!stream.atEnd()) {
        line = stream.readLine().trimmed();
        ++lineNumber;
        if (line.isEmpty()) {
            continue;
        }
        if (line.startsWith(QStringLiteral("# TimeWindow,"))) {
            bool ok = false;
            const double value = line.mid(13).toDouble(&ok);
            if (ok && value > 0.0) {
                loadedTimeWindow = value;
            }
            continue;
        }
        if (line.startsWith(QStringLiteral("# ChannelConfig,"))) {
            const QStringList fields = line.split(QLatin1Char(','));
            if (fields.size() < 6) {
                continue;
            }
            const int channel = channelIndexForKey(fields.at(1).trimmed());
            bool gainOk = false;
            bool offsetOk = false;
            if (channel < 0) {
                continue;
            }
            const double gain = fields.at(2).toDouble(&gainOk);
            const double offset = fields.at(3).toDouble(&offsetOk);
            if (gainOk && offsetOk) {
                gains[channel] = gain;
                offsets[channel] = offset;
            }
            units[channel] = fields.at(4).trimmed();
            visible[channel] = fields.at(5).trimmed().toInt() != 0;
            continue;
        }
        if (line == QStringLiteral("TimeSeconds,Channel,RawValue")) {
            hasDataHeader = true;
            continue;
        }
        if (!hasDataHeader || line.startsWith(QLatin1Char('#'))) {
            continue;
        }

        const QStringList fields = line.split(QLatin1Char(','));
        if (fields.size() != 3) {
            QMessageBox::warning(
                this, QStringLiteral("读取波形失败"),
                QStringLiteral("第 %1 行格式无效。").arg(lineNumber));
            return;
        }
        const int channel = channelIndexForKey(fields.at(1).trimmed());
        bool timeOk = false;
        bool valueOk = false;
        const double time = fields.at(0).toDouble(&timeOk);
        const double rawValue = fields.at(2).toDouble(&valueOk);
        if (channel < 0 || !timeOk || !valueOk || !std::isfinite(time)
            || !std::isfinite(rawValue) || time < 0.0) {
            QMessageBox::warning(
                this, QStringLiteral("读取波形失败"),
                QStringLiteral("第 %1 行包含无效的时间或数值。").arg(lineNumber));
            return;
        }
        loaded[channel].append(QPointF(time, rawValue));
        maximumTime = qMax(maximumTime, time);
    }

    bool hasSamples = false;
    for (QVector<QPointF> &samples : loaded) {
        std::sort(samples.begin(), samples.end(),
                  [](const QPointF &left, const QPointF &right) {
                      return left.x() < right.x();
                  });
        hasSamples = hasSamples || !samples.isEmpty();
    }
    if (!hasSamples) {
        QMessageBox::warning(this, QStringLiteral("读取波形失败"),
                             QStringLiteral("文件中没有波形采样数据。"));
        return;
    }

    m_running = false;
    m_runButton->setText(QStringLiteral("开始"));
    m_renderTimer->stop();
    m_playbackTimer->stop();
    m_telemetryQueue.clear();
    m_playbackClock.invalidate();
    m_playbackFrameCount = 0;

    m_samples = loaded;
    m_channelGains = gains;
    m_channelOffsets = offsets;
    m_channelDirty.fill(false);
    m_seriesDirty = false;
    m_elapsedSeconds = maximumTime;
    m_firstTimestampMs = 0.0;
    m_latest = DeviceSerialService::Telemetry();
    m_hasLatest = true;
    m_showFullWaveform = true;
    m_manualXView = false;
    m_yScaleFactor = 1.0;

    m_timeWindowSeconds =
        qBound(1.0, static_cast<double>(qRound(loadedTimeWindow)), 300.0);
    {
        const QSignalBlocker blocker(m_timeWindowSpin);
        m_timeWindowSpin->setValue(m_timeWindowSeconds);
    }
    {
        const QSignalBlocker yModeBlocker(m_yModeCombo);
        const QSignalBlocker minBlocker(m_yMinSpin);
        const QSignalBlocker maxBlocker(m_yMaxSpin);
        m_yModeCombo->setCurrentIndex(0);
        m_yMinSpin->setValue(-100.0);
        m_yMaxSpin->setValue(100.0);
        m_yMinSpin->setEnabled(false);
        m_yMaxSpin->setEnabled(false);
    }

    for (int channel = 0; channel < MetricCount; ++channel) {
        {
            const QSignalBlocker blocker(m_channelGainSpins.at(channel));
            m_channelGainSpins.at(channel)->setValue(gains.at(channel));
        }
        {
            const QSignalBlocker blocker(m_channelOffsetSpins.at(channel));
            m_channelOffsetSpins.at(channel)->setValue(offsets.at(channel));
        }
        {
            const QSignalBlocker blocker(m_channelUnitEdits.at(channel));
            m_channelUnitEdits.at(channel)->setText(units.at(channel));
        }
        {
            const QSignalBlocker blocker(m_channelChecks.at(channel));
            m_channelChecks.at(channel)->setChecked(visible.at(channel));
        }
        m_series.at(channel)->setVisible(visible.at(channel));
        applyChannelTransform(channel);
    }

    hideHoverIndicator();
    updateTimeAxisRange();
    updateAxisRange();
    emit statusMessage(QStringLiteral("波形已读取：%1").arg(fileName));
}

double MonitorPage::metricValue(const DeviceSerialService::Telemetry &telemetry,
                                int channel) const
{
    switch (static_cast<Metric>(channel)) {
    case PositionMetric:
        return telemetry.position;
    case PositionCommandMetric:
        return telemetry.positionCommand;
    case SpeedMetric:
        return telemetry.scopeSpeed;
    case SpeedCommandMetric:
        return telemetry.speedCommand;
    case CurrentMetric:
        return telemetry.scopeCurrent;
    case CurrentCommandMetric:
        return telemetry.currentCommand;
    case PositionErrorMetric:
        return telemetry.positionError;
    case ParameterMonitorMetric:
        return telemetry.parameterMonitor;
    }
    return 0.0;
}

bool MonitorPage::isCommandChannel(int channel)
{
    return channel == PositionCommandMetric || channel == SpeedCommandMetric
        || channel == CurrentCommandMetric;
}

void MonitorPage::applyTimeWindow()
{
    if (!m_hasLatest) {
        m_axisX->setRange(0, m_timeWindowSeconds);
        return;
    }

    for (int channel = 0; channel < m_samples.size(); ++channel) {
        applyChannelTransform(channel);
    }
    updateTimeAxisRange();
}

void MonitorPage::flushPendingSamples()
{
    if (!m_seriesDirty) {
        return;
    }
    updateTimeAxisRange();
    for (int channel = 0; channel < m_channelDirty.size(); ++channel) {
        if (m_channelDirty.at(channel)) {
            applyChannelTransform(channel);
            m_channelDirty[channel] = false;
        }
    }
    updateAxisRange();
    m_seriesDirty = false;
}

void MonitorPage::updateTimeAxisRange()
{
    if (!m_hasLatest) {
        m_axisX->setRange(0, m_timeWindowSeconds);
        return;
    }
    if (m_manualXView) {
        if (m_manualXEnd > m_manualXStart) {
            m_axisX->setRange(m_manualXStart, m_manualXEnd);
        }
        return;
    }
    if (!m_showFullWaveform) {
        m_axisX->setRange(qMax(0.0, m_elapsedSeconds - m_timeWindowSeconds),
                          qMax(m_timeWindowSeconds, m_elapsedSeconds));
        return;
    }

    double firstTime = 0.0;
    double lastTime = 0.0;
    bool hasData = false;
    for (int channel = 0; channel < m_samples.size(); ++channel) {
        if (!m_channelChecks.at(channel)->isChecked()
            || m_samples.at(channel).isEmpty()) {
            continue;
        }
        const double channelFirst = m_samples.at(channel).first().x();
        const double channelLast = m_samples.at(channel).last().x();
        if (!hasData) {
            firstTime = channelFirst;
            lastTime = channelLast;
            hasData = true;
        } else {
            firstTime = qMin(firstTime, channelFirst);
            lastTime = qMax(lastTime, channelLast);
        }
    }
    if (!hasData) {
        m_axisX->setRange(0, m_timeWindowSeconds);
        return;
    }
    if (lastTime - firstTime < 0.002) {
        lastTime = firstTime + 0.002;
    }
    m_axisX->setRange(firstTime, lastTime);
}

void MonitorPage::applyWheelZoom(int wheelDelta, double anchorX)
{
    if (m_zoomMode == 1) {
        const double currentStart = m_axisX->min();
        const double currentEnd = m_axisX->max();
        const double currentSpan = qMax(0.002, currentEnd - currentStart);
        const double factor = wheelDelta > 0 ? 0.8 : 1.25;
        const double availableSpan = qMax(m_elapsedSeconds, m_timeWindowSeconds);
        const double newSpan = qBound(0.002, currentSpan * factor, availableSpan);
        const double anchorRatio =
            qBound(0.0, (anchorX - currentStart) / currentSpan, 1.0);
        m_manualXStart = anchorX - anchorRatio * newSpan;
        m_manualXEnd = m_manualXStart + newSpan;
        m_manualXView = true;
        m_showFullWaveform = false;
        updateTimeAxisRange();
        updateAxisRange();
        return;
    }
    if (m_zoomMode == 2) {
        applyYScale(wheelDelta > 0 ? 1.0 / 1.25 : 1.25);
    }
}

void MonitorPage::panXWaveform(int deltaPixels)
{
    if (m_zoomMode != 1 || !m_chartView || !m_chartView->chart()) {
        return;
    }
    const double plotWidth = m_chartView->chart()->plotArea().width();
    if (plotWidth <= 0.0) {
        return;
    }

    const double currentStart = m_axisX->min();
    const double currentEnd = m_axisX->max();
    const double span = currentEnd - currentStart;
    if (span <= 0.0) {
        return;
    }

    double dataStart = currentStart;
    double dataEnd = currentEnd;
    bool hasData = false;
    for (int channel = 0; channel < m_samples.size(); ++channel) {
        if (!m_channelChecks.at(channel)->isChecked()
            || m_samples.at(channel).isEmpty()) {
            continue;
        }
        const double channelStart = m_samples.at(channel).first().x();
        const double channelEnd = m_samples.at(channel).last().x();
        if (!hasData) {
            dataStart = channelStart;
            dataEnd = channelEnd;
            hasData = true;
        } else {
            dataStart = qMin(dataStart, channelStart);
            dataEnd = qMax(dataEnd, channelEnd);
        }
    }

    const double shift = -static_cast<double>(deltaPixels) / plotWidth * span;
    double newStart = currentStart + shift;
    if (hasData && dataEnd - dataStart > span) {
        newStart = qBound(dataStart, newStart, dataEnd - span);
    }
    m_manualXStart = newStart;
    m_manualXEnd = newStart + span;
    m_manualXView = true;
    m_showFullWaveform = false;
    updateTimeAxisRange();
    updateAxisRange();
}

void MonitorPage::resetWaveformView()
{
    m_showFullWaveform = true;
    m_manualXView = false;
    m_yScaleFactor = 1.0;
    {
        const QSignalBlocker minBlocker(m_yMinSpin);
        const QSignalBlocker maxBlocker(m_yMaxSpin);
        m_yMinSpin->setValue(-100.0);
        m_yMaxSpin->setValue(100.0);
    }
    m_yModeCombo->setCurrentIndex(0);
    m_yMinSpin->setEnabled(false);
    m_yMaxSpin->setEnabled(false);
    updateTimeAxisRange();
    updateAxisRange();
    hideHoverIndicator();
}

void MonitorPage::applyYScale(double factor)
{
    if (m_yModeCombo->currentData().toBool()) {
        m_yScaleFactor = qBound(0.1, m_yScaleFactor * factor, 25.0);
        updateAxisRange();
        return;
    }

    const double center = (m_yMinSpin->value() + m_yMaxSpin->value()) / 2.0;
    const double halfRange = (m_yMaxSpin->value() - m_yMinSpin->value()) * factor / 2.0;
    const QSignalBlocker minBlocker(m_yMinSpin);
    const QSignalBlocker maxBlocker(m_yMaxSpin);
    m_yMinSpin->setValue(center - halfRange);
    m_yMaxSpin->setValue(center + halfRange);
    updateAxisRange();
}

double MonitorPage::transformedValue(int channel, double rawValue) const
{
    if (channel < 0 || channel >= m_channelGains.size()) {
        return rawValue;
    }
    return rawValue * m_channelGains.at(channel) + m_channelOffsets.at(channel);
}

int MonitorPage::channelDisplayDecimals(int channel) const
{
    if (channel < 0 || channel >= MetricCount) {
        return 3;
    }
    const bool transformed =
        qAbs(m_channelGains.at(channel) - 1.0) > 0.0000001
        || qAbs(m_channelOffsets.at(channel)) > 0.0000001;
    return transformed ? qMin(7, channelDecimals[channel] + 3)
                       : channelDecimals[channel];
}

QString MonitorPage::formattedChannelValue(int channel, double value) const
{
    return QString::number(value, 'f', channelDisplayDecimals(channel));
}

void MonitorPage::applyChannelTransform(int channel)
{
    if (channel < 0 || channel >= m_series.size()) {
        return;
    }
    const QVector<QPointF> &samples = m_samples.at(channel);
    QVector<QPointF> displayed;
    displayed.reserve(samples.size());
    for (const QPointF &point : samples) {
        displayed.append(QPointF(point.x(), transformedValue(channel, point.y())));
    }
    m_series.at(channel)->replace(displayed);
}

void MonitorPage::showHoverInfo(const QPointF &chartValue,
                                const QPoint &globalPosition)
{
    double cursorTime = chartValue.x();
    bool hasNearestTime = false;
    double nearestTimeDistance = 0.0;
    for (int channel = 0; channel < m_samples.size(); ++channel) {
        if (!m_channelChecks.at(channel)->isChecked()) {
            continue;
        }
        for (const QPointF &point : m_samples.at(channel)) {
            const double distance = qAbs(point.x() - chartValue.x());
            if (!hasNearestTime || distance < nearestTimeDistance) {
                hasNearestTime = true;
                nearestTimeDistance = distance;
                cursorTime = point.x();
            }
        }
    }

    if (hasNearestTime && m_cursorLine) {
        m_cursorLine->replace(QVector<QPointF> {
            QPointF(cursorTime, m_axisY->min()),
            QPointF(cursorTime, m_axisY->max()),
        });
        m_cursorLine->setVisible(true);
    }

    QStringList lines {
        QStringLiteral("时间：%1 S").arg(cursorTime, 0, 'f', 3),
    };

    for (int channel = 0; channel < m_samples.size(); ++channel) {
        if (!m_channelChecks.at(channel)->isChecked()) {
            continue;
        }
        const QVector<QPointF> &samples = m_samples.at(channel);
        if (samples.isEmpty()) {
            continue;
        }

        int nearestIndex = 0;
        double nearestDistance = qAbs(samples.first().x() - cursorTime);
        for (int index = 1; index < samples.size(); ++index) {
            const double distance = qAbs(samples.at(index).x() - cursorTime);
            if (distance < nearestDistance) {
                nearestDistance = distance;
                nearestIndex = index;
            }
        }

        const double value =
            transformedValue(channel, samples.at(nearestIndex).y());
        const QString unit = m_channelUnitEdits.at(channel)->text().trimmed();
        lines.append(QStringLiteral("%1：%2 %3")
                         .arg(shortChannelNames.at(channel))
                         .arg(formattedChannelValue(channel, value))
                         .arg(unit));
    }

    if (lines.size() == 1) {
        lines.append(QStringLiteral("暂无可用采样数据"));
    }
    QToolTip::showText(globalPosition, lines.join(QLatin1Char('\n')), m_chartView);
}

void MonitorPage::hideHoverIndicator()
{
    if (m_cursorLine) {
        m_cursorLine->setVisible(false);
        m_cursorLine->clear();
    }
}

void MonitorPage::updateAxisRange()
{
    int visibleDecimals = 0;
    for (int channel = 0; channel < m_channelChecks.size(); ++channel) {
        if (m_channelChecks.at(channel)->isChecked()) {
            visibleDecimals = qMax(visibleDecimals, channelDisplayDecimals(channel));
        }
    }
    m_axisY->setLabelFormat(QStringLiteral("%.%1f").arg(visibleDecimals));

    if (!m_yModeCombo->currentData().toBool()) {
        double minimum = qMin(m_yMinSpin->value(), m_yMaxSpin->value());
        double maximum = qMax(m_yMinSpin->value(), m_yMaxSpin->value());
        if (qFuzzyCompare(minimum, maximum)) {
            minimum -= 1.0;
            maximum += 1.0;
        }
        m_axisY->setRange(minimum, maximum);
        return;
    }

    double minimum = 0.0;
    double maximum = 0.0;
    bool hasValue = false;
    for (int channel = 0; channel < m_samples.size(); ++channel) {
        if (!m_channelChecks.at(channel)->isChecked()) {
            continue;
        }
        for (const QPointF &point : m_samples.at(channel)) {
            if (point.x() < m_axisX->min()) {
                continue;
            }
            const double value = transformedValue(channel, point.y());
            if (!hasValue) {
                minimum = maximum = value;
                hasValue = true;
            } else {
                minimum = qMin(minimum, value);
                maximum = qMax(maximum, value);
            }
        }
    }

    if (!hasValue) {
        m_axisY->setRange(-100 * m_yScaleFactor, 100 * m_yScaleFactor);
        return;
    }
    if (qFuzzyCompare(minimum, maximum)) {
        minimum -= 1.0;
        maximum += 1.0;
    }
    const double center = (minimum + maximum) / 2.0;
    const double halfRange = qMax(0.1, (maximum - minimum) * 0.55 * m_yScaleFactor);
    m_axisY->setRange(center - halfRange, center + halfRange);
}

void MonitorPage::updateLiveSummary(const DeviceSerialService::Telemetry &telemetry)
{
    if (m_motionValues.size() != 8) {
        return;
    }
    m_motionValues.at(0)->setText(QString::number(telemetry.position, 'f', 0));
    m_motionValues.at(1)->setText(QString::number(telemetry.positionCommand, 'f', 0));
    m_motionValues.at(2)->setText(QString::number(telemetry.scopeSpeed, 'f', 1));
    m_motionValues.at(3)->setText(QString::number(telemetry.speedCommand, 'f', 1));
    m_motionValues.at(4)->setText(QString::number(telemetry.scopeCurrent, 'f', 2));
    m_motionValues.at(5)->setText(QString::number(telemetry.currentCommand, 'f', 2));
    m_motionValues.at(6)->setText(QString::number(telemetry.positionError, 'f', 0));
    m_motionValues.at(7)->setText(QString::number(telemetry.parameterMonitor, 'f', 0));
}

void MonitorPage::updateParameterSummary()
{
    if (!m_parameterModel || m_performanceValues.size() != 7) {
        return;
    }
    const QVector<quint16> parameters {
        31, 32, 33, 34, 65, 63, 64,
    };
    for (int index = 0; index < parameters.size(); ++index) {
        m_performanceValues.at(index)->setText(
            QString::number(m_parameterModel->value(parameters.at(index))));
    }
}
