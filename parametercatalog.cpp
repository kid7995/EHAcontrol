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
        {20, QStringLiteral("再生电阻投入电压阈值")},
        {24, QStringLiteral("母线电压限定")},
        {25, QStringLiteral("制动器稳态保持电压")},
        {26, QStringLiteral("制动器控制电压")},
        {31, QStringLiteral("位置环比例增益")},
        {32, QStringLiteral("速度环相关参数")},
        {33, QStringLiteral("位置指令滤波")},
        {34, QStringLiteral("前馈相关参数")},
        {44, QStringLiteral("目标设定力")},
        {48, QStringLiteral("质量基准清零")},
        {58, QStringLiteral("电机每转指令脉冲数1（高四位）")},
        {59, QStringLiteral("电机每转指令脉冲数2（低四位）")},
        {63, QStringLiteral("速度环比例增益 KVP")},
        {64, QStringLiteral("速度环积分增益 KVI")},
        {65, QStringLiteral("位置环比例增益1 KPP1")},
        {66, QStringLiteral("速度反馈滤波")},
        {67, QStringLiteral("速度环输出限幅")},
        {70, QStringLiteral("EtherCAT 丢帧容错阈值")},
        {80, QStringLiteral("电机极对数")},
        {85, QStringLiteral("电机额定电流")},
        {91, QStringLiteral("瞬时过载保护倍数")},
        {92, QStringLiteral("长时过载保护倍数")},
        {102, QStringLiteral("力矩零点触发")},
        {120, QStringLiteral("数字量输出配置")},
        {140, QStringLiteral("正向标定点 1")},
        {149, QStringLiteral("正向标定点 10")},
        {151, QStringLiteral("制动器控制类型")},
        {152, QStringLiteral("参数保存到 EEPROM")},
        {130, QStringLiteral("电机转速限定")},
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
        if (index == 85) {
            definition.group = QStringLiteral("电机");
        } else if (index == 25 || index == 26 || index == 151) {
            definition.group = QStringLiteral("制动器");
        } else if (index == 20) {
            definition.group = QStringLiteral("再生电阻");
        } else if (index == 24 || index == 91 || index == 92 || index == 130) {
            definition.group = QStringLiteral("限定");
        }
        definition.decimals = index == 85 ? 2 : 0;
        definition.signedDecimalEncoding = index == 169;

        switch (index) {
        case 44:
            definition.unit = QStringLiteral("N");
            break;
        case 20:
        case 24:
        case 25:
        case 26:
            definition.unit = QStringLiteral("V");
            break;
        case 85:
            definition.unit = QStringLiteral("A");
            break;
        case 91:
        case 92:
            definition.unit = QStringLiteral("×");
            break;
        case 130:
            definition.unit = QStringLiteral("rpm");
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
