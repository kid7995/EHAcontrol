#ifndef PARAMETERPAGE_H
#define PARAMETERPAGE_H

#include <QWidget>

class ParameterModel;
class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QTableView;
class QTableWidget;

class ParameterPage : public QWidget
{
    Q_OBJECT

public:
    explicit ParameterPage(ParameterModel *model, QWidget *parent = nullptr);

    void selectGroup(const QString &group);

public slots:
    void setConnected(bool connected);
    void setReadProgress(int received, int total);
    void setReadFinished(bool complete, int received);
    void updateCounts();

signals:
    void readAllRequested();
    void writeDirtyRequested();
    void writeAndSaveRequested();
    void statusMessage(const QString &message);

private slots:
    void chooseExportFile();
    void chooseImportFile();
    void confirmRestoreRemote();
    void setCompactMode(bool compact);

private:
    void refreshCompactGrid();
    void compactItemChanged(class QTableWidgetItem *item);

    ParameterModel *m_model = nullptr;
    QLineEdit *m_search = nullptr;
    QComboBox *m_groupCombo = nullptr;
    QTableView *m_table = nullptr;
    QTableWidget *m_compactGrid = nullptr;
    QStackedWidget *m_views = nullptr;
    QPushButton *m_compactButton = nullptr;
    QPushButton *m_detailButton = nullptr;
    QLabel *m_countLabel = nullptr;
    QLabel *m_dirtyLabel = nullptr;
    QProgressBar *m_progress = nullptr;
    QPushButton *m_readButton = nullptr;
    QPushButton *m_writeButton = nullptr;
    QPushButton *m_writeSaveButton = nullptr;
    bool m_refreshingCompactGrid = false;
};

#endif // PARAMETERPAGE_H
