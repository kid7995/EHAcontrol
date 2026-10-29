#include "parameterpage.h"

#include "parametermodel.h"

#include <QBrush>
#include <QColor>
#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {

constexpr int RowsPerParameterColumn = 37;
constexpr int ParameterColumnGroups = 6;

} // namespace

ParameterPage::ParameterPage(ParameterModel *model, QWidget *parent)
    : QWidget(parent)
    , m_model(model)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(14, 12, 14, 12);
    root->setSpacing(8);

    auto *toolbar = new QHBoxLayout;
    toolbar->setSpacing(8);
    m_readButton = new QPushButton(QStringLiteral("读取参数"), this);
    m_readButton->setObjectName(QStringLiteral("primaryButton"));
    m_exportButton = new QPushButton(QStringLiteral("参数导出"), this);
    m_exportButton->setObjectName(QStringLiteral("secondaryButton"));
    m_importButton = new QPushButton(QStringLiteral("参数导入"), this);
    m_importButton->setObjectName(QStringLiteral("secondaryButton"));
    toolbar->addWidget(m_readButton);
    toolbar->addWidget(m_exportButton);
    toolbar->addWidget(m_importButton);
    toolbar->addStretch();
    root->addLayout(toolbar);

    m_grid = new QTableWidget(this);
    m_grid->setObjectName(QStringLiteral("compactParameterTable"));
    m_grid->setColumnCount(12);
    m_grid->setAlternatingRowColors(true);
    m_grid->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_grid->setSelectionMode(QAbstractItemView::SingleSelection);
    m_grid->setWordWrap(false);
    m_grid->setTextElideMode(Qt::ElideNone);
    m_grid->verticalHeader()->setVisible(false);
    m_grid->verticalHeader()->setDefaultSectionSize(32);
    m_grid->horizontalHeader()->setVisible(false);
    for (int pair = 0; pair < 6; ++pair) {
        const int parameterColumn = pair * 2;
        const int valueColumn = parameterColumn + 1;
        m_grid->horizontalHeader()->setSectionResizeMode(parameterColumn,
                                                         QHeaderView::Fixed);
        m_grid->horizontalHeader()->setSectionResizeMode(valueColumn,
                                                         QHeaderView::Stretch);
        m_grid->setColumnWidth(parameterColumn, 62);
    }
    root->addWidget(m_grid, 1);

    connect(m_readButton, &QPushButton::clicked,
            this, &ParameterPage::readAllRequested);
    connect(m_exportButton, &QPushButton::clicked,
            this, &ParameterPage::chooseExportFile);
    connect(m_importButton, &QPushButton::clicked,
            this, &ParameterPage::chooseImportFile);
    connect(m_model, &ParameterModel::parameterEdited,
            this, &ParameterPage::parameterWriteRequested);
    connect(m_grid, &QTableWidget::itemChanged,
            this, &ParameterPage::gridItemChanged);
    connect(m_model, &ParameterModel::modelReset,
            this, &ParameterPage::scheduleGridRefresh);
    connect(m_model, &ParameterModel::dataChanged,
            this, &ParameterPage::scheduleGridRefresh);

    setConnected(false);
    refreshGrid();
}

void ParameterPage::updateEditingState()
{
    const bool editable = m_connected && !m_reading;
    m_grid->setEditTriggers(editable
                                ? QAbstractItemView::AllEditTriggers
                                : QAbstractItemView::NoEditTriggers);
}

void ParameterPage::setConnected(bool connected)
{
    m_connected = connected;
    m_readButton->setEnabled(connected && !m_reading);
    updateEditingState();
}

void ParameterPage::setReadProgress(int received, int total)
{
    m_reading = true;
    m_readButton->setEnabled(false);
    m_readButton->setText(QStringLiteral("读取中 %1/%2")
                              .arg(received)
                              .arg(qMax(1, total)));
    updateEditingState();
}

void ParameterPage::setReadFinished(bool complete, int received)
{
    m_reading = false;
    m_readButton->setText(QStringLiteral("读取参数"));
    m_readButton->setEnabled(m_connected);
    updateEditingState();
    emit statusMessage(complete
                           ? QStringLiteral("参数读取完成，共 %1 项").arg(received)
                           : QStringLiteral("参数读取未完成，共收到 %1 项").arg(received));
}

