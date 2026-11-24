#include "calibrationsettingsdialog.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

CalibrationSettingsDialog::CalibrationSettingsDialog(
    const CalibrationProfile &profile, QWidget *parent)
    : QDialog(parent)
    , m_profile(profile)
{
    setObjectName(QStringLiteral("calibrationSettingsDialog"));
    setWindowTitle(QStringLiteral("标定设置"));
    setWindowIcon(QIcon(QStringLiteral(":/pic/image/logo.ico")));
    setModal(true);
    setMinimumWidth(460);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(12);

    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("settingsCard"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(16, 14, 16, 14);
    cardLayout->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("标定方案"), card);
    title->setObjectName(QStringLiteral("settingsTitle"));
    cardLayout->addWidget(title);

    auto *form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(8);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_modelCombo = new QComboBox(card);
    for (const CalibrationProfile &preset : standardCalibrationProfiles()) {
        m_modelCombo->addItem(preset.displayName, preset.id);
    }
    m_rangeModeCombo = new QComboBox(card);
    m_rangeModeCombo->addItem(QStringLiteral("全量程"), 0);
    m_rangeModeCombo->addItem(QStringLiteral("自定义标定范围"), 1);

    m_maxForceSpin = new QSpinBox(card);
    m_maxForceSpin->setRange(1, 2000);
    m_maxForceSpin->setSuffix(QStringLiteral(" N"));

    m_stepSpin = new QSpinBox(card);
    m_stepSpin->setRange(1, 2000);
    m_stepSpin->setSuffix(QStringLiteral(" N"));

    m_toleranceSpin = new QDoubleSpinBox(card);
    m_toleranceSpin->setRange(0.1, 10.0);
    m_toleranceSpin->setDecimals(1);
    m_toleranceSpin->setSingleStep(0.1);
    m_toleranceSpin->setSuffix(QStringLiteral(" %"));

    m_algorithmValue = new QLabel(card);
    m_algorithmValue->setObjectName(QStringLiteral("settingsValue"));
    m_toleranceValue = new QLabel(card);
    m_toleranceValue->setObjectName(QStringLiteral("settingsValue"));

    const auto addCaption = [card](const QString &text) {
        auto *label = new QLabel(text, card);
        label->setObjectName(QStringLiteral("settingsCaption"));
        return label;
    };
    form->addRow(addCaption(QStringLiteral("设备型号")), m_modelCombo);
    form->addRow(addCaption(QStringLiteral("标定范围")), m_rangeModeCombo);
    form->addRow(addCaption(QStringLiteral("上限 X")), m_maxForceSpin);
    form->addRow(addCaption(QStringLiteral("分段 Δ")), m_stepSpin);
    form->addRow(addCaption(QStringLiteral("误差比例")), m_toleranceSpin);
    form->addRow(addCaption(QStringLiteral("标定算法")), m_algorithmValue);
    form->addRow(addCaption(QStringLiteral("允许误差")), m_toleranceValue);
    cardLayout->addLayout(form);

    m_summaryLabel = new QLabel(card);
    m_summaryLabel->setObjectName(QStringLiteral("settingsSummary"));
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setMinimumHeight(68);
    cardLayout->addWidget(m_summaryLabel);
    root->addWidget(card);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *cancelButton = new QPushButton(QStringLiteral("取消"), this);
    cancelButton->setObjectName(QStringLiteral("secondaryButton"));
    m_applyButton = new QPushButton(QStringLiteral("应用设置"), this);
    m_applyButton->setObjectName(QStringLiteral("primaryButton"));
    buttons->addWidget(cancelButton);
    buttons->addWidget(m_applyButton);
    root->addLayout(buttons);

    connect(m_modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CalibrationSettingsDialog::modelChanged);
    connect(m_rangeModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CalibrationSettingsDialog::rangeModeChanged);
    connect(m_maxForceSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &CalibrationSettingsDialog::forceRangeChanged);
    connect(m_stepSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &CalibrationSettingsDialog::updateSummary);
    connect(m_toleranceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &CalibrationSettingsDialog::updateSummary);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_applyButton, &QPushButton::clicked,
            this, &CalibrationSettingsDialog::acceptSettings);

    applyStyle();
    applyPreset(m_profile);
}

