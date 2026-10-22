#include "deviceconfigpage.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QVBoxLayout>

DeviceConfigPage::DeviceConfigPage(const QString &title, const QString &introduction,
                                   const QVector<Field> &fields, QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(22, 18, 22, 18);
    root->setSpacing(12);

    auto *headingRow = new QHBoxLayout;
    auto *heading = new QVBoxLayout;
    auto *headingLabel = new QLabel(title, this);
    headingLabel->setObjectName(QStringLiteral("pageHeading"));
    auto *introLabel = new QLabel(introduction, this);
    introLabel->setObjectName(QStringLiteral("pageSubtitle"));
    introLabel->setWordWrap(true);
    heading->addWidget(headingLabel);
    heading->addWidget(introLabel);
    headingRow->addLayout(heading, 1);
    m_readButton = new QPushButton(QStringLiteral("读取设备参数"), this);
    m_readButton->setObjectName(QStringLiteral("secondaryButton"));
    m_writeButton = new QPushButton(QStringLiteral("写入本页修改"), this);
    m_writeButton->setObjectName(QStringLiteral("primaryButton"));
    m_restoreButton = new QPushButton(QStringLiteral("恢复已读取值"), this);
    m_restoreButton->setObjectName(QStringLiteral("ghostButton"));
    headingRow->addWidget(m_readButton);
    headingRow->addWidget(m_writeButton);
    headingRow->addWidget(m_restoreButton);
    root->addLayout(headingRow);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("settingsContent"));
    auto *grid = new QGridLayout(content);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(12);
    grid->setColumnStretch(1, 1);

    int row = 0;
    for (const Field &definition : fields) {
        FieldWidget field;
        field.definition = definition;
        field.baseline = static_cast<quint16>(qRound(definition.defaultValue * definition.factor));
        field.loaded = definition.defaultValue > 0.0;

        auto *label = new QLabel(definition.label, content);
        label->setObjectName(QStringLiteral("infoCaption"));
        grid->addWidget(label, row, 0);

        auto *valueRow = new QHBoxLayout;
        valueRow->setSpacing(8);
        if (!definition.choices.isEmpty()) {
            field.combo = new QComboBox(content);
            int selected = 0;
            for (int i = 0; i < definition.choices.size(); ++i) {
                field.combo->addItem(definition.choices.at(i), i);
                if (qFuzzyCompare(i, definition.defaultValue)) {
                    selected = i;
                }
            }
            field.combo->setCurrentIndex(selected);
            valueRow->addWidget(field.combo, 1);
            connect(field.combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, &DeviceConfigPage::updateDirtyState);
        } else {
            field.spin = new QDoubleSpinBox(content);
            field.spin->setDecimals(definition.decimals);
            field.spin->setRange(0.0, 65535.0 / definition.factor);
            field.spin->setSingleStep(definition.decimals > 0 ? 0.01 : 1.0);
            field.spin->setValue(definition.defaultValue);
            field.spin->setMinimumWidth(180);
            valueRow->addWidget(field.spin, 1);
            connect(field.spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, &DeviceConfigPage::updateDirtyState);
        }
        if (!definition.unit.isEmpty()) {
            auto *unit = new QLabel(definition.unit, content);
            unit->setObjectName(QStringLiteral("mutedText"));
            valueRow->addWidget(unit);
        }
        grid->addLayout(valueRow, row, 1);
        ++row;

        if (!definition.hint.isEmpty()) {
            auto *hint = new QLabel(definition.hint, content);
            hint->setObjectName(QStringLiteral("fieldHint"));
            hint->setWordWrap(true);
            grid->addWidget(hint, row, 1);
            ++row;
        }
        m_fields.append(field);
    }
    grid->setRowStretch(row, 1);
    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    auto *footer = new QHBoxLayout;
    m_statusLabel = new QLabel(
        QStringLiteral("字段映射来自参考版 KaiserStudio 的页面事件与参数编号。"), this);
    m_statusLabel->setObjectName(QStringLiteral("mutedText"));
    footer->addWidget(m_statusLabel);
    footer->addStretch();
    root->addLayout(footer);

    connect(m_readButton, &QPushButton::clicked, this, &DeviceConfigPage::readAllRequested);
    connect(m_writeButton, &QPushButton::clicked, this, &DeviceConfigPage::applyChanges);
    connect(m_restoreButton, &QPushButton::clicked,
            this, &DeviceConfigPage::restoreLoadedValues);
    setConnected(false);
}

