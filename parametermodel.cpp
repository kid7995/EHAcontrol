#include "parametermodel.h"

#include <QBrush>
#include <QColor>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

#include <cmath>

ParameterModel::ParameterModel(QObject *parent)
    : QAbstractTableModel(parent)
    , m_definitions(ParameterCatalog::all().toVector())
    , m_entries(200)
{
}

int ParameterModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_definitions.size();
}

int ParameterModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant ParameterModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_definitions.size()) {
        return {};
    }

    const int parameterIndex = index.row();
    const ParameterDefinition &definition = m_definitions.at(parameterIndex);
    const Entry &entry = m_entries.at(parameterIndex);

    if (role == Qt::BackgroundRole && entry.dirty && index.column() != StateColumn) {
        return QBrush(QColor(QStringLiteral("#FFF7D6")));
    }
    if (role == Qt::ForegroundRole && index.column() == StateColumn) {
        return QBrush(entry.dirty ? QColor(QStringLiteral("#B86B00"))
                                  : QColor(QStringLiteral("#64748B")));
    }
    if (role == Qt::TextAlignmentRole) {
        if (index.column() == ParameterColumn || index.column() == ValueColumn
            || index.column() == StateColumn) {
            return int(Qt::AlignCenter);
        }
        return int(Qt::AlignVCenter | Qt::AlignLeft);
    }
    if (role == Qt::ToolTipRole && index.column() == NameColumn) {
        return QStringLiteral("%1 · %2").arg(definition.name, definition.group);
    }
    if (role != Qt::DisplayRole && role != Qt::EditRole) {
        return {};
    }

    switch (index.column()) {
    case ParameterColumn:
        return QStringLiteral("Pn%1").arg(definition.index, 3, 10, QLatin1Char('0'));
    case NameColumn:
        return definition.name;
    case ValueColumn:
        if (definition.signedDecimalEncoding) {
            return ParameterCatalog::decodePn169(entry.value);
        }
        if (definition.decimals > 0) {
            const double factor = std::pow(10.0, definition.decimals);
            return QString::number(entry.value / factor, 'f', definition.decimals);
        }
        return entry.value;
    case UnitColumn:
        return definition.unit;
    case GroupColumn:
        return definition.group;
    case StateColumn:
        return stateText(entry);
    default:
        return {};
    }
}

QVariant ParameterModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }
    switch (section) {
    case ParameterColumn:
        return QStringLiteral("参数");
    case NameColumn:
        return QStringLiteral("名称");
    case ValueColumn:
        return QStringLiteral("当前值");
    case UnitColumn:
        return QStringLiteral("单位");
    case GroupColumn:
        return QStringLiteral("分组");
    case StateColumn:
        return QStringLiteral("同步状态");
    default:
        return {};
    }
}

Qt::ItemFlags ParameterModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags result = QAbstractTableModel::flags(index);
    if (index.isValid() && index.column() == ValueColumn) {
        result |= Qt::ItemIsEditable;
    }
    return result;
}

bool ParameterModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::EditRole || index.column() != ValueColumn) {
        return false;
    }

    bool ok = false;
    const double numericValue = value.toDouble(&ok);
    if (!ok) {
        return false;
    }

    const int parameterIndex = index.row();
    const ParameterDefinition &definition = m_definitions.at(parameterIndex);
    quint16 rawValue = 0;

    if (definition.signedDecimalEncoding) {
        rawValue = ParameterCatalog::encodePn169(qRound(numericValue));
    } else {
        const double factor = std::pow(10.0, definition.decimals);
        const int scaledValue = qRound(numericValue * factor);
        if (scaledValue < 0 || scaledValue > 65535) {
            return false;
        }
        rawValue = static_cast<quint16>(scaledValue);
    }

    Entry &entry = m_entries[parameterIndex];
    const quint16 previousValue = entry.value;
    entry.value = rawValue;
    entry.dirty = !entry.confirmed || entry.value != entry.remoteValue;
    emit dataChanged(this->index(index.row(), 0),
                     this->index(index.row(), ColumnCount - 1));
    emit dirtyStateChanged(dirtyCount());
    if (previousValue != rawValue) {
        emit parameterEdited(definition.index, rawValue);
    }
    return true;
}