CalibrationProfile CalibrationSettingsDialog::calibrationProfile() const
{
    return m_profile;
}

void CalibrationSettingsDialog::modelChanged()
{
    CalibrationProfile profile =
        calibrationProfileById(m_modelCombo->currentData().toString());
    profile.calibrationMaxForceN = profile.ratedMaxForceN;
    profile.stepN = defaultCalibrationStep(profile.ratedMaxForceN);
    profile.tolerancePercent = 1.5;
    applyPreset(profile);
}

void CalibrationSettingsDialog::rangeModeChanged()
{
    const CalibrationProfile base =
        calibrationProfileById(m_modelCombo->currentData().toString());
    const bool narrow = base.allowsNarrowRange
        && m_rangeModeCombo->currentData().toInt() == 1;

    const QSignalBlocker maxBlocker(m_maxForceSpin);
    const QSignalBlocker stepBlocker(m_stepSpin);
    m_maxForceSpin->setEnabled(narrow);
    m_stepSpin->setEnabled(narrow);
    if (narrow) {
        const int suggestedMax = qMin(base.ratedMaxForceN, 100);
        m_maxForceSpin->setRange(1, base.ratedMaxForceN);
        m_maxForceSpin->setValue(suggestedMax);
        m_stepSpin->setRange(1, suggestedMax);
        m_stepSpin->setValue(defaultCalibrationStep(suggestedMax));
    } else {
        m_maxForceSpin->setRange(1, base.ratedMaxForceN);
        m_maxForceSpin->setValue(base.ratedMaxForceN);
        m_stepSpin->setRange(1, base.ratedMaxForceN);
        m_stepSpin->setValue(base.stepN);
    }
    updateSummary();
}

void CalibrationSettingsDialog::forceRangeChanged()
{
    const QSignalBlocker blocker(m_stepSpin);
    m_stepSpin->setRange(1, qMax(1, m_maxForceSpin->value()));
    updateSummary();
}

void CalibrationSettingsDialog::updateSummary()
{
    CalibrationProfile profile =
        calibrationProfileById(m_modelCombo->currentData().toString());
    const bool narrow = profile.allowsNarrowRange
        && m_rangeModeCombo->currentData().toInt() == 1;
    profile.calibrationMaxForceN =
        narrow ? m_maxForceSpin->value() : profile.ratedMaxForceN;
    profile.stepN = narrow ? m_stepSpin->value() : profile.stepN;
    profile.tolerancePercent = m_toleranceSpin->value();
    m_profile = profile;

    const bool lookup =
        profile.algorithm() == CalibrationAlgorithm::LookupTable;
    const QVector<int> points =
        buildCalibrationForcePoints(profile.calibrationMaxForceN, profile.stepN);
    QStringList allPoints;
    for (int point : points) {
        allPoints.append(QString::number(point));
    }

    m_algorithmValue->setText(
        lookup ? QStringLiteral("查表（Pn140–149、Pn180–189）")
               : QStringLiteral("K/B 拟合（Pn168、Pn169）"));
    m_toleranceValue->setText(
        QStringLiteral("±%1 N").arg(profile.toleranceN(), 0, 'f', 1));
    m_summaryLabel->setText(
        QStringLiteral(
            "当前型号：%1\n额定满量程：0–%2 N\n"
            "实际标定范围：0–%3 N，%4 N 分段\n"
            "测试点：%5 个")
            .arg(profile.displayName)
            .arg(profile.ratedMaxForceN)
            .arg(profile.calibrationMaxForceN)
            .arg(profile.stepN)
            .arg(points.size()));
    m_summaryLabel->setToolTip(
        QStringLiteral("测试点：%1 N").arg(allPoints.join(QStringLiteral(" / "))));
    m_applyButton->setEnabled(points.size() >= 2);
}