void DeviceConfigPage::setConnected(bool connected)
{
    m_connected = connected;
    m_readButton->setEnabled(connected);
    m_writeButton->setEnabled(connected);
}

void DeviceConfigPage::setParameterValue(quint16 parameter, quint16 value)
{
    for (FieldWidget &field : m_fields) {
        if (field.definition.parameter == parameter) {
            field.baseline = value;
            field.loaded = true;
            if (field.combo) {
                field.combo->blockSignals(true);
                field.combo->setCurrentIndex(
                    qBound(0, static_cast<int>(value), field.combo->count() - 1));
                field.combo->blockSignals(false);
            } else if (field.spin) {
                field.spin->blockSignals(true);
                field.spin->setValue(value / field.definition.factor);
                field.spin->blockSignals(false);
            }
        }
    }
    updateDirtyState();
}

quint16 DeviceConfigPage::currentRawValue(const FieldWidget &field) const
{
    if (field.combo) {
        return static_cast<quint16>(field.combo->currentIndex());
    }
    return static_cast<quint16>(
        qBound(0, qRound(field.spin->value() * field.definition.factor), 65535));
}

void DeviceConfigPage::setWidgetValue(FieldWidget &field, quint16 rawValue)
{
    if (field.combo) {
        field.combo->blockSignals(true);
        field.combo->setCurrentIndex(
            qBound(0, static_cast<int>(rawValue), field.combo->count() - 1));
        field.combo->blockSignals(false);
    } else if (field.spin) {
        field.spin->blockSignals(true);
        field.spin->setValue(rawValue / field.definition.factor);
        field.spin->blockSignals(false);
    }
}

void DeviceConfigPage::applyChanges()
{
    QVector<FieldWidget *> pending;
    QStringList preview;
    for (FieldWidget &field : m_fields) {
        const quint16 value = currentRawValue(field);
        if (!field.loaded || value != field.baseline) {
            pending.append(&field);
            preview.append(QStringLiteral("Pn%1 = %2")
                               .arg(field.definition.parameter, 3, 10, QLatin1Char('0'))
                               .arg(value));
        }
    }
    if (pending.isEmpty()) {
        emit statusMessage(QStringLiteral("本页没有需要写入的修改"));
        return;
    }
    if (QMessageBox::question(
            this, QStringLiteral("确认写入设备"),
            QStringLiteral("将写入 %1 个参数：\n\n%2\n\n参数修改可能立即影响设备动作。")
                .arg(pending.size())
                .arg(preview.join(QLatin1Char('\n'))))
        != QMessageBox::Yes) {
        return;
    }

    for (FieldWidget *field : pending) {
        const quint16 value = currentRawValue(*field);
        emit writeRequested(field->definition.parameter, value);
        field->baseline = value;
        field->loaded = true;
    }
    emit statusMessage(QStringLiteral("已提交 %1 个本页参数").arg(pending.size()));
    updateDirtyState();
}

void DeviceConfigPage::restoreLoadedValues()
{
    for (FieldWidget &field : m_fields) {
        setWidgetValue(field, field.baseline);
    }
    updateDirtyState();
}

void DeviceConfigPage::updateDirtyState()
{
    int dirty = 0;
    for (const FieldWidget &field : m_fields) {
        dirty += (!field.loaded || currentRawValue(field) != field.baseline) ? 1 : 0;
    }
    m_writeButton->setText(dirty > 0
                               ? QStringLiteral("写入本页修改 (%1)").arg(dirty)
                               : QStringLiteral("写入本页修改"));
    m_writeButton->setProperty("dirty", dirty > 0);
    m_writeButton->style()->unpolish(m_writeButton);
    m_writeButton->style()->polish(m_writeButton);
}
