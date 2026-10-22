#include "feedbackpage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

QFrame *panel(QWidget *parent)
{
    auto *frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("panel"));
    return frame;
}

QLabel *sectionTitle(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("sectionTitle"));
    return label;
}

QDoubleSpinBox *integerEditor(QWidget *parent, int maximum = 65535)
{
    auto *editor = new QDoubleSpinBox(parent);
    editor->setDecimals(0);
    editor->setRange(0, maximum);
    editor->setSingleStep(1);
    editor->setMinimumWidth(180);
    return editor;
}

} // namespace

FeedbackPage::FeedbackPage(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(22, 18, 22, 18);
    root->setSpacing(12);

    auto *headingRow = new QHBoxLayout;
    auto *heading = new QVBoxLayout;
    auto *title = new QLabel(QStringLiteral("反馈与编码器"), this);
    title->setObjectName(QStringLiteral("pageHeading"));
    auto *subtitle = new QLabel(
        QStringLiteral("配置编码器类型、位置清零、磁极检测与等价编码器输出。"
                       "编码器类型和清零命令通常需要重新上电或下使能后生效。"),
        this);
    subtitle->setObjectName(QStringLiteral("pageSubtitle"));
    subtitle->setWordWrap(true);
    heading->addWidget(title);
    heading->addWidget(subtitle);
    headingRow->addLayout(heading, 1);
    auto *readButton = new QPushButton(QStringLiteral("读取反馈参数"), this);
    readButton->setObjectName(QStringLiteral("secondaryButton"));
    headingRow->addWidget(readButton);
    root->addLayout(headingRow);

    auto *positionPanel = new QFrame(this);
    positionPanel->setObjectName(QStringLiteral("liveRibbon"));
    auto *positionLayout = new QHBoxLayout(positionPanel);
    positionLayout->setContentsMargins(16, 8, 16, 8);
    positionLayout->setSpacing(22);
    const auto addPositionValue = [positionPanel, positionLayout](
                                      const QString &caption, QLabel *&value) {
        auto *block = new QWidget(positionPanel);
        auto *layout = new QVBoxLayout(block);
        layout->setContentsMargins(0, 0, 0, 0);
        auto *captionLabel = new QLabel(caption, block);
        captionLabel->setObjectName(QStringLiteral("liveCaption"));
        value = new QLabel(QStringLiteral("--"), block);
        value->setObjectName(QStringLiteral("liveValue"));
        layout->addWidget(captionLabel);
        layout->addWidget(value);
        positionLayout->addWidget(block, 1);
    };
    addPositionValue(QStringLiteral("当前位置"), m_positionValue);
    addPositionValue(QStringLiteral("多圈位置"), m_multiTurnValue);
    addPositionValue(QStringLiteral("单圈位置"), m_singleTurnValue);
    root->addWidget(positionPanel);

    auto *tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("feedbackTabs"));

    auto *typePage = new QWidget(tabs);
    auto *typeLayout = new QVBoxLayout(typePage);
    typeLayout->setContentsMargins(16, 16, 16, 16);
    typeLayout->setSpacing(12);
    typeLayout->addWidget(sectionTitle(QStringLiteral("编码器类型"), typePage));
    m_encoderCombo = new QComboBox(typePage);
    appendEncoderOptions();
    typeLayout->addWidget(m_encoderCombo);
    m_encoderDescription = new QLabel(typePage);
    m_encoderDescription->setObjectName(QStringLiteral("fieldHint"));
    m_encoderDescription->setWordWrap(true);
    typeLayout->addWidget(m_encoderDescription);
    auto *encoderApplyRow = new QHBoxLayout;
    auto *encoderApplyButton = new QPushButton(QStringLiteral("写入编码器类型"), typePage);
    encoderApplyButton->setObjectName(QStringLiteral("primaryButton"));
    encoderApplyRow->addWidget(encoderApplyButton);
    encoderApplyRow->addStretch();
    auto *encoderNote = new QLabel(
        QStringLiteral("参考写入序列：Pn004、Pn006、Pn009、Pn150；"
                       "增量式编码器使用 Pn004 区分省线/非省线。修改后需重新上电。"),
        typePage);
    encoderNote->setObjectName(QStringLiteral("mutedText"));
    encoderNote->setWordWrap(true);
    encoderApplyRow->addWidget(encoderNote, 1);
    typeLayout->addLayout(encoderApplyRow);
    typeLayout->addStretch();
    tabs->addTab(typePage, QStringLiteral("编码器类型"));

    auto *positionTab = new QWidget(tabs);
    auto *positionPageLayout = new QVBoxLayout(positionTab);
    positionPageLayout->setContentsMargins(16, 16, 16, 16);
    positionPageLayout->setSpacing(12);
    positionPageLayout->addWidget(sectionTitle(QStringLiteral("位置清零"), positionTab));
    auto *positionWarning = new QLabel(
        QStringLiteral("参考版提示：当前位置清零指令只允许在下使能状态执行，"
                       "否则会返回“当前位置清零指令无效”。"),
        positionTab);
    positionWarning->setObjectName(QStringLiteral("fieldHint"));
    positionWarning->setWordWrap(true);
    positionPageLayout->addWidget(positionWarning);
    auto *absoluteRow = new QHBoxLayout;
    m_clearAbsoluteButton = new QPushButton(
        QStringLiteral("编码器绝对位置清零（Pn101 0→1→0）"), positionTab);
    m_clearAbsoluteButton->setObjectName(QStringLiteral("dangerButton"));
    m_absoluteStatus = new QLabel(QStringLiteral("等待设备状态"), positionTab);
    m_absoluteStatus->setObjectName(QStringLiteral("mutedText"));
    absoluteRow->addWidget(m_clearAbsoluteButton);
    absoluteRow->addWidget(m_absoluteStatus, 1);
    positionPageLayout->addLayout(absoluteRow);
    positionPageLayout->addStretch();
    tabs->addTab(positionTab, QStringLiteral("位置清零"));

    auto *polePage = new QWidget(tabs);
    auto *poleLayout = new QVBoxLayout(polePage);
    poleLayout->setContentsMargins(16, 16, 16, 16);
    poleLayout->setSpacing(12);
    poleLayout->addWidget(sectionTitle(QStringLiteral("编码器磁极检测"), polePage));
    m_poleDetectionSafety = new QCheckBox(
        QStringLiteral("我已确认电机可自由锁轴，并完成现场安全检查"), polePage);
    poleLayout->addWidget(m_poleDetectionSafety);
    m_poleDetectionStatus = new QLabel(
        QStringLiteral("磁极检测使能 Pn050：未读取"), polePage);
    m_poleDetectionStatus->setObjectName(QStringLiteral("mutedText"));
    poleLayout->addWidget(m_poleDetectionStatus);
    auto *poleForm = new QFormLayout;
    m_detectionTime = integerEditor(polePage);
    m_detectionFrequency = integerEditor(polePage);
    m_detectionVoltage = integerEditor(polePage);
    poleForm->addRow(QStringLiteral("检测旋转时间 Pn103"), m_detectionTime);
    poleForm->addRow(QStringLiteral("检测频率 Pn110"), m_detectionFrequency);
    poleForm->addRow(QStringLiteral("锁轴电压 Pn109"), m_detectionVoltage);
    poleLayout->addLayout(poleForm);
    auto *poleHint = new QLabel(
        QStringLiteral("参考说明：锁轴电压过低会导致刚度不足；"
                       "过高或旋转时间不合理可能导致过流。启动前请从较低电压逐步验证。"),
        polePage);
    poleHint->setObjectName(QStringLiteral("fieldHint"));
    poleHint->setWordWrap(true);
    poleLayout->addWidget(poleHint);
    auto *poleButtonRow = new QHBoxLayout;
    auto *applyPoleButton = new QPushButton(QStringLiteral("写入检测参数"), polePage);
    applyPoleButton->setObjectName(QStringLiteral("secondaryButton"));
    m_startDetectionButton = new QPushButton(QStringLiteral("开始检测"), polePage);
    m_startDetectionButton->setObjectName(QStringLiteral("dangerButton"));
    poleButtonRow->addWidget(applyPoleButton);
    poleButtonRow->addWidget(m_startDetectionButton);
    poleButtonRow->addStretch();
    poleLayout->addLayout(poleButtonRow);
    poleLayout->addStretch();
    tabs->addTab(polePage, QStringLiteral("磁极检测"));

    auto *outputPage = new QWidget(tabs);
    auto *outputLayout = new QVBoxLayout(outputPage);
    outputLayout->setContentsMargins(16, 16, 16, 16);
    outputLayout->setSpacing(12);
    outputLayout->addWidget(sectionTitle(QStringLiteral("等价编码器输出"), outputPage));
    auto *outputForm = new QFormLayout;
    m_outputType = new QComboBox(outputPage);
    m_outputType->addItem(QStringLiteral("无脉冲输出"), 0);
    m_outputType->addItem(QStringLiteral("AB 正交（A+B）"), 1);
    m_outputType->addItem(QStringLiteral("双脉冲（CW+CCW）"), 2);
    m_outputType->addItem(QStringLiteral("脉冲+方向（Pulse+Dir）"), 3);
    m_pulseHigh = integerEditor(outputPage);
    m_pulseLow = integerEditor(outputPage);
    outputForm->addRow(QStringLiteral("输出脉冲类型 Pn124"), m_outputType);
    outputForm->addRow(QStringLiteral("每圈脉冲数1（高四位）Pn123"), m_pulseHigh);
    outputForm->addRow(QStringLiteral("每圈脉冲数2（低四位）Pn122"), m_pulseLow);
    outputLayout->addLayout(outputForm);
    auto *outputHint = new QLabel(
        QStringLiteral("编码器每圈反馈脉冲在驱动器内部被分频为"
                       "“脉冲数1 × 10000 + 脉冲数2”。输出类型修改后重新上电生效。"),
        outputPage);
    outputHint->setObjectName(QStringLiteral("fieldHint"));
    outputHint->setWordWrap(true);
    outputLayout->addWidget(outputHint);
    auto *applyOutputButton = new QPushButton(QStringLiteral("写入等价输出设置"), outputPage);
    applyOutputButton->setObjectName(QStringLiteral("primaryButton"));
    outputLayout->addWidget(applyOutputButton, 0, Qt::AlignLeft);
    outputLayout->addStretch();
    tabs->addTab(outputPage, QStringLiteral("等价输出"));

    root->addWidget(tabs, 1);
    m_encoderStatus = new QLabel(
        QStringLiteral("参考编码器类型共 21 项：Nikon、Tamagawa、Biss-C 和增量式编码器。"),
        this);
    m_encoderStatus->setObjectName(QStringLiteral("mutedText"));
    root->addWidget(m_encoderStatus);

    connect(readButton, &QPushButton::clicked, this, &FeedbackPage::readAllRequested);
    connect(m_encoderCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FeedbackPage::updateEncoderDescription);
    connect(encoderApplyButton, &QPushButton::clicked,
            this, &FeedbackPage::applyEncoderType);
    connect(m_clearAbsoluteButton, &QPushButton::clicked,
            this, &FeedbackPage::clearAbsolutePosition);
    connect(m_poleDetectionSafety, &QCheckBox::toggled,
            this, &FeedbackPage::togglePoleDetectionSafety);
    connect(applyPoleButton, &QPushButton::clicked,
            this, &FeedbackPage::applyPoleDetectionSettings);
    connect(m_startDetectionButton, &QPushButton::clicked,
            this, &FeedbackPage::startPoleDetection);
    connect(applyOutputButton, &QPushButton::clicked,
            this, &FeedbackPage::applyOutputType);

    togglePoleDetectionSafety(false);
    updateEncoderDescription();
    setConnected(false);
}

