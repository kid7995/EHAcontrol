#ifndef FEEDBACKPAGE_H
#define FEEDBACKPAGE_H

#include "deviceserialservice.h"

#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

class FeedbackPage : public QWidget
{
    Q_OBJECT

public:
    explicit FeedbackPage(QWidget *parent = nullptr);

public slots:
    void setConnected(bool connected);
    void setMotorEnabled(bool enabled);
    void setParameterValue(quint16 parameter, quint16 value);
    void updateTelemetry(const DeviceSerialService::Telemetry &telemetry);

signals:
    void readAllRequested();
    void parameterWriteRequested(quint16 parameter, quint16 value);
    void rawFrameRequested(const QByteArray &frame);
    void statusMessage(const QString &message);

private slots:
    void applyEncoderType();
    void applyOutputType();
    void applyPoleDetectionSettings();
    void togglePoleDetectionSafety(bool enabled);
    void startPoleDetection();
    void clearAbsolutePosition();
    void updateEncoderDescription();

private:
    struct EncoderOption {
        QString label;
        quint16 singleBits = 0;
        quint16 multiBits = 0;
        int protocolGroup = -1;
    };

    void appendEncoderOptions();
    void updateValueLabels();

    QComboBox *m_encoderCombo = nullptr;
    QLabel *m_encoderDescription = nullptr;
    QLabel *m_positionValue = nullptr;
    QLabel *m_multiTurnValue = nullptr;
    QLabel *m_singleTurnValue = nullptr;
    QLabel *m_encoderStatus = nullptr;
    QLabel *m_absoluteStatus = nullptr;
    QCheckBox *m_poleDetectionSafety = nullptr;
    QLabel *m_poleDetectionStatus = nullptr;
    QDoubleSpinBox *m_detectionTime = nullptr;
    QDoubleSpinBox *m_detectionFrequency = nullptr;
    QDoubleSpinBox *m_detectionVoltage = nullptr;
    QComboBox *m_outputType = nullptr;
    QDoubleSpinBox *m_pulseHigh = nullptr;
    QDoubleSpinBox *m_pulseLow = nullptr;
    QPushButton *m_clearAbsoluteButton = nullptr;
    QPushButton *m_startDetectionButton = nullptr;
    bool m_connected = false;
    bool m_motorEnabled = false;
    bool m_poleDetectionArmed = false;
};

#endif // FEEDBACKPAGE_H
