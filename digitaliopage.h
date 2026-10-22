#ifndef DIGITALIOPAGE_H
#define DIGITALIOPAGE_H

#include <QStringList>
#include <QVector>
#include <QWidget>

class QTableWidget;

class DigitalIoPage : public QWidget
{
    Q_OBJECT

public:
    explicit DigitalIoPage(QWidget *parent = nullptr);

signals:
    void readAllRequested();

private:
    QTableWidget *createTable(const QVector<QStringList> &rows, QWidget *parent);
};

#endif // DIGITALIOPAGE_H