void FeedbackPage::setConnected(bool connected)
{
    m_connected = connected;
    for (QWidget *widget : std::initializer_list<QWidget *>{
             m_encoderCombo, m_clearAbsoluteButton, m_poleDetectionSafety,
             m_detectionTime, m_detectionFrequency, m_detectionVoltage,
             m_outputType, m_pulseHigh, m_pulseLow}) {
        widget->setEnabled(connected);
    }
    updateValueLabels();
    togglePoleDetectionSafety(m_poleDetectionSafety->isChecked());
}

void FeedbackPage::setMotorEnabled(bool enabled)
{
    m_motorEnabled = enabled;
    m_clearAbsoluteButton->setEnabled(m_connected && !enabled);
    m_absoluteStatus->setText(
        enabled ? QStringLiteral("电机已使能，绝对清零按钮已锁定")
                : QStringLiteral("电机下使能，可执行绝对位置清零"));
}

void FeedbackPage::setParameterValue(quint16 parameter, quint16 value)
{
    switch (parameter) {
    case 122:
        m_pulseLow->setValue(value);
        break;
    case 123:
        m_pulseHigh->setValue(value);
        break;
    case 124:
        m_outputType->setCurrentIndex(
            qBound(0, static_cast<int>(value), m_outputType->count() - 1));
        break;
    case 103:
        m_detectionTime->setValue(value);
        break;
    case 110:
        m_detectionFrequency->setValue(value);
        break;
    case 109:
        m_detectionVoltage->setValue(value);
        break;
    case 50:
        m_poleDetectionStatus->setText(
            QStringLiteral("磁极检测使能 Pn050：%1")
                .arg(value == 0 ? QStringLiteral("Disabled")
                                : QStringLiteral("Enabled")));
        break;
    default:
        break;
    }
}

