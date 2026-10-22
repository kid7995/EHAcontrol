#ifndef COMMUNICATIONPAGE_H
#define COMMUNICATIONPAGE_H

#include <QByteArray>
#include <QWidget>

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;

class CommunicationPage : public QWidget
{
    Q_OBJECT

public:
    explicit CommunicationPage(QWidget *parent = nullptr);

    void setConnected(bool connected, const QString &portName);

public slots:
    void logFrame(bool transmitted, const QByteArray &frame);
    void logMessage(const QString &message);

signals:
    void openRequested(const QString &portName, int baudRate, int dataBits,
                       int parity, int stopBits);
    void disconnectRequested();
    void readAllRequested();
    void enableRequested();
    void disableRequested();
    void faultResetRequested();
    void saveRequested();
    void rawFrameRequested(const QByteArray &frame);

private slots:
    void refreshPorts();
    void toggleConnection();
    void sendRawCommand();
    void clearLog();

private:
    QComboBox *m_portCombo = nullptr;
    QComboBox *m_baudCombo = nullptr;
    QComboBox *m_dataBitsCombo = nullptr;
    QComboBox *m_parityCombo = nullptr;
    QComboBox *m_stopBitsCombo = nullptr;
    QPushButton *m_connectButton = nullptr;
    QLabel *m_connectionLabel = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QPlainTextEdit *m_rawInput = nullptr;
    QPushButton *m_readButton = nullptr;
    QPushButton *m_enableButton = nullptr;
    QPushButton *m_disableButton = nullptr;
    QPushButton *m_faultButton = nullptr;
    QPushButton *m_saveButton = nullptr;
    bool m_connected = false;
};

#endif // COMMUNICATIONPAGE_H
