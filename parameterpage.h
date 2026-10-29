#ifndef PARAMETERPAGE_H
#define PARAMETERPAGE_H

#include <QWidget>

class ParameterModel;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;

class ParameterPage : public QWidget
{
    Q_OBJECT

public:
    explicit ParameterPage(ParameterModel *model, QWidget *parent = nullptr);

public slots:
    void setConnected(bool connected);
    void setReadProgress(int received, int total);
    void setReadFinished(bool complete, int received);

signals:
    void readAllRequested();
    void parameterWriteRequested(quint16 parameter, quint16 rawValue);
    void statusMessage(const QString &message);

private slots:
    void chooseExportFile();
    void chooseImportFile();
    void gridItemChanged(QTableWidgetItem *item);

private:
    void updateEditingState();
    void scheduleGridRefresh();
    void refreshGrid();

    ParameterModel *m_model = nullptr;
    QTableWidget *m_grid = nullptr;
    QPushButton *m_readButton = nullptr;
    QPushButton *m_exportButton = nullptr;
    QPushButton *m_importButton = nullptr;
    bool m_connected = false;
    bool m_reading = false;
    bool m_refreshingGrid = false;
    bool m_refreshPending = false;
};

#endif // PARAMETERPAGE_H
