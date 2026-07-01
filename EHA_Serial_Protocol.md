# EHA 驱动器串口通信协议文档

> 从 EHAcontrol 项目（Qt 5.15, MinGW 64bit）中提炼  
> 适用场景：任何需要通过串口与赛威德 EHA 驱动器通信的上位机程序

---

## 1. 串口硬件参数

| 参数       | 值              |
|------------|-----------------|
| 波特率     | 115200          |
| 数据位     | 8               |
| 校验位     | None（无）      |
| 停止位     | 1               |
| 流控       | None（无）      |
| 方向       | 全双工 ReadWrite |

```cpp
serial->setBaudRate(QSerialPort::Baud115200);
serial->setDataBits(QSerialPort::Data8);
serial->setParity(QSerialPort::NoParity);
serial->setStopBits(QSerialPort::OneStop);
serial->setFlowControl(QSerialPort::NoFlowControl);
serial->open(QIODevice::ReadWrite);
```

---

## 2. 上位机 → 驱动器：发送帧格式

所有发送帧均为 **8 字节定长**，大端序（高字节在前）。

### 2.1 通用写参数帧

```
字节位置:  0     1     2     3     4     5     6     7
内容:     0x55  0x55  PnH   PnL   ValH  ValL  0xAA  0xAA
```

| 字段  | 长度  | 说明                          |
|-------|-------|-------------------------------|
| 0x55 0x55 | 2B | 帧头，固定           |
| PnH PnL   | 2B | 参数编号，大端 uint16 |
| ValH ValL | 2B | 参数值，大端 uint16   |
| 0xAA 0xAA | 2B | 帧尾，固定           |

**Qt 实现：**
```cpp
void sendData(quint16 pn, quint16 value) {
    QByteArray data;
    data.append(0x55);
    data.append(0x55);
    data.append(static_cast<char>((pn    >> 8) & 0xFF));
    data.append(static_cast<char>( pn           & 0xFF));
    data.append(static_cast<char>((value >> 8) & 0xFF));
    data.append(static_cast<char>( value        & 0xFF));
    data.append(0xAA);
    data.append(0xAA);
    serial->write(data);
    // 每帧发送后建议等待 200ms，确保驱动器处理完毕
    delayMS(200);
}
```

---

### 2.2 特殊控制帧（固定内容）

| 用途              | 帧内容（HEX）                         | 说明                                      |
|-------------------|---------------------------------------|-------------------------------------------|
| **握手包**        | `55 55 B9 9B 9B B9 AA AA`             | 串口打开后**必须第一帧发送**，否则设备不响应 |
| **心跳包**        | `55 55 B6 B6 7B 7B AA AA`             | 每 **100ms** 发送一次，维持通信活跃        |
| **读取所有参数**  | `55 55 FE FE EF EF AA AA`             | 触发设备推送全部 PN 参数值                  |

**连接时序（必须严格遵守）：**
```
1. serial->open()
2. 发送握手包: 55 55 B9 9B 9B B9 AA AA
3. delayMS(50)          // 等待设备就绪
4. sendData(2, 79)      // 设置参数监视模式
5. 启动心跳定时器(100ms)
```

---

## 3. 驱动器 → 上位机：接收帧格式

接收数据为**流式**，需在接收缓冲区中扫描帧头定位数据。

### 3.1 参数值回报帧（响应"读所有参数"命令）

```
帧头:     EF  AD  AA
参数号:   PnH PnL       (2字节, 大端 uint16)
参数值:   ValH ValL     (2字节, 大端 uint16)
```

**解析代码：**
```cpp
// 在接收缓冲区 buf 中扫描
for (int i = 0; i <= buf.size() - 7; ++i) {
    if ((quint8)buf[i]   == 0xEF &&
        (quint8)buf[i+1] == 0xAD &&
        (quint8)buf[i+2] == 0xAA) {
        quint16 pn  = ((quint8)buf[i+3] << 8) | (quint8)buf[i+4];
        quint16 val = ((quint8)buf[i+5] << 8) | (quint8)buf[i+6];
        // 处理 pn / val
    }
}
```