void FeedbackPage::updateTelemetry(const DeviceSerialService::Telemetry &telemetry)
{
    m_positionValue->setText(QString::number(telemetry.position, 'f', 0));
    m_multiTurnValue->setText(QString::number(telemetry.multiTurnPosition, 'f', 0));
    m_singleTurnValue->setText(QString::number(telemetry.singleTurnPosition, 'f', 0));
}

void FeedbackPage::appendEncoderOptions()
{
    const QVector<EncoderOption> options {
        {QStringLiteral("Nikon 绝对值编码器：20-bit Single / 16-bit Multi-turn"), 20, 16, 0},
        {QStringLiteral("Nikon 绝对值编码器：22-bit Single / 16-bit Multi-turn"), 22, 16, 0},
        {QStringLiteral("Nikon 绝对值编码器：24-bit Single / 16-bit Multi-turn"), 24, 16, 0},
        {QStringLiteral("Tamagawa 绝对值编码器：17-bit Single / 16-bit Multi-turn"), 17, 16, 1},
        {QStringLiteral("Tamagawa 绝对值编码器：23-bit Single / 16-bit Multi-turn"), 23, 16, 1},
        {QStringLiteral("Biss-C 编码器：18-bit Single / 0-bit Multi-turn"), 18, 0, 2},
        {QStringLiteral("Biss-C 编码器：18-bit Single / 16-bit Multi-turn"), 18, 16, 2},
        {QStringLiteral("Biss-C 编码器：19-bit Single / 0-bit Multi-turn"), 19, 0, 2},
        {QStringLiteral("Biss-C 编码器：19-bit Single / 16-bit Multi-turn"), 19, 16, 2},
        {QStringLiteral("Biss-C 编码器：20-bit Single / 0-bit Multi-turn"), 20, 0, 2},
        {QStringLiteral("Biss-C 编码器：20-bit Single / 16-bit Multi-turn"), 20, 16, 2},
        {QStringLiteral("Biss-C 编码器：21-bit Single / 0-bit Multi-turn"), 21, 0, 2},
        {QStringLiteral("Biss-C 编码器：21-bit Single / 16-bit Multi-turn"), 21, 16, 2},
        {QStringLiteral("Biss-C 编码器：22-bit Single / 0-bit Multi-turn"), 22, 0, 2},
        {QStringLiteral("Biss-C 编码器：22-bit Single / 16-bit Multi-turn"), 22, 16, 2},
        {QStringLiteral("非省线式增量编码器 A/B/Z+U/V/W：2500 线"), 2500, 0, 3},
        {QStringLiteral("省线式增量编码器 A/B/Z：2500 线"), 2500, 1, 3},
        {QStringLiteral("非省线式增量编码器 A/B/Z+U/V/W：5000 线"), 5000, 0, 3},
        {QStringLiteral("省线式增量编码器 A/B/Z：5000 线"), 5000, 1, 3},
        {QStringLiteral("非省线式增量编码器 A/B/Z+U/V/W：10000 线"), 10000, 0, 3},
        {QStringLiteral("省线式增量编码器 A/B/Z：10000 线"), 10000, 1, 3},
    };
    for (const EncoderOption &option : options) {
        m_encoderCombo->addItem(option.label);
    }
}

