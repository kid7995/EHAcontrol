#include "parameterpage.h"

#include "parametermodel.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableView>
#include <QTableWidget>
#include <QVBoxLayout>

ParameterPage::ParameterPage(ParameterModel *model, QWidget *parent)
    : QWidget(parent)
    , m_model(model)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(22, 18, 22, 18);
    root->setSpacing(10);

    auto *headingRow = new QHBoxLayout;
    auto *titleBlock = new QVBoxLayout;
    auto *title = new QLabel(QStringLiteral("Pn 参数工作台"), this);
    title->setObjectName(QStringLiteral("pageHeading"));
    auto *subtitle = new QLabel(
        QStringLiteral("读取设备全部参数、批量编辑并写回驱动器"), this);
    subtitle->setObjectName(QStringLiteral("pageSubtitle"));
    titleBlock->addWidget(title);
    titleBlock->addWidget(subtitle);
    headingRow->addLayout(titleBlock);
    headingRow->addStretch();

    m_readButton = new QPushButton(QStringLiteral("读取全部参数"), this);
    m_readButton->setObjectName(QStringLiteral("primaryButton"));
    m_writeButton = new QPushButton(QStringLiteral("写入修改项"), this);
    m_writeButton->setObjectName(QStringLiteral("secondaryButton"));
    m_writeSaveButton = new QPushButton(QStringLiteral("写入并保存"), this);
    m_writeSaveButton->setObjectName(QStringLiteral("successButton"));
    headingRow->addWidget(m_readButton);
    headingRow->addWidget(m_writeButton);
    headingRow->addWidget(m_writeSaveButton);
    root->addLayout(headingRow);

    auto *filterRow = new QHBoxLayout;
    filterRow->setSpacing(10);
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral("搜索 Pn 编号、名称或分组…"));
    m_search->setClearButtonEnabled(true);
    m_groupCombo = new QComboBox(this);
    m_groupCombo->addItems(ParameterCatalog::groups());
    m_groupCombo->setMinimumWidth(150);
    filterRow->addWidget(m_search, 1);
    filterRow->addWidget(m_groupCombo);

    auto *viewGroup = new QButtonGroup(this);
    viewGroup->setExclusive(true);
    m_compactButton = new QPushButton(QStringLiteral("紧凑网格"), this);
    m_compactButton->setObjectName(QStringLiteral("modeButton"));
    m_compactButton->setCheckable(true);
    m_compactButton->setChecked(true);
    m_detailButton = new QPushButton(QStringLiteral("详细表格"), this);
    m_detailButton->setObjectName(QStringLiteral("modeButton"));
    m_detailButton->setCheckable(true);
    viewGroup->addButton(m_compactButton);
    viewGroup->addButton(m_detailButton);
    filterRow->addWidget(m_compactButton);
    filterRow->addWidget(m_detailButton);

    auto *exportButton = new QPushButton(QStringLiteral("导出 CSV"), this);
    exportButton->setObjectName(QStringLiteral("ghostButton"));
    auto *importButton = new QPushButton(QStringLiteral("导入 CSV"), this);
    importButton->setObjectName(QStringLiteral("ghostButton"));
    auto *restoreButton = new QPushButton(QStringLiteral("撤销未写入修改"), this);
    restoreButton->setObjectName(QStringLiteral("ghostButton"));
    filterRow->addWidget(importButton);
    filterRow->addWidget(exportButton);
    filterRow->addWidget(restoreButton);
    root->addLayout(filterRow);

    m_views = new QStackedWidget(this);
    m_views->setObjectName(QStringLiteral("parameterViews"));

    m_compactGrid = new QTableWidget(this);
    m_compactGrid->setObjectName(QStringLiteral("compactParameterTable"));
    m_compactGrid->setColumnCount(12);
    QStringList headers;
    for (int pair = 0; pair < 6; ++pair) {
        headers << QStringLiteral("Pn") << QStringLiteral("值");
    }
    m_compactGrid->setHorizontalHeaderLabels(headers);
    m_compactGrid->verticalHeader()->setVisible(false);
    m_compactGrid->verticalHeader()->setDefaultSectionSize(30);
    m_compactGrid->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_compactGrid->setAlternatingRowColors(true);
    m_compactGrid->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_compactGrid->setSelectionMode(QAbstractItemView::SingleSelection);
    m_views->addWidget(m_compactGrid);

    m_table = new QTableView(this);
    m_table->setObjectName(QStringLiteral("parameterTable"));
    m_table->setModel(m_model);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(34);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(ParameterModel::ParameterColumn,
                                                      QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ParameterModel::NameColumn,
                                                      QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(ParameterModel::ValueColumn,
                                                      QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(ParameterModel::UnitColumn,
                                                      QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ParameterModel::GroupColumn,
                                                      QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ParameterModel::StateColumn,
                                                      QHeaderView::ResizeToContents);
    m_table->setColumnWidth(ParameterModel::ValueColumn, 130);
    m_views->addWidget(m_table);
    root->addWidget(m_views, 1);

    auto *footerRow = new QHBoxLayout;
    m_countLabel = new QLabel(this);
    m_countLabel->setObjectName(QStringLiteral("mutedText"));
    m_dirtyLabel = new QLabel(this);
    m_dirtyLabel->setObjectName(QStringLiteral("warningText"));
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 200);
    m_progress->setValue(0);
    m_progress->setTextVisible(true);
    m_progress->setMaximumWidth(280);
    footerRow->addWidget(m_countLabel);
    footerRow->addWidget(m_dirtyLabel);
    footerRow->addStretch();
    footerRow->addWidget(m_progress);
    root->addLayout(footerRow);

    connect(m_search, &QLineEdit::textChanged,
            m_model, &ParameterModel::setTextFilter);
    connect(m_groupCombo, &QComboBox::currentTextChanged,
            m_model, &ParameterModel::setGroupFilter);
    connect(m_model, &ParameterModel::dirtyStateChanged,
            this, &ParameterPage::updateCounts);
    connect(m_readButton, &QPushButton::clicked,
            this, &ParameterPage::readAllRequested);
    connect(m_writeButton, &QPushButton::clicked,
            this, &ParameterPage::writeDirtyRequested);
    connect(m_writeSaveButton, &QPushButton::clicked,
            this, &ParameterPage::writeAndSaveRequested);
    connect(exportButton, &QPushButton::clicked,
            this, &ParameterPage::chooseExportFile);
    connect(importButton, &QPushButton::clicked,
            this, &ParameterPage::chooseImportFile);
    connect(restoreButton, &QPushButton::clicked,
            this, &ParameterPage::confirmRestoreRemote);
    connect(m_compactButton, &QPushButton::clicked,
            this, [this]() { setCompactMode(true); });
    connect(m_detailButton, &QPushButton::clicked,
            this, [this]() { setCompactMode(false); });
    connect(m_model, &ParameterModel::modelReset,
            this, &ParameterPage::refreshCompactGrid);
    connect(m_model, &ParameterModel::dataChanged,
            this, [this]() { refreshCompactGrid(); });
    connect(m_compactGrid, &QTableWidget::itemChanged,
            this, &ParameterPage::compactItemChanged);

    setConnected(false);
    setCompactMode(true);
    refreshCompactGrid();
    updateCounts();
}