**关键参数编号（十六进制）：**

| PN编号 (dec) | 十六进制 | 说明                |
|-------------|----------|---------------------|
| 168         | 0x00A8   | K值（增益）          |
| 169         | 0x00A9   | B值（偏置）          |
| 199         | 0x00C7   | 最后一个参数（结束标志）|

> **全参读取结束判断**：收到 PN199 (`0x00C7`) 时说明所有参数已推送完毕。

---

### 3.2 实时监控推送帧（心跳期间持续推送）

```
帧头:     EF  AD  AB
数据类型: type          (1字节)
数据内容: b4 b5 b6 b7  (4字节, 大端)
```

**数据类型 `type` 含义：**

| type值 | 含义         | 数值类型      | 典型用途               |
|--------|--------------|---------------|------------------------|
| `0x07` | 转矩/力反馈  | uint32        | 实时显示输出力         |
| `0x08` | 电流         | int32（有符号）| 电流监控               |
| `0x09` | 速度         | int32（有符号）| 速度监控               |
| `0x0A` | 多圈位置     | uint32        | 绝对位置               |
| `0x0B` | 单圈位置     | uint32        | 单圈编码器值           |
| `0x0C` | 位置         | int32（有符号）| 相对位置               |
| `0x1A` | 母线电压     | uint32        | 电源电压监控           |

**解析代码：**
```cpp
for (int i = 0; i <= buf.size() - 8; ++i) {
    if ((quint8)buf[i]   != 0xEF ||
        (quint8)buf[i+1] != 0xAD ||
        (quint8)buf[i+2] != 0xAB) continue;

    quint8  type = (quint8)buf[i+3];
    quint32 rawU = ((quint32)(quint8)buf[i+4] << 24)
                 | ((quint32)(quint8)buf[i+5] << 16)
                 | ((quint32)(quint8)buf[i+6] <<  8)
                 |  (quint32)(quint8)buf[i+7];
    qint32  rawS = (qint32)rawU;

    switch (type) {
    case 0x07: /* 力/转矩反馈: rawU */ break;
    case 0x09: /* 速度:        rawS */ break;
    case 0x0C: /* 位置:        rawS */ break;
    }
}
```

---

## 4. 参数表（PN Parameter 速查）

| PN编号 | 功能           | 常用写入值                   | 说明                                         |
|--------|----------------|------------------------------|----------------------------------------------|
| 0      | 系统复位       | 9999                         | 内部参数复位                                 |
| 2      | 数显屏显示模式 | 79 = 转矩反馈<br>81 = 位移量 | 控制驱动器面板显示内容                       |
| 44     | 设定力 (Pn044) | 0 ~ 目标力 (单位: N×10)     | `settleForce(N)` 实际写 `pn=0x002C, val=N`  |
| 48     | 质量基准清零   | 0                            | 清除质量基准值                               |
| 102    | 力矩清零命令   | 1 → 触发 / 0 → 复位          | 先写1再写0触发一次清零                       |
| 152    | 参数保存       | 1                            | 将当前参数固化到 EEPROM                      |
| 160    | 控制模式       | 0 = 串口控制<br>1 = EtherCAT | 切换前必须保证设备安全                       |
| 168    | K值（增益）    | 计算所得正整数               | 线性标定斜率，写入后生效                     |
| 169    | B值（偏置）    | 见下方编码规则               | 线性标定截距，**支持正负值，编码特殊**       |
| 170    | 质量补偿       | 0 ~ 补偿值                   | 补偿负载自重引起的力偏差                     |

---

## 4.1 PN169 负数编码规则（重要）

PN169 **不使用标准二进制补码**，采用**十进制符号位**编码：

| 目标逻辑值 | 实际写入值 | 计算规则 |
|-----------|-----------|---------|
| +100      | 100       | 正数直接写 |
| 0         | 0         | 零直接写 |
| **−100**  | **10100** | `10000 + 100` |
| −500      | 10500     | `10000 + 500` |
| −1500     | 11500     | `10000 + 1500` |

