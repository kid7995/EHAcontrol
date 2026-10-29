#ifndef DIGITALIOPAGE_H
#define DIGITALIOPAGE_H

#include <QStringList>
#include <QVector>
#include <QWidget>

class QTableWidget;
class QSpinBox;
class QPushButton;

class DigitalIoPage : public QWidget
{
    Q_OBJECT

public:
    explicit DigitalIoPage(QWidget *parent = nullptr);
    void setConnected(bool connected);

public slots:
    void setParameterValue(quint16 parameter, quint16 value);

signals:
    void readAllRequested();
    void parameterWriteRequested(quint16 parameter, quint16 value);

private:
    QTableWidget *createTable(const QVector<QStringList> &rows, QWidget *parent);
    void applyConfiguration();

    QSpinBox *m_inputA = nullptr;
    QSpinBox *m_inputB = nullptr;
    QSpinBox *m_outputA = nullptr;
    QSpinBox *m_outputB = nullptr;
    QPushButton *m_writeButton = nullptr;
};

#endif // DIGITALIOPAGE_H
