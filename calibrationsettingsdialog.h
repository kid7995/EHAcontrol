#ifndef CALIBRATIONSETTINGSDIALOG_H
#define CALIBRATIONSETTINGSDIALOG_H

#include "calibrationprofile.h"

#include <QDialog>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;

class CalibrationSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CalibrationSettingsDialog(const CalibrationProfile &profile,
                                       QWidget *parent = nullptr);

    CalibrationProfile calibrationProfile() const;

private slots:
    void modelChanged();
    void rangeModeChanged();
    void forceRangeChanged();
    void updateSummary();

private:
    void applyStyle();
    void applyPreset(const CalibrationProfile &profile);
    void acceptSettings();

    QComboBox *m_modelCombo = nullptr;
    QComboBox *m_rangeModeCombo = nullptr;
    QSpinBox *m_maxForceSpin = nullptr;
    QSpinBox *m_stepSpin = nullptr;
    QDoubleSpinBox *m_toleranceSpin = nullptr;
    QLabel *m_algorithmValue = nullptr;
    QLabel *m_toleranceValue = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QPushButton *m_applyButton = nullptr;
    CalibrationProfile m_profile;
};

#endif // CALIBRATIONSETTINGSDIALOG_H
