#ifndef FAULTPAGE_H
#define FAULTPAGE_H

#include <QVector>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTableWidget;
class QTabWidget;

class FaultPage : public QWidget
{
    Q_OBJECT

public:
    explicit FaultPage(QWidget *parent = nullptr);

public slots:
    void setConnected(bool connected);
    void setMotorEnabled(bool enabled);
    void setFaultCode(quint32 faultCode);
    void appendHistory(const QString &message);

signals:
    void enableRequested();
    void disableRequested();
    void faultResetRequested();

private slots:
    void filterCodes(const QString &text);

private:
    struct ErrorEntry {
        QString code;
        QString description;
        QString process;
    };

    void loadCatalog();
    void populateTable();

    QVector<ErrorEntry> m_entries;
    QLineEdit *m_search = nullptr;
    QTableWidget *m_table = nullptr;
    QLabel *m_faultCode = nullptr;
    QLabel *m_faultDescription = nullptr;
    QLabel *m_motorState = nullptr;
    QLabel *m_resultCount = nullptr;
    QTabWidget *m_tabs = nullptr;
    QPlainTextEdit *m_history = nullptr;
    quint32 m_lastFaultCode = 0;
    bool m_hasFaultCode = false;
};

#endif // FAULTPAGE_H