void ParameterModel::setRemoteValue(quint16 parameter, quint16 rawValue)
{
    if (parameter >= m_entries.size()) {
        return;
    }
    Entry &entry = m_entries[parameter];
    entry.remoteValue = rawValue;
    entry.value = rawValue;
    entry.confirmed = true;
    entry.dirty = false;

    emit dataChanged(index(parameter, 0), index(parameter, ColumnCount - 1));
    emit dirtyStateChanged(dirtyCount());
}

void ParameterModel::markWritten(quint16 parameter)
{
    if (parameter >= m_entries.size()) {
        return;
    }
    Entry &entry = m_entries[parameter];
    entry.remoteValue = entry.value;
    entry.confirmed = true;
    entry.dirty = false;

    emit dataChanged(index(parameter, 0), index(parameter, ColumnCount - 1));
    emit dirtyStateChanged(dirtyCount());
}

int ParameterModel::applyReadValues(const QMap<quint16, quint16> &values)
{
    beginResetModel();
    int applied = 0;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        if (it.key() >= m_entries.size()) {
            continue;
        }
        Entry &entry = m_entries[it.key()];
        entry.remoteValue = it.value();
        entry.value = it.value();
        entry.confirmed = true;
        entry.dirty = false;
        ++applied;
    }
    endResetModel();
    emit dirtyStateChanged(dirtyCount());
    return applied;
}

QMap<quint16, quint16> ParameterModel::dirtyValues() const
{
    QMap<quint16, quint16> result;
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).dirty) {
            result.insert(static_cast<quint16>(i), m_entries.at(i).value);
        }
    }
    return result;
}

QMap<quint16, quint16> ParameterModel::allValues() const
{
    QMap<quint16, quint16> result;
    for (int i = 0; i < m_entries.size(); ++i) {
        result.insert(static_cast<quint16>(i), m_entries.at(i).value);
    }
    return result;
}

quint16 ParameterModel::value(quint16 parameter) const
{
    return parameter < m_entries.size() ? m_entries.at(parameter).value : 0;
}

QString ParameterModel::parameterName(quint16 parameter) const
{
    return parameter < m_definitions.size() ? m_definitions.at(parameter).name : QString();
}

int ParameterModel::confirmedCount() const
{
    int count = 0;
    for (const Entry &entry : m_entries) {
        count += entry.confirmed ? 1 : 0;
    }
    return count;
}

int ParameterModel::dirtyCount() const
{
    int count = 0;
    for (const Entry &entry : m_entries) {
        count += entry.dirty ? 1 : 0;
    }
    return count;
}

bool ParameterModel::exportCsv(const QString &fileName, QString *error) const
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream << "Pn,Name,Value,Unit,Group,State\n";
    for (const ParameterDefinition &definition : m_definitions) {
        const Entry &entry = m_entries.at(definition.index);
        const double exportedValue = definition.signedDecimalEncoding
            ? ParameterCatalog::decodePn169(entry.value)
            : (definition.decimals > 0
                   ? entry.value / std::pow(10.0, definition.decimals)
                   : static_cast<double>(entry.value));
        QString escapedName = definition.name;
        escapedName.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        stream << definition.index << ",\"" << escapedName << "\","
               << QString::number(exportedValue, 'f', definition.decimals) << ','
               << definition.unit << ',' << definition.group << ',' << stateText(entry) << '\n';
    }
    return true;
}

