#ifndef PARAMETERMODEL_H
#define PARAMETERMODEL_H

#include "parametercatalog.h"

#include <QAbstractTableModel>
#include <QMap>
#include <QVector>

class ParameterModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        ParameterColumn = 0,
        NameColumn,
        ValueColumn,
        UnitColumn,
        GroupColumn,
        StateColumn,
        ColumnCount
    };

    explicit ParameterModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value,
                 int role = Qt::EditRole) override;

    void setGroupFilter(const QString &group);
    void setTextFilter(const QString &text);
    void setRemoteValue(quint16 parameter, quint16 rawValue);
    void markWritten(quint16 parameter);
    int applyReadValues(const QMap<quint16, quint16> &values);

    QMap<quint16, quint16> dirtyValues() const;
    QMap<quint16, quint16> allValues() const;
    quint16 value(quint16 parameter) const;
    QString parameterName(quint16 parameter) const;
    int confirmedCount() const;
    int dirtyCount() const;

    bool exportCsv(const QString &fileName, QString *error = nullptr) const;
    bool importCsv(const QString &fileName, QString *error = nullptr);
    void restoreRemoteValues();

signals:
    void dirtyStateChanged(int count);
    void groupFilterChanged(const QString &group);

private:
    struct Entry {
        quint16 value = 0;
        quint16 remoteValue = 0;
        bool confirmed = false;
        bool dirty = false;
    };

    void rebuildVisibleRows();
    int rowForParameter(quint16 parameter) const;
    QString stateText(const Entry &entry) const;
    bool matchesFilter(const ParameterDefinition &definition) const;

    QVector<ParameterDefinition> m_definitions;
    QVector<Entry> m_entries;
    QVector<int> m_visibleIndices;
    QString m_groupFilter = QStringLiteral("全部参数");
    QString m_textFilter;
};

#endif // PARAMETERMODEL_H