void CalibrationSettingsDialog::applyPreset(const CalibrationProfile &profile)
{
    const QSignalBlocker modelBlocker(m_modelCombo);
    const QSignalBlocker rangeBlocker(m_rangeModeCombo);
    const QSignalBlocker maxBlocker(m_maxForceSpin);
    const QSignalBlocker stepBlocker(m_stepSpin);
    const QSignalBlocker toleranceBlocker(m_toleranceSpin);

    const int modelIndex = m_modelCombo->findData(profile.id);
    if (modelIndex >= 0) {
        m_modelCombo->setCurrentIndex(modelIndex);
    }
    m_rangeModeCombo->setCurrentIndex(profile.allowsNarrowRange
                                          && profile.calibrationMaxForceN < profile.ratedMaxForceN
                                      ? 1
                                      : 0);
    m_rangeModeCombo->setEnabled(profile.allowsNarrowRange);
    m_maxForceSpin->setRange(1, qMax(1, profile.ratedMaxForceN));
    m_maxForceSpin->setValue(profile.calibrationMaxForceN);
    m_stepSpin->setRange(1, qMax(1, profile.calibrationMaxForceN));
    m_stepSpin->setValue(profile.stepN);
    m_toleranceSpin->setValue(profile.tolerancePercent);

    const bool narrow = profile.allowsNarrowRange
        && profile.calibrationMaxForceN < profile.ratedMaxForceN;
    m_maxForceSpin->setEnabled(narrow);
    m_stepSpin->setEnabled(narrow);
    updateSummary();
}

void CalibrationSettingsDialog::acceptSettings()
{
    updateSummary();
    if (buildCalibrationForcePoints(m_profile.calibrationMaxForceN,
                                    m_profile.stepN).size() < 2) {
        return;
    }
    accept();
}

void CalibrationSettingsDialog::applyStyle()
{
    setStyleSheet(QStringLiteral(R"(
        QDialog#calibrationSettingsDialog {
            background: #EDEDED;
            color: #172033;
            font-family: "Microsoft YaHei UI";
            font-size: 13px;
        }
        QFrame#settingsCard {
            background: #FFFFFF;
            border: 1px solid #D2D8E0;
            border-radius: 8px;
        }
        QLabel#settingsTitle {
            color: #1E293B;
            font-size: 16px;
            font-weight: 700;
            padding-bottom: 4px;
        }
        QLabel#settingsCaption {
            color: #555F6D;
            font-family: "Cascadia Mono";
            font-size: 10pt;
        }
        QLabel#settingsValue {
            color: #172033;
            font-family: "Cascadia Mono";
            font-size: 10pt;
            font-weight: 600;
        }
        QLabel#settingsSummary {
            color: #3F4A58;
            background: #F7F9FB;
            border: 1px solid #DCE3EA;
            border-radius: 8px;
            padding: 7px;
            font-family: "Microsoft YaHei UI";
            font-size: 11px;
        }
        QComboBox, QSpinBox, QDoubleSpinBox {
            min-height: 30px;
            color: #1E222B;
            background: #FFFFFF;
            border: 1px solid #CBD5E1;
            border-radius: 8px;
            padding: 1px 9px;
            font-family: "Microsoft YaHei UI";
            font-size: 13px;
        }
        QComboBox:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled {
            color: #7A8797;
            background: #F1F5F9;
        }
        QPushButton {
            min-width: 92px;
            min-height: 36px;
            border-radius: 12px;
            padding: 0 16px;
            font-family: "Microsoft YaHei UI";
            font-size: 14px;
            font-weight: 600;
        }
        QPushButton#primaryButton {
            color: #FFFFFF;
            background: #616161;
            border: 1px solid #616161;
        }
        QPushButton#primaryButton:hover {
            background: #757575;
        }
        QPushButton#primaryButton:pressed {
            background: #424242;
        }
        QPushButton#primaryButton:disabled {
            color: #E5E7EB;
            background: #9E9E9E;
            border-color: #9E9E9E;
        }
        QPushButton#secondaryButton {
            color: #FFFFFF;
            background: #616161;
            border: 1px solid #616161;
        }
        QPushButton#secondaryButton:hover {
            background: #757575;
        }
        QPushButton#secondaryButton:pressed {
            background: #424242;
        }
    )"));
}