bool ParameterModel::importCsv(const QString &fileName, QString *error)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    QString line = stream.readLine();
    if (!line.startsWith(QStringLiteral("Pn"))) {
        if (error) {
            *error = QStringLiteral("CSV 表头无效，应为 Pn,Name,Value,...");
        }
        return false;
    }

    int imported = 0;
    while (!stream.atEnd()) {
        line = stream.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const QStringList fields = line.split(QLatin1Char(','));
        if (fields.size() < 3) {
            continue;
        }
        bool parameterOk = false;
        bool valueOk = false;
        const int parameter = fields.at(0).toInt(&parameterOk);
        const double logicalValue = fields.at(2).toDouble(&valueOk);
        if (!parameterOk || !valueOk || parameter < 0 || parameter >= m_entries.size()) {
            continue;
        }

        Entry &entry = m_entries[parameter];
        if (m_definitions.at(parameter).signedDecimalEncoding) {
            entry.value = ParameterCatalog::encodePn169(qRound(logicalValue));
        } else {
            const double factor = std::pow(10.0, m_definitions.at(parameter).decimals);
            entry.value = static_cast<quint16>(
                qBound(0, qRound(logicalValue * factor), 65535));
        }
        entry.dirty = !entry.confirmed || entry.value != entry.remoteValue;
        ++imported;
    }

    emit dataChanged(index(0, 0), index(qMax(0, rowCount() - 1), ColumnCount - 1));
    emit dirtyStateChanged(dirtyCount());
    if (imported == 0 && error) {
        *error = QStringLiteral("文件中没有可导入的参数行");
    }
    return imported > 0;
}

bool ParameterModel::exportDriveText(const QString &fileName, QString *error) const
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    // The reference application stores the raw 16-bit values, not the
    // engineering values shown in the editor. Keep this format lossless.
    for (const ParameterDefinition &definition : m_definitions) {
        const Entry &entry = m_entries.at(definition.index);
        stream << QStringLiteral("Pn%1 %2\n")
                      .arg(definition.index, 3, 10, QLatin1Char('0'))
                      .arg(entry.value);
    }
    return true;
}

bool ParameterModel::importDriveText(const QString &fileName, QString *error)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    int imported = 0;
    int invalid = 0;
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        const QStringList fields = line.split(QRegularExpression(QStringLiteral("\\s+")),
                                              Qt::SkipEmptyParts);
        if (fields.size() < 2 || !fields.first().startsWith(QStringLiteral("Pn"),
                                                            Qt::CaseInsensitive)) {
            ++invalid;
            continue;
        }
        bool parameterOk = false;
        bool valueOk = false;
        const int parameter = fields.at(0).mid(2).toInt(&parameterOk);
        const uint rawValue = fields.at(1).toUInt(&valueOk);
        if (!parameterOk || !valueOk || parameter < 0
            || parameter >= m_entries.size() || rawValue > 65535) {
            ++invalid;
            continue;
        }

        Entry &entry = m_entries[parameter];
        entry.value = static_cast<quint16>(rawValue);
        entry.dirty = !entry.confirmed || entry.value != entry.remoteValue;
        ++imported;
    }
    emit dataChanged(index(0, 0), index(qMax(0, rowCount() - 1), ColumnCount - 1));
    emit dirtyStateChanged(dirtyCount());
    if (imported == 0 && error) {
        *error = invalid > 0
            ? QStringLiteral("文件中没有有效的参数行；应为“PnNNN 原始值”，范围 0–65535")
            : QStringLiteral("文件中没有可识别的“PnNNN 原始值”参数行");
    }
    return imported > 0;
}

void ParameterModel::restoreRemoteValues()
{
    beginResetModel();
    for (Entry &entry : m_entries) {
        entry.value = entry.remoteValue;
        entry.dirty = false;
    }
    endResetModel();
    emit dirtyStateChanged(0);
}

QString ParameterModel::stateText(const Entry &entry) const
{
    if (entry.dirty) {
        return QStringLiteral("待写入");
    }
    return entry.confirmed ? QStringLiteral("已同步") : QStringLiteral("未读取");
}
