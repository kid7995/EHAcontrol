#ifndef RESETPAGE_H
#define RESETPAGE_H

#include <QWidget>

class QLabel;

class ResetPage : public QWidget
{
    Q_OBJECT

public:
    explicit ResetPage(QWidget *parent = nullptr);

public slots:
    void setConnected(bool connected);

signals:
    void resetRequested();

private:
    QLabel *m_status = nullptr;
};

#endif // RESETPAGE_H
