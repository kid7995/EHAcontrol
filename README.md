# KaiserStudio Qt

这是一个面向转数/位置伺服驱动器的 Qt 串口调试工作台，界面与工作流参考
`KaiserStudio`，并基于本项目已有的 EHA 串口协议重新组织代码。

## 当前功能

- 串口自动枚举，支持波特率、数据位、校验位和停止位配置
- 连接后自动发送握手帧、切换监视通道并启动 100 ms 心跳
- 所有页面常驻显示位置、多圈、单圈、速度、电流、母线电压和力反馈
- 顶部显示使能、故障、驱动模式和可切换的监视参数
- 读取设备 `Pn000`–`Pn199` 全部参数
- 参数搜索、分组过滤、六组并排紧凑网格、详细表格和批量写回
- 驱动器信息页集中显示通信架构、控制模式和关键标定参数
- `Pn169` 十进制符号位编码与解码
- 参数 CSV 导入、导出和撤销未写入修改
- 位置、多圈、单圈、速度、电流、母线电压、力/转矩实时卡片
- 7 通道可勾选叠加、最大 30 秒滚动示波图和监控数据 CSV 导出
- 电机使能、安全停止、故障复位和参数保存快捷命令
- 原始十六进制通信日志和自定义帧发送
- 参数写入、EEPROM 保存和退出前的安全确认

## 页面快捷键

| 快捷键 | 页面 |
|---|---|
| `F1` | 仪表盘 |
| `F2` | Pn 参数工作台 |
| `F3` | 实时示波器 |
| `F4` | 串口通信中心 |
| `F5` | 驱动器信息 |

## 代码结构

| 文件 | 职责 |
|---|---|
| `deviceserialservice.*` | 串口生命周期、心跳、帧收发、参数与遥测解析 |
| `parametercatalog.*` | Pn 参数定义、分组及 Pn169 编解码 |
| `parametermodel.*` | 200 项参数表、修改跟踪、CSV 备份恢复 |
| `dashboardpage.*` | 实时状态卡片和设备快捷操作 |
| `parameterpage.*` | 参数检索、编辑、批量写入和备份 |
| `monitorpage.*` | 实时曲线、统计和采样导出 |
| `communicationpage.*` | 串口设置、设备命令和原始通信日志 |
| `driveinfopage.*` | 驱动器身份、关键参数与通信架构概览 |
| `mainwindow.*` | 页面编排、业务确认和模块连接 |

旧版 EHA 自动标定界面保留在仓库历史及未参与构建的旧文件中，新分支以通用驱动器
调试工作台为入口。

## 构建

建议使用 Qt 5.15.2 MinGW 64-bit：

```powershell
cmake -S . -B build/KaiserStudio-Qt5-Release -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH=C:/Qt/5.15.2/mingw81_64 `
  -DCMAKE_MAKE_PROGRAM=C:/Qt/Tools/Ninja/ninja.exe
cmake --build build/KaiserStudio-Qt5-Release --parallel
```

构建产物为 `build/KaiserStudio-Qt5-Release/KaiserStudio.exe`。

## 硬件联调说明

参数读取和实时帧解析已按现有 `EHA_Serial_Protocol.md` 实现。正式连接设备前应先在
安全空载状态下核对串口、反馈倍率和故障位定义，再由现场人员确认使能、参数写入和
EEPROM 保存操作。