void ParameterPage::chooseExportFile()
{
    const QString fileName = QFileDialog::getSaveFileName(
        this, QStringLiteral("参数导出"),
        QStringLiteral("参数备份.txt"),
        QStringLiteral("参数文本 (*.txt)"));
    if (fileName.isEmpty()) {
        return;
    }

    QString error;
    if (!m_model->exportDriveText(fileName, &error)) {
        QMessageBox::warning(this, QStringLiteral("参数导出失败"), error);
        return;
    }
    emit statusMessage(QStringLiteral("参数已导出：%1").arg(fileName));
}

void ParameterPage::chooseImportFile()
{
    const QString fileName = QFileDialog::getOpenFileName(
        this, QStringLiteral("参数导入"), QString(),
        QStringLiteral("参数文本 (*.txt);;所有文件 (*.*)"));
    if (fileName.isEmpty()) {
        return;
    }

    QString error;
    if (!m_model->importDriveText(fileName, &error)) {
        QMessageBox::warning(this, QStringLiteral("参数导入失败"), error);
        return;
    }
    emit statusMessage(QStringLiteral("参数已导入：%1").arg(fileName));
}

void ParameterPage::scheduleGridRefresh()
{
    if (m_refreshPending) {
        return;
    }
    m_refreshPending = true;
    QTimer::singleShot(0, this, [this]() {
        m_refreshPending = false;
        refreshGrid();
    });
}

void ParameterPage::refreshGrid()
{
    if (!m_model || m_refreshingGrid) {
        return;
    }

    m_refreshingGrid = true;
    m_grid->blockSignals(true);
    const int parameterCount = m_model->rowCount();
    m_grid->setRowCount(RowsPerParameterColumn);

    for (int row = 0; row < RowsPerParameterColumn; ++row) {
        for (int pair = 0; pair < ParameterColumnGroups; ++pair) {
            const int modelRow = pair * RowsPerParameterColumn + row;
            const int parameterColumn = pair * 2;
            const int valueColumn = parameterColumn + 1;
            if (modelRow >= parameterCount) {
                auto *emptyParameter = new QTableWidgetItem;
                emptyParameter->setFlags(Qt::NoItemFlags);
                auto *emptyValue = new QTableWidgetItem;
                emptyValue->setFlags(Qt::NoItemFlags);
                m_grid->setItem(row, parameterColumn, emptyParameter);
                m_grid->setItem(row, valueColumn, emptyValue);
                continue;
            }

            const QModelIndex parameterIndex =
                m_model->index(modelRow, ParameterModel::ParameterColumn);
            const QModelIndex valueIndex =
                m_model->index(modelRow, ParameterModel::ValueColumn);
            auto *parameterItem =
                new QTableWidgetItem(m_model->data(parameterIndex).toString());
            parameterItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            parameterItem->setTextAlignment(Qt::AlignCenter);
            parameterItem->setBackground(QBrush(QColor(QStringLiteral("#F1F5F9"))));
            parameterItem->setForeground(QBrush(QColor(QStringLiteral("#334155"))));
            QFont parameterFont = parameterItem->font();
            parameterFont.setBold(true);
            parameterItem->setFont(parameterFont);
            parameterItem->setToolTip(
                m_model->parameterName(static_cast<quint16>(modelRow)));

            auto *valueItem =
                new QTableWidgetItem(m_model->data(valueIndex).toString());
            valueItem->setData(Qt::UserRole, modelRow);
            valueItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable
                                | Qt::ItemIsEditable);
            valueItem->setTextAlignment(Qt::AlignCenter);
            valueItem->setToolTip(QStringLiteral("可编辑参数值，输入后按回车写入"));
            m_grid->setItem(row, parameterColumn, parameterItem);
            m_grid->setItem(row, valueColumn, valueItem);
        }
    }

    m_grid->blockSignals(false);
    m_refreshingGrid = false;
}

void ParameterPage::gridItemChanged(QTableWidgetItem *item)
{
    if (m_refreshingGrid || !item || item->column() % 2 == 0) {
        return;
    }

    const int modelRow = item->data(Qt::UserRole).toInt();
    const QModelIndex valueIndex =
        m_model->index(modelRow, ParameterModel::ValueColumn);
    if (!m_model->setData(valueIndex, item->text(), Qt::EditRole)) {
        emit statusMessage(
            QStringLiteral("Pn%1 参数值无效，未写入设备")
                .arg(modelRow, 3, 10, QLatin1Char('0')));
        scheduleGridRefresh();
    }
}