void FeedbackPage::updateEncoderDescription()
{
    const int index = m_encoderCombo->currentIndex();
    if (index < 0) {
        return;
    }
    m_encoderDescription->setText(
        index <= 14
            ? QStringLiteral("绝对值编码器：写入单圈位数、多圈位数和协议组。")
            : QStringLiteral("增量式编码器：写入省线/非省线类型，编码器线数决定基础分辨率。"));
}

void FeedbackPage::applyEncoderType()
{
    if (!m_connected || m_encoderCombo->currentIndex() < 0) {
        emit statusMessage(QStringLiteral("请先连接设备并选择编码器类型"));
        return;
    }
    if (QMessageBox::question(
            this, QStringLiteral("写入编码器类型"),
            QStringLiteral("编码器类型错误会导致方向、位置或磁极检测异常。\n\n"
                           "选择：%1\n\n确认写入并重新上电后检查反馈？")
                .arg(m_encoderCombo->currentText()))
        != QMessageBox::Yes) {
        return;
    }

    const int index = m_encoderCombo->currentIndex();
    if (index <= 2) {
        emit parameterWriteRequested(4, 1);
        emit parameterWriteRequested(6, index == 0 ? 20 : (index == 1 ? 22 : 24));
        emit parameterWriteRequested(9, 0);
    } else if (index <= 4) {
        emit parameterWriteRequested(4, 1);
        emit parameterWriteRequested(6, index == 3 ? 17 : 23);
        emit parameterWriteRequested(9, 1);
    } else if (index <= 14) {
        static const quint16 bits[] {18, 18, 19, 19, 20, 20, 21, 21, 22, 22};
        const bool multiTurn = ((index - 5) % 2) != 0;
        emit parameterWriteRequested(4, 1);
        emit parameterWriteRequested(6, bits[index - 5]);
        emit parameterWriteRequested(9, 2);
        emit parameterWriteRequested(150, multiTurn ? 16 : 0);
    } else {
        emit parameterWriteRequested(4, (index % 2 == 0) ? 3 : 0);
    }
    m_encoderStatus->setText(
        QStringLiteral("编码器类型已加入发送队列；请重新上电后复核方向与位置。"));
    emit statusMessage(QStringLiteral("编码器类型写入序列已提交"));
}

