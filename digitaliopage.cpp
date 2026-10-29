#include "digitaliopage.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
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

    auto *configurationPanel = new QFrame(this);
    configurationPanel->setObjectName(QStringLiteral("panel"));
    auto *configurationLayout = new QHBoxLayout(configurationPanel);
    configurationLayout->setContentsMargins(16, 12, 16, 12);
    configurationLayout->setSpacing(12);
    auto *configurationTitle = new QLabel(QStringLiteral("I/O 配置原始值"), configurationPanel);
    configurationTitle->setObjectName(QStringLiteral("sectionTitle"));
    configurationLayout->addWidget(configurationTitle);
    const auto addConfigField = [configurationPanel, configurationLayout](
                                    const QString &label, QSpinBox *&editor) {
        auto *caption = new QLabel(label, configurationPanel);
        caption->setObjectName(QStringLiteral("infoCaption"));
        editor = new QSpinBox(configurationPanel);
        editor->setRange(0, 65535);
        editor->setMinimumWidth(115);
        configurationLayout->addWidget(caption);
        configurationLayout->addWidget(editor);
    };
    addConfigField(QStringLiteral("Pn013"), m_inputA);
    addConfigField(QStringLiteral("Pn014"), m_inputB);
    addConfigField(QStringLiteral("Pn015"), m_outputA);
    addConfigField(QStringLiteral("Pn016"), m_outputB);
    m_writeButton = new QPushButton(QStringLiteral("写入 I/O 配置"), configurationPanel);
    m_writeButton->setObjectName(QStringLiteral("primaryButton"));
    configurationLayout->addWidget(m_writeButton);
    root->addWidget(configurationPanel);

    auto *footer = new QLabel(
        QStringLiteral("参考版写 I/O 配置时使用 Pn013/Pn014（输入）和 Pn015/Pn016（输出）成对命令；"
                       "正式启用前需要结合具体机型确认位定义。"),
        this);
    footer->setObjectName(QStringLiteral("mutedText"));
    footer->setWordWrap(true);
    root->addWidget(footer);

    connect(readButton, &QPushButton::clicked, this, &DigitalIoPage::readAllRequested);
    connect(m_writeButton, &QPushButton::clicked,
            this, &DigitalIoPage::applyConfiguration);
    setConnected(false);
}

void DigitalIoPage::setConnected(bool connected)
{
    for (QWidget *widget : std::initializer_list<QWidget *>{
             m_inputA, m_inputB, m_outputA, m_outputB}) {
        widget->setEnabled(connected);
    }
    m_writeButton->setEnabled(connected);
}

void DigitalIoPage::setParameterValue(quint16 parameter, quint16 value)
{
    switch (parameter) {
    case 13: m_inputA->setValue(value); break;
    case 14: m_inputB->setValue(value); break;
    case 15: m_outputA->setValue(value); break;
    case 16: m_outputB->setValue(value); break;
    default: break;
    }
}

void DigitalIoPage::applyConfiguration()
{
    if (QMessageBox::question(
            this, QStringLiteral("写入数字 I/O 配置"),
            QStringLiteral("将写入 Pn013/Pn014 输入配置和 Pn015/Pn016 输出配置。\n\n"
                           "请确认端子映射、有效电平和整机安全逻辑。"))
        != QMessageBox::Yes) {
        return;
    }
    emit parameterWriteRequested(13, static_cast<quint16>(m_inputA->value()));
    emit parameterWriteRequested(14, static_cast<quint16>(m_inputB->value()));
    emit parameterWriteRequested(15, static_cast<quint16>(m_outputA->value()));
    emit parameterWriteRequested(16, static_cast<quint16>(m_outputB->value()));
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