void ParameterPage::selectGroup(const QString &group)
{
    const int index = m_groupCombo->findText(group);
    if (index >= 0) {
        m_groupCombo->setCurrentIndex(index);
    }
}

void ParameterPage::setCompactMode(bool compact)
{
    m_compactButton->setChecked(compact);
    m_detailButton->setChecked(!compact);
    m_views->setCurrentWidget(compact ? static_cast<QWidget *>(m_compactGrid)
                                      : static_cast<QWidget *>(m_table));
    if (compact) {
        refreshCompactGrid();
    }
}

void ParameterPage::refreshCompactGrid()
{
    if (!m_compactGrid || !m_model || m_refreshingCompactGrid) {
        return;
    }

    m_refreshingCompactGrid = true;
    m_compactGrid->blockSignals(true);
    const int parameterCount = m_model->rowCount();
    const int rowCount = (parameterCount + 5) / 6;
    m_compactGrid->setRowCount(rowCount);

    for (int row = 0; row < rowCount; ++row) {
        for (int pair = 0; pair < 6; ++pair) {
            const int modelRow = row * 6 + pair;
            const int pnColumn = pair * 2;
            const int valueColumn = pnColumn + 1;
            if (modelRow >= parameterCount) {
                auto *emptyPn = new QTableWidgetItem(QString());
                emptyPn->setFlags(Qt::NoItemFlags);
                auto *emptyValue = new QTableWidgetItem(QString());
                emptyValue->setFlags(Qt::NoItemFlags);
                m_compactGrid->setItem(row, pnColumn, emptyPn);
                m_compactGrid->setItem(row, valueColumn, emptyValue);
                continue;
            }

            const QModelIndex pnIndex =
                m_model->index(modelRow, ParameterModel::ParameterColumn);
            const QModelIndex valueIndex =
                m_model->index(modelRow, ParameterModel::ValueColumn);
            auto *pnItem = new QTableWidgetItem(m_model->data(pnIndex).toString());
            pnItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            pnItem->setToolTip(m_model->data(
                m_model->index(modelRow, ParameterModel::NameColumn)).toString());
            pnItem->setBackground(m_model->data(pnIndex, Qt::BackgroundRole).value<QBrush>());

            auto *valueItem = new QTableWidgetItem(m_model->data(valueIndex).toString());
            valueItem->setData(Qt::UserRole, modelRow);
            valueItem->setTextAlignment(Qt::AlignCenter);
            valueItem->setToolTip(QStringLiteral("双击编辑，修改后点击“写入修改项”"));
            valueItem->setBackground(m_model->data(valueIndex, Qt::BackgroundRole).value<QBrush>());
            m_compactGrid->setItem(row, pnColumn, pnItem);
            m_compactGrid->setItem(row, valueColumn, valueItem);
        }
    }

    m_compactGrid->blockSignals(false);
    m_refreshingCompactGrid = false;
}

