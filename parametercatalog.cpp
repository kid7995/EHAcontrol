#include "parametercatalog.h"

#include <QHash>

namespace {

QString pointName(quint16 index)
{
    if (index >= 140 && index <= 149) {
        return QStringLiteral("正向标定点 %1").arg(index - 139);
    }
    if (index >= 180 && index <= 189) {
        return QStringLiteral("反向标定点 %1").arg(index - 179);
    }
    return QStringLiteral("驱动器参数 Pn%1").arg(index, 3, 10, QLatin1Char('0'));
}

QList<ParameterDefinition> buildCatalog()
{
    const QHash<quint16, QString> names {
        {0, QStringLiteral("系统复位")},
        {2, QStringLiteral("数显屏显示模式")},
        {31, QStringLiteral("位置环比例增益")},
        {32, QStringLiteral("速度环相关参数")},
        {33, QStringLiteral("位置指令滤波")},
        {34, QStringLiteral("前馈相关参数")},
        {44, QStringLiteral("目标设定力")},
        {48, QStringLiteral("质量基准清零")},
        {58, QStringLiteral("回零方向选择")},
        {59, QStringLiteral("回零速度")},
        {63, QStringLiteral("速度环比例增益")},
        {64, QStringLiteral("速度环积分时间")},
        {65, QStringLiteral("速度环微分参数")},
        {66, QStringLiteral("速度反馈滤波")},
        {67, QStringLiteral("速度环输出限幅")},
        {102, QStringLiteral("力矩零点触发")},
        {120, QStringLiteral("数字量输出配置")},
        {140, QStringLiteral("正向标定点 1")},
        {149, QStringLiteral("正向标定点 10")},
        {152, QStringLiteral("参数保存到 EEPROM")},
        {160, QStringLiteral("控制模式选择")},
        {168, QStringLiteral("力标定 K 值（增益）")},
        {169, QStringLiteral("力标定 B 值（偏置）")},
        {170, QStringLiteral("质量/重力补偿")},
        {172, QStringLiteral("内部参数初始化模式")},
        {180, QStringLiteral("反向标定点 1")},
        {189, QStringLiteral("反向标定点 10")},
        {199, QStringLiteral("参数读取结束标志")},
    };

    QList<ParameterDefinition> result;
    result.reserve(200);
    for (quint16 index = 0; index < 200; ++index) {
        ParameterDefinition definition;
        definition.index = index;
        definition.name = names.value(index, pointName(index));
        definition.group = ParameterCatalog::defaultGroupForIndex(index);
        definition.decimals = index == 169 ? 0 : 0;
        definition.signedDecimalEncoding = index == 169;

        switch (index) {
        case 44:
            definition.unit = QStringLiteral("N");
            break;
        case 169:
            definition.unit = QStringLiteral("逻辑值");
            break;
        default:
            break;
        }
        result.append(definition);
    }
    return result;
}

} // namespace

const QList<ParameterDefinition> &ParameterCatalog::all()
{
    static const QList<ParameterDefinition> catalog = buildCatalog();
    return catalog;
}

QStringList ParameterCatalog::groups()
{
    return {
        QStringLiteral("全部参数"),
        QStringLiteral("电机"),
        QStringLiteral("反馈"),
        QStringLiteral("制动器"),
        QStringLiteral("限定"),
        QStringLiteral("再生电阻"),
        QStringLiteral("数字 I/O"),
        QStringLiteral("控制与标定"),
        QStringLiteral("系统"),
    };
}

QString ParameterCatalog::defaultGroupForIndex(quint16 index)
{
    if (index <= 39) {
        return QStringLiteral("电机");
    }
    if (index <= 59) {
        return QStringLiteral("反馈");
    }
    if (index <= 79) {
        return QStringLiteral("制动器");
    }
    if (index <= 99) {
        return QStringLiteral("限定");
    }
    if (index <= 119) {
        return QStringLiteral("再生电阻");
    }
    if (index <= 139) {
        return QStringLiteral("数字 I/O");
    }
    if (index <= 179) {
        return QStringLiteral("控制与标定");
    }
    return QStringLiteral("系统");
}

int ParameterCatalog::decodePn169(quint16 rawValue)
{
    return rawValue >= 10000 ? -static_cast<int>(rawValue - 10000) : static_cast<int>(rawValue);
}

quint16 ParameterCatalog::encodePn169(int logicalValue)
{
    if (logicalValue >= 0) {
        return static_cast<quint16>(qMin(logicalValue, 9999));
    }
    return static_cast<quint16>(10000 + qMin(qAbs(logicalValue), 9999));
}
