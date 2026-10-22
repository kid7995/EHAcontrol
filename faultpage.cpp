#include "faultpage.h"

#include <QFile>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QXmlStreamReader>

FaultPage::FaultPage(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(22, 18, 22, 18);
    root->setSpacing(12);

    auto *headingRow = new QHBoxLayout;
    auto *heading = new QVBoxLayout;
    auto *title = new QLabel(QStringLiteral("使能、报警与故障"), this);
    title->setObjectName(QStringLiteral("pageHeading"));
    auto *subtitle = new QLabel(
        QStringLiteral("查看当前运行状态，检索参考版 ErrorCode 故障说明与处理办法。"),
        this);
    subtitle->setObjectName(QStringLiteral("pageSubtitle"));
    heading->addWidget(title);
    heading->addWidget(subtitle);
    headingRow->addLayout(heading, 1);
    auto *enableButton = new QPushButton(QStringLiteral("使能设备"), this);
    enableButton->setObjectName(QStringLiteral("successButton"));
    auto *disableButton = new QPushButton(QStringLiteral("安全停止"), this);
    disableButton->setObjectName(QStringLiteral("dangerButton"));
    auto *resetButton = new QPushButton(QStringLiteral("报警复位"), this);
    resetButton->setObjectName(QStringLiteral("secondaryButton"));
    headingRow->addWidget(enableButton);
    headingRow->addWidget(disableButton);
    headingRow->addWidget(resetButton);
    root->addLayout(headingRow);

    auto *statusPanel = new QFrame(this);
    statusPanel->setObjectName(QStringLiteral("panel"));
    auto *statusLayout = new QGridLayout(statusPanel);
    statusLayout->setContentsMargins(16, 12, 16, 12);
    statusLayout->setHorizontalSpacing(18);
    statusLayout->setVerticalSpacing(5);
    auto *codeCaption = new QLabel(QStringLiteral("当前故障代码"), statusPanel);
    codeCaption->setObjectName(QStringLiteral("metricTitle"));
    m_faultCode = new QLabel(QStringLiteral("--"), statusPanel);
    m_faultCode->setObjectName(QStringLiteral("infoValue"));
    auto *descriptionCaption = new QLabel(QStringLiteral("当前状态说明"), statusPanel);
    descriptionCaption->setObjectName(QStringLiteral("metricTitle"));
    m_faultDescription = new QLabel(QStringLiteral("等待设备状态帧"), statusPanel);
    m_faultDescription->setObjectName(QStringLiteral("infoValue"));
    auto *motorCaption = new QLabel(QStringLiteral("使能状态"), statusPanel);
    motorCaption->setObjectName(QStringLiteral("metricTitle"));
    m_motorState = new QLabel(QStringLiteral("未知"), statusPanel);
    m_motorState->setObjectName(QStringLiteral("infoValue"));
    statusLayout->addWidget(codeCaption, 0, 0);
    statusLayout->addWidget(m_faultCode, 1, 0);
    statusLayout->addWidget(descriptionCaption, 0, 1);
    statusLayout->addWidget(m_faultDescription, 1, 1);
    statusLayout->addWidget(motorCaption, 0, 2);
    statusLayout->addWidget(m_motorState, 1, 2);
    statusLayout->setColumnStretch(1, 1);
    root->addWidget(statusPanel);

    auto *filterRow = new QHBoxLayout;
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral("搜索报警代码、说明或处理办法…"));
    m_search->setClearButtonEnabled(true);
    m_resultCount = new QLabel(this);
    m_resultCount->setObjectName(QStringLiteral("mutedText"));
    filterRow->addWidget(m_search, 1);
    filterRow->addWidget(m_resultCount);
    root->addLayout(filterRow);

    m_table = new QTableWidget(this);
    m_table->setObjectName(QStringLiteral("faultTable"));
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("代码"), QStringLiteral("说明"), QStringLiteral("处理办法")});
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->setWordWrap(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    root->addWidget(m_table, 1);

    connect(enableButton, &QPushButton::clicked, this, &FaultPage::enableRequested);
    connect(disableButton, &QPushButton::clicked, this, &FaultPage::disableRequested);
    connect(resetButton, &QPushButton::clicked, this, &FaultPage::faultResetRequested);
    connect(m_search, &QLineEdit::textChanged, this, &FaultPage::filterCodes);

    loadCatalog();
    filterCodes(QString());
    setConnected(false);
}

void FaultPage::setConnected(bool connected)
{
    if (!connected) {
        m_motorState->setText(QStringLiteral("离线"));
    }
}

void FaultPage::setMotorEnabled(bool enabled)
{
    m_motorState->setText(enabled ? QStringLiteral("已使能")
                                  : QStringLiteral("未使能"));
}

void FaultPage::setFaultCode(quint32 faultCode)
{
    m_faultCode->setText(faultCode == 0
                             ? QStringLiteral("无故障")
                             : QStringLiteral("0x%1")
                                   .arg(faultCode, 8, 16, QLatin1Char('0')).toUpper());
    if (faultCode == 0) {
        m_faultDescription->setText(QStringLiteral("设备未上报故障"));
    } else {
        m_faultDescription->setText(
            QStringLiteral("设备上报故障码，请根据下方代码表检查接线、参数与现场状态"));
    }
}

void FaultPage::filterCodes(const QString &text)
{
    const QString query = text.trimmed();
    int row = 0;
    for (const ErrorEntry &entry : m_entries) {
        const bool matches = query.isEmpty()
            || entry.code.contains(query, Qt::CaseInsensitive)
            || entry.description.contains(query, Qt::CaseInsensitive)
            || entry.process.contains(query, Qt::CaseInsensitive);
        if (!matches) {
            continue;
        }
        if (row >= m_table->rowCount()) {
            m_table->insertRow(row);
        }
        auto *codeItem = new QTableWidgetItem(entry.code);
        codeItem->setData(Qt::UserRole, entry.code);
        auto *descriptionItem = new QTableWidgetItem(entry.description);
        auto *processItem = new QTableWidgetItem(entry.process);
        m_table->setItem(row, 0, codeItem);
        m_table->setItem(row, 1, descriptionItem);
        m_table->setItem(row, 2, processItem);
        m_table->resizeRowToContents(row);
        ++row;
    }
    m_table->setRowCount(row);
    m_resultCount->setText(QStringLiteral("%1 条记录").arg(row));
}

void FaultPage::loadCatalog()
{
    QFile file(QStringLiteral(":/reference/data/ErrorCode.xml"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    QXmlStreamReader xml(&file);
    ErrorEntry current;
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) {
            continue;
        }
        if (xml.name() == QStringLiteral("id_no")) {
            current.code = xml.readElementText().trimmed();
        } else if (xml.name() == QStringLiteral("description")) {
            current.description = xml.readElementText().trimmed();
        } else if (xml.name() == QStringLiteral("process")) {
            current.process = xml.readElementText().trimmed();
            if (!current.code.isEmpty()) {
                m_entries.append(current);
            }
            current = ErrorEntry();
        }
    }
}