> 写入值 ≥ 10000 时驱动器解释为负数，有效范围：**±9999**

```cpp
// 编码（逻辑值 → 写入值）
quint16 encodePn169(int logicalValue) {
    if (logicalValue >= 0)
        return static_cast<quint16>(qMin(logicalValue, 9999));
    else
        return static_cast<quint16>(10000 + qMin(qAbs(logicalValue), 9999));
}

// 解码（读回值 → 逻辑值）
int decodePn169(quint16 rawValue) {
    return (rawValue >= 10000) ? -(int)(rawValue - 10000) : (int)rawValue;
}

// 使用示例
sendData(169, encodePn169( 300));  // → 写入 300   (实际 +300)
sendData(169, encodePn169(-100));  // → 写入 10100 (实际 -100)
```

> ⚠️ **切勿**将负数直接强转 `quint16` 写入（-100 会变成 65436，完全错误）

---

## 5. 典型操作流程（伪代码）

### 5.1 连接设备
```
open(serial, 115200, 8N1)
send(握手包: 55 55 B9 9B 9B B9 AA AA)
wait(50ms)
sendData(2, 79)          // 切换到转矩监视
startTimer(心跳, 100ms)  // 每100ms发心跳包
```

### 5.2 读取参数（以读取 PN168 为例）
```
stop(心跳定时器)                    // 避免心跳干扰响应
send(读所有参数: 55 55 FE FE EF EF AA AA)
// 在 readyRead 回调中扫描 EF AD AA 帧头
// 找到 pn==0x00A8(168) 时记录 val
// 找到 pn==0x00C7(199) 时表示传输完毕
start(心跳定时器)                   // 恢复心跳
```

### 5.3 设置输出力
```
sendData(44, targetForce)   // Pn044 = 目标力值 (N)
wait(2000ms)                // 等待力值稳定
read(sensorforceValue)      // 从力传感器读取实际力
```

### 5.4 力矩清零
```
sendData(44, 200)           // 施加预紧力
wait(2000ms)
sendData(102, 1)            // 触发清零
sendData(102, 0)            // 复位清零信号
```

### 5.5 写入标定 K 值并保存
```
sendData(168, K_write)      // 写入新 K 值到 PN168
sendData(152, 1)            // 保存参数到 EEPROM
sendData(2, 79)             // 切回转矩反馈显示
```

---

## 6. Qt 工程配置要点

### CMakeLists.txt / .pro 依赖
```cmake
find_package(Qt5 COMPONENTS SerialPort REQUIRED)
target_link_libraries(your_target Qt5::SerialPort)
```

### 接收缓冲区管理（防溢出）
```cpp
// 在 onSerialReadyRead 末尾加：
if (m_serialRxBuffer.size() > 4096)
    m_serialRxBuffer.remove(0, m_serialRxBuffer.size() - 2048);
```

### 阻塞延时（保持事件循环）
```cpp
// 不要用 QThread::sleep！用 QEventLoop 保持 UI 响应：
static void delayMS(int ms) {
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}
```

---

## 7. 注意事项

1. **握手包必须在第一帧发送**，否则驱动器不响应任何命令。
2. **心跳包不可缺失**，超时后设备会停止推送实时数据；读参数期间需暂停心跳。
3. **每帧 sendData 之间需 200ms 间隔**，驱动器处理速度有限，连续发送会丢包。
4. **读所有参数期间停止心跳**，两种帧头（`EF AD AA` vs `EF AD AB`）可能互相干扰缓冲区解析。
5. **力值单位**：`sensorforceValue` 为原始值，换算为 N 需除以 10.0。
6. **K 值写入后立即生效**，无需重启；如需掉电保持须调用 `sendData(152, 1)` 保存。
7. **控制模式切换**（PN160）前确保设备处于安全状态，切换后需等待 1s 再操作。
