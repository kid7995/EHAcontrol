#ifndef FAULTPAGE_H
#define FAULTPAGE_H

#include <QVector>
#include <QWidget>

class QLabel;
class QLineEdit;
class QTableWidget;

class FaultPage : public QWidget
{
    Q_OBJECT

public:
    explicit FaultPage(QWidget *parent = nullptr);

public slots:
    void setConnected(bool connected);
    void setMotorEnabled(bool enabled);
    void setFaultCode(quint32 faultCode);

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
};

#endif // FAULTPAGE_H
