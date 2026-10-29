#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "deviceserialservice.h"

#include <QMainWindow>

class CommunicationPage;
class BackupPage;
class AboutPage;
class DashboardPage;
class DeviceConfigPage;
class DigitalIoPage;
class DriveInfoPage;
class FaultPage;
class FeedbackPage;
class HomingPage;
class MonitorPage;
class ParameterModel;
class ParameterPage;
class PreferencesPage;
class QButtonGroup;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTimer;
class ResetPage;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void openSerial(const QString &portName, int baudRate, int dataBits,
                    int parity, int stopBits);
    void closeSerial();
    void setConnectionState(bool connected, const QString &portName);
    void handleTelemetry(const DeviceSerialService::Telemetry &telemetry);
    void readAllParameters();
    void writeDirtyParameters();
    void requestParameterSave();
    void showPage(int index);
    void handleParameterReadFinished(bool complete, int received);
    void handleSerialError(const QString &message);
    void showCommunicationPage();

private:
    bool confirmReadWhileDirty();
    bool confirmWrite(const QMap<quint16, quint16> &values);
    void createInterface();
    QPushButton *createNavButton(const QString &text, int pageIndex);
    void applyStyle();
    void setCurrentPageTitle(const QString &title);
    void updateDirtyStatus(int count);
    void flushTelemetryUi();
    void applyTelemetryToUi(const DeviceSerialService::Telemetry &telemetry);
    void updateMonitorMetric(const DeviceSerialService::Telemetry &telemetry);

    DeviceSerialService *m_serial = nullptr;
    ParameterModel *m_parameterModel = nullptr;
    DashboardPage *m_dashboard = nullptr;
    ParameterPage *m_parameterPage = nullptr;
    MonitorPage *m_monitorPage = nullptr;
    CommunicationPage *m_communicationPage = nullptr;
    DriveInfoPage *m_driveInfoPage = nullptr;
    DeviceConfigPage *m_motorPage = nullptr;
    DeviceConfigPage *m_brakePage = nullptr;
    DeviceConfigPage *m_limitsPage = nullptr;
    DeviceConfigPage *m_resistorPage = nullptr;
    HomingPage *m_homingPage = nullptr;
    FaultPage *m_faultPage = nullptr;
    DigitalIoPage *m_digitalIoPage = nullptr;
    FeedbackPage *m_feedbackPage = nullptr;
    DeviceConfigPage *m_currentLoopPage = nullptr;
    DeviceConfigPage *m_velocityLoopPage = nullptr;
    DeviceConfigPage *m_positionLoopPage = nullptr;
    DeviceConfigPage *m_motionModePage = nullptr;
    BackupPage *m_backupPage = nullptr;
    ResetPage *m_resetPage = nullptr;
    PreferencesPage *m_preferencesPage = nullptr;
    AboutPage *m_aboutPage = nullptr;
    QStackedWidget *m_stack = nullptr;
    QLabel *m_topConnectionLabel = nullptr;
    QLabel *m_topEnableLabel = nullptr;
    QLabel *m_topFaultLabel = nullptr;
    QLabel *m_topModeLabel = nullptr;
    QLabel *m_livePosition = nullptr;
    QLabel *m_liveMultiTurn = nullptr;
    QLabel *m_liveSingleTurn = nullptr;
    QLabel *m_liveSpeed = nullptr;
    QLabel *m_liveCurrent = nullptr;
    QLabel *m_liveVoltage = nullptr;
    QLabel *m_liveMonitorValue = nullptr;
    QLabel *m_liveMonitorUnit = nullptr;
    QLabel *m_dirtyStatusLabel = nullptr;
    QButtonGroup *m_navGroup = nullptr;
    QTimer *m_telemetryUiTimer = nullptr;
    DeviceSerialService::Telemetry m_pendingUiTelemetry;
    bool m_hasPendingUiTelemetry = false;
    bool m_connected = false;
    bool m_writingAndSaving = false;
};

#endif // MAINWINDOW_H