void FeedbackPage::clearAbsolutePosition()
{
    if (!m_connected || m_motorEnabled) {
        m_absoluteStatus->setText(
            QStringLiteral("电机必须在低使能状态才能执行绝对位置清零"));
        return;
    }
    if (QMessageBox::question(
            this, QStringLiteral("绝对位置清零"),
            QStringLiteral("该操作会重定义编码器绝对位置基准。"
                           "请确认机械零位和回零策略，是否继续？"))
        != QMessageBox::Yes) {
        return;
    }
    emit parameterWriteRequested(101, 0);
    emit parameterWriteRequested(101, 1);
    emit parameterWriteRequested(101, 0);
    m_absoluteStatus->setText(QStringLiteral("Pn101 绝对位置清零序列已提交"));
}

void FeedbackPage::togglePoleDetectionSafety(bool enabled)
{
    m_poleDetectionArmed = enabled;
    m_startDetectionButton->setEnabled(m_connected && enabled);
}

void FeedbackPage::applyPoleDetectionSettings()
{
    if (!m_connected) {
        emit statusMessage(QStringLiteral("请先连接设备"));
        return;
    }
    if (QMessageBox::question(
            this, QStringLiteral("写入磁极检测参数"),
            QStringLiteral("将写入 Pn103、Pn110、Pn109 和 Pn050。"
                           "启动检测前请确认电机可自由锁轴。"))
        != QMessageBox::Yes) {
        return;
    }
    emit parameterWriteRequested(103, static_cast<quint16>(m_detectionTime->value()));
    emit parameterWriteRequested(110, static_cast<quint16>(m_detectionFrequency->value()));
    emit parameterWriteRequested(109, static_cast<quint16>(m_detectionVoltage->value()));
    emit parameterWriteRequested(50, 1);
    m_poleDetectionStatus->setText(
        QStringLiteral("磁极检测使能 Pn050：Enabled（已写入）"));
    emit statusMessage(QStringLiteral("磁极检测参数已提交"));
}

void FeedbackPage::startPoleDetection()
{
    if (!m_connected || !m_poleDetectionArmed) {
        return;
    }
    if (QMessageBox::warning(
            this, QStringLiteral("启动磁极检测"),
            QStringLiteral("电机将执行锁轴和旋转动作。确认现场无人接触、"
                           "机构行程安全后继续。"),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
        != QMessageBox::Yes) {
        return;
    }
    emit rawFrameRequested(QByteArray::fromHex("55557AA7A77AAAAA"));
    emit statusMessage(QStringLiteral("已发送编码器磁极检测启动帧"));
}

void FeedbackPage::applyOutputType()
{
    if (!m_connected) {
        emit statusMessage(QStringLiteral("请先连接设备"));
        return;
    }
    if (QMessageBox::question(
            this, QStringLiteral("写入等价编码器输出"),
            QStringLiteral("将写入 Pn124、Pn123、Pn122。输出类型修改后重新上电生效。"))
        != QMessageBox::Yes) {
        return;
    }
    emit parameterWriteRequested(124, static_cast<quint16>(m_outputType->currentData().toInt()));
    emit parameterWriteRequested(123, static_cast<quint16>(m_pulseHigh->value()));
    emit parameterWriteRequested(122, static_cast<quint16>(m_pulseLow->value()));
}

void FeedbackPage::updateValueLabels()
{
    if (!m_connected) {
        m_positionValue->setText(QStringLiteral("--"));
        m_multiTurnValue->setText(QStringLiteral("--"));
        m_singleTurnValue->setText(QStringLiteral("--"));
    }
}
