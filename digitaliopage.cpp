#include "digitaliopage.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

DigitalIoPage::DigitalIoPage(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(22, 18, 22, 18);
    root->setSpacing(12);

    auto *headingRow = new QHBoxLayout;
    auto *heading = new QVBoxLayout;
    auto *title = new QLabel(QStringLiteral("数字 I/O"), this);
    title->setObjectName(QStringLiteral("pageHeading"));
    auto *subtitle = new QLabel(
        QStringLiteral("对照数字输入和输出功能、有效电平与连接端子。硬件极性请以整机接线定义为准。"),
        this);
    subtitle->setObjectName(QStringLiteral("pageSubtitle"));
    subtitle->setWordWrap(true);
    heading->addWidget(title);
    heading->addWidget(subtitle);
    headingRow->addLayout(heading, 1);
    auto *readButton = new QPushButton(QStringLiteral("读取 I/O 参数"), this);
    readButton->setObjectName(QStringLiteral("secondaryButton"));
    headingRow->addWidget(readButton);
    root->addLayout(headingRow);

    auto *notice = new QLabel(
        QStringLiteral("参考版 Digital__IO 页将输入、输出分成独立表格。"
                       "部分 I/O 功能由驱动器内部固定，端子号与有效电平由设备配置决定；"
                       "本版本只展示已确认功能，不自动改写极性。"),
        this);
    notice->setObjectName(QStringLiteral("fieldHint"));
    notice->setWordWrap(true);
    root->addWidget(notice);

    auto *tablesRow = new QHBoxLayout;
    tablesRow->setSpacing(12);

    auto *inputPanel = new QFrame(this);
    inputPanel->setObjectName(QStringLiteral("panel"));
    auto *inputLayout = new QVBoxLayout(inputPanel);
    inputLayout->setContentsMargins(14, 12, 14, 12);
    auto *inputTitle = new QLabel(QStringLiteral("数字输入 · DI"), inputPanel);
    inputTitle->setObjectName(QStringLiteral("sectionTitle"));
    inputLayout->addWidget(inputTitle);
    inputLayout->addWidget(createTable({
        {QStringLiteral("DI1"), QStringLiteral("伺服 ON（内部速度模式）"), QStringLiteral("待确认"), QStringLiteral("未读取")},
        {QStringLiteral("DI2"), QStringLiteral("回零命令"), QStringLiteral("待确认"), QStringLiteral("未读取")},
        {QStringLiteral("DI3"), QStringLiteral("机械原点"), QStringLiteral("待确认"), QStringLiteral("未读取")},
        {QStringLiteral("DI4"), QStringLiteral("正转驱动禁止"), QStringLiteral("待确认"), QStringLiteral("未读取")},
        {QStringLiteral("DI5"), QStringLiteral("反转驱动禁止"), QStringLiteral("待确认"), QStringLiteral("未读取")},
        {QStringLiteral("DI6"), QStringLiteral("报警复位"), QStringLiteral("待确认"), QStringLiteral("未读取")},
        {QStringLiteral("DI7"), QStringLiteral("伺服 ON（位置齿轮模式）"), QStringLiteral("待确认"), QStringLiteral("未读取")},
        {QStringLiteral("DI8"), QStringLiteral("保留"), QStringLiteral("--"), QStringLiteral("--")},
    }, inputPanel), 1);
    tablesRow->addWidget(inputPanel, 1);

    auto *outputPanel = new QFrame(this);
    outputPanel->setObjectName(QStringLiteral("panel"));
    auto *outputLayout = new QVBoxLayout(outputPanel);
    outputLayout->setContentsMargins(14, 12, 14, 12);
    auto *outputTitle = new QLabel(QStringLiteral("数字输出 · DO"), outputPanel);
    outputTitle->setObjectName(QStringLiteral("sectionTitle"));
    outputLayout->addWidget(outputTitle);
    outputLayout->addWidget(createTable({
        {QStringLiteral("DO1"), QStringLiteral("回零完成"), QStringLiteral("待确认"), QStringLiteral("未读取")},
        {QStringLiteral("DO2"), QStringLiteral("伺服报警"), QStringLiteral("待确认"), QStringLiteral("未读取")},
        {QStringLiteral("DO3"), QStringLiteral("伺服准备就绪"), QStringLiteral("待确认"), QStringLiteral("未读取")},
        {QStringLiteral("DO4"), QStringLiteral("编码器模拟 Z 脉冲输出"), QStringLiteral("待确认"), QStringLiteral("未读取")},
        {QStringLiteral("DO5"), QStringLiteral("制动器（刹车）控制输出"), QStringLiteral("待确认"), QStringLiteral("未读取")},
        {QStringLiteral("DO6"), QStringLiteral("保留"), QStringLiteral("--"), QStringLiteral("--")},
    }, outputPanel), 1);
    tablesRow->addWidget(outputPanel, 1);
    root->addLayout(tablesRow, 1);

    auto *footer = new QLabel(
        QStringLiteral("参考版写 I/O 配置时使用 Pn013/Pn014（输入）和 Pn015/Pn016（输出）成对命令；"
                       "正式启用前需要结合具体机型确认位定义。"),
        this);
    footer->setObjectName(QStringLiteral("mutedText"));
    footer->setWordWrap(true);
    root->addWidget(footer);

    connect(readButton, &QPushButton::clicked, this, &DigitalIoPage::readAllRequested);
}

QTableWidget *DigitalIoPage::createTable(const QVector<QStringList> &rows, QWidget *parent)
{
    auto *table = new QTableWidget(parent);
    table->setObjectName(QStringLiteral("ioTable"));
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels(
        {QStringLiteral("端子"), QStringLiteral("功能"),
         QStringLiteral("有效电平"), QStringLiteral("状态")});
    table->setRowCount(rows.size());
    for (int row = 0; row < rows.size(); ++row) {
        for (int column = 0; column < 4; ++column) {
            auto *item = new QTableWidgetItem(rows.at(row).value(column));
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            table->setItem(row, column, item);
        }
    }
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->setDefaultSectionSize(34);
    return table;
}
