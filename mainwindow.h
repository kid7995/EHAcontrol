#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "deviceserialservice.h"

#include <QMainWindow>

class CommunicationPage;
class DashboardPage;
class DriveInfoPage;
class MonitorPage;
class ParameterModel;
class ParameterPage;
class QButtonGroup;
class QComboBox;
class QLabel;
class QPushButton;
class QStackedWidget;

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
    void writeAndSaveParameters();
    void requestParameterSave();
    void showParametersForGroup(const QString &group);
    void showPage(int index);
    void handleParameterReadFinished(bool complete, int received);
    void handleSerialError(const QString &message);
    void showCommunicationPage();

private:
    bool confirmReadWhileDirty();
    bool confirmWrite(const QMap<quint16, quint16> &values);
    void createInterface();
    QPushButton *createNavButton(const QString &text, int pageIndex,
                                 const QString &group = QString(),
                                 const QString &viewTitle = QString());
    void applyStyle();
    void setCurrentPageTitle(const QString &title);
    void updateDirtyStatus(int count);

    DeviceSerialService *m_serial = nullptr;
    ParameterModel *m_parameterModel = nullptr;
    DashboardPage *m_dashboard = nullptr;
    ParameterPage *m_parameterPage = nullptr;
    MonitorPage *m_monitorPage = nullptr;
    CommunicationPage *m_communicationPage = nullptr;
    DriveInfoPage *m_driveInfoPage = nullptr;
    QStackedWidget *m_stack = nullptr;
    QLabel *m_pageTitle = nullptr;
    QLabel *m_topConnectionLabel = nullptr;
    QLabel *m_topEnableLabel = nullptr;
    QLabel *m_topFaultLabel = nullptr;
    QLabel *m_topModeLabel = nullptr;
    QComboBox *m_topMonitorCombo = nullptr;
    QLabel *m_livePosition = nullptr;
    QLabel *m_liveMultiTurn = nullptr;
    QLabel *m_liveSingleTurn = nullptr;
    QLabel *m_liveSpeed = nullptr;
    QLabel *m_liveCurrent = nullptr;
    QLabel *m_liveVoltage = nullptr;
    QLabel *m_liveForce = nullptr;
    QLabel *m_dirtyStatusLabel = nullptr;
    QButtonGroup *m_navGroup = nullptr;
    bool m_connected = false;
    bool m_writingAndSaving = false;
};

#endif // MAINWINDOW_H
