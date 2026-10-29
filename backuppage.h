#ifndef BACKUPPAGE_H
#define BACKUPPAGE_H

#include <QWidget>

class ParameterModel;
class QLabel;

class BackupPage : public QWidget
{
    Q_OBJECT

public:
    explicit BackupPage(ParameterModel *model, QWidget *parent = nullptr);

public slots:
    void setConnected(bool connected);
    void handleParameterReadFinished(bool complete, int received);

signals:
    void readAllRequested();
    void writeDirtyRequested();
    void statusMessage(const QString &message);

private slots:
    void exportBackup();
    void importBackup();

private:
    ParameterModel *m_model = nullptr;
    QLabel *m_status = nullptr;
    bool m_connected = false;
};

#endif // BACKUPPAGE_H