void ParameterPage::compactItemChanged(QTableWidgetItem *item)
{
    if (m_refreshingCompactGrid || !item || item->column() % 2 == 0) {
        return;
    }
    const int modelRow = item->data(Qt::UserRole).toInt();
    const QModelIndex valueIndex = m_model->index(modelRow, ParameterModel::ValueColumn);
    if (!m_model->setData(valueIndex, item->text(), Qt::EditRole)) {
        refreshCompactGrid();
    }
}

void ParameterPage::setConnected(bool connected)
{
    m_readButton->setEnabled(connected);
    m_writeButton->setEnabled(connected);
    m_writeSaveButton->setEnabled(connected);
}

void ParameterPage::setReadProgress(int received, int total)
{
    m_progress->setRange(0, qMax(1, total));
    m_progress->setValue(received);
    m_progress->setFormat(QStringLiteral("读取 %v / %m"));
}

void ParameterPage::setReadFinished(bool complete, int received)
{
    m_progress->setFormat(complete
                              ? QStringLiteral("完成 · %1 项").arg(received)
                              : QStringLiteral("超时 · %1 项").arg(received));
    if (complete) {
        m_progress->setValue(m_progress->maximum());
    }
}

void ParameterPage::updateCounts()
{
    m_countLabel->setText(QStringLiteral("已读取 %1 / 200 项")
                              .arg(m_model->confirmedCount()));
    const int dirty = m_model->dirtyCount();
    m_dirtyLabel->setText(dirty > 0
                              ? QStringLiteral("%1 个参数待写入").arg(dirty)
                              : QString());
    m_writeButton->setText(dirty > 0
                               ? QStringLiteral("写入修改项 (%1)").arg(dirty)
                               : QStringLiteral("写入修改项"));
}

void ParameterPage::chooseExportFile()
{
    const QString fileName = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出参数备份"),
        QStringLiteral("KaiserStudio_Parameters.csv"),
        QStringLiteral("CSV 文件 (*.csv)"));
    if (fileName.isEmpty()) {
        return;
    }
    QString error;
    if (!m_model->exportCsv(fileName, &error)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), error);
        return;
    }
    emit statusMessage(QStringLiteral("参数已导出：%1").arg(fileName));
}

void ParameterPage::chooseImportFile()
{
    const QString fileName = QFileDialog::getOpenFileName(
        this, QStringLiteral("导入参数备份"), QString(),
        QStringLiteral("CSV 文件 (*.csv);;所有文件 (*.*)"));
    if (fileName.isEmpty()) {
        return;
    }
    QString error;
    if (!m_model->importCsv(fileName, &error)) {
        QMessageBox::warning(this, QStringLiteral("导入失败"), error);
        return;
    }
    updateCounts();
    emit statusMessage(QStringLiteral("参数已导入，请检查修改项后写入设备"));
}

void ParameterPage::confirmRestoreRemote()
{
    if (m_model->dirtyCount() == 0) {
        return;
    }
    if (QMessageBox::question(
            this, QStringLiteral("撤销修改"),
            QStringLiteral("撤销所有尚未写入设备的参数修改？"))
        == QMessageBox::Yes) {
        m_model->restoreRemoteValues();
        emit statusMessage(QStringLiteral("已撤销未写入修改"));
    }
}
