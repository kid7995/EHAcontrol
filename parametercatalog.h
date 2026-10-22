#ifndef PARAMETERCATALOG_H
#define PARAMETERCATALOG_H

#include <QList>
#include <QString>

struct ParameterDefinition
{
    quint16 index = 0;
    QString name;
    QString group;
    QString unit;
    int decimals = 0;
    bool signedDecimalEncoding = false;
};

class ParameterCatalog
{
public:
    static const QList<ParameterDefinition> &all();
    static QStringList groups();
    static QString defaultGroupForIndex(quint16 index);
    static int decodePn169(quint16 rawValue);
    static quint16 encodePn169(int logicalValue);
};

#endif // PARAMETERCATALOG_H
