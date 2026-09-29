# macOS U2D2 / TTL / 三维工作台实操

> 交付范围：GitHub 提供 MCU 源码、独立 DXL 命令行与本指引。Web 服务源码上传被连接工具拦截，尚未入库；本页涉及 `server.py`、`start_macos.sh`、`capture_board.py` 和三维界面的命令，请在本轮完整下载包 `Microduck_IMU_macOS_Diagnostics_20260929.zip` 解压目录执行，不能仅克隆仓库后直接运行。

使用已完成供电检查的单板，一次只接一块ID200。J4/J5不使用，ST-LINK烧完可拆。接线按实板已核准的引脚编号；旧J3.4电源文字有矛盾，不凭线色/左右判断。

## 1. 两种通信通道不要混用

|路线|接线|参数|用途|
|---|---|---|---|
|USB-TTL日志|适配器RX←J3.4，GND→J3.1；TX/VCC不接|115200、8N1、无流控|现有HSI-DXL版五字段日志；v2另有DBG2行|
|U2D2正式数据|U2D2 **TTL三针口** GND→J2.1、DATA→J2.3|1000000、8N1、Protocol2.0、ID200|Ping/Read/0x82 SyncRead及三维|
|UART1专用排障|需另刷UART1固件，USB-TTL TX→J3.5，RX←J3.4、GND→J3.1，VCC不接|1000000、8N1|绕过U3，不能据此验收单线DXL|

板子三种路线都从 **J1.2接外部稳压+5V，J1.1接电源GND**。USB-TTL要3.3V逻辑；不是RS232±电压，也不是ST-LINK。普通USB-UART的TX/RX不能短接后直接接DXL DATA。

U2D2不会从USB给板子供电，官方TTL三针口第2针N/C。正确EH三芯直通线可以接J2（三针编号一致）；板侧J2.2虽然有5V，不代表U2D2输出5V。自制测试线只接GND和DATA也可。全部电源必须共地且只有一个板电源来源。不得接四针RS485或把其他型号电机电源图套过来。

```text
Mac USB ─ U2D2 TTL3P ── GND ────── J2.1
                       DATA ───── J2.3
外部稳压 +5V ──────────────────── J1.2
外部电源 GND ──────────────────── J1.1
```

## 2. Mac安装与枚举

先解锁Mac，USB-C转接器必须支持数据。Apple Silicon笔记本出现配件提示时允许此探针；在“系统设置→隐私与安全性→允许配件连接”核查，不关闭SIP/系统安全，不盲目加载旧内核扩展。

优先使用系统已能枚举的VCP驱动：

```bash
sw_vers
uname -m
system_profiler SPUSBDataType
ls /dev/cu.*
```

U2D2通常出现 `/dev/cu.usbserial-...`，不是ST-LINK序列号，也不是Windows的COM6。先断开/再插入对比只确认枚举，接线修改必须断电。能看到端口且能打开时，不为“安装流程完整”额外换驱动。

若USB已枚举但无串口，按当前Mac系统/架构从 **FTDI官方VCP Drivers** 页面选择支持版本，而不是D2XX。官方当前提供DriverKit驱动；根据其说明安装并启动应用/允许系统扩展。不同macOS支持表不同，不固定套用某旧教程。驱动链接和Apple许可见 [SOURCES](SOURCES.md)。Windows设备管理器的Latency Timer=1ms不是macOS菜单；不建议靠卸载系统驱动或sudo hack来照搬。先记录实际时延。

官方DYNAMIXEL Wizard2可作为可选工具，选择符合当前macOS系统要求的版本；本项目私有型号0x7D00且不支持广播Ping，自动扫描不到不等于坏板。**不要对ID200运行Firmware Recovery、Factory Reset或写XL330控制表**。本包单播读取更直接。

## 3. 安装项目

以下命令在Mac实际连U2D2的终端，不是在远程GPU工作站。需要Python3.11+；缺失时通过Python官方安装包或你已有包管理器安装，不替换系统Python。

```bash
git clone https://github.com/superobk/microduck-imu.git
cd microduck-imu
bash scripts/setup_macos.sh
.venv/bin/python -m serial.tools.list_ports -v
```

setup只在项目内创建venv并装pyserial3.5，首次需要网络。已有uv也可自行创建等价环境；不改训练依赖。后续以下PORT均替换为实际值：

```bash
PORT=/dev/cu.usbserial-实际编号
# TTL日志时使用USB-TTL对应的另一个端口和115200
.venv/bin/python -m serial.tools.miniterm "$PORT" 115200
# Ctrl+]退出；随后U2D2使用其自己的实际端口，不对U2D2期待自发文字
```

## 4. U2D2只读与诊断

关闭Wizard、其他串口助手以及网页采集，一个端口一次只由一个程序打开：

```bash
PORT=/dev/cu.usbserial-实际U2D2编号
PROBE=src/firmware/tools/imu_probe.py
.venv/bin/python "$PROBE" --port "$PORT" --baud 1000000 ping
.venv/bin/python "$PROBE" --port "$PORT" diag
.venv/bin/python "$PROBE" --port "$PORT" read --addr 124 --length 12
.venv/bin/python scripts/capture_board.py --port "$PORT" --board A --seconds 60
```

原固件版本1和诊断版本2都能读。v2增加 startup_debug、init_stage_name、init_error_name、who_history、who_mismatches、spi_hz。WHO应112=0x70；错误板采样为零时capture仍保存诊断。核心Read报0x80需排IMU健康，不忽略；Ping仅代表串口协议通。

板号A/B/C由操作者分配，不能代替实际贴片批号；正常板先保留原始固件。日志独占创建且不覆盖历史。`scripts/analyze_log.py`做比较摘要，不凭摘要自动判定根因。

## 5. 三维控制台在Mac本机运行

```bash
bash scripts/start_macos.sh
open http://127.0.0.1:8765
```

网页输入板号A，选择实际U2D2端口、**DXL→J1/J2**、**标准Sync Read**，确认隔离测试后点击“连接实板·只读”。当前HSI-DXL不选UART1桥接。若只验证显示，主动点击DEMO，所有DEMO须保留模拟水印，不把模拟运动当板子通过。

工作台显示刚体板/小鸭姿态、gyro、重力、温度、seq/age、SPI/DXL错误以及v2启动阶段和WHO历史。启动失败时正常显示失败诊断；没有真实新样本时灰显最后姿态，零采样时温度显示未采样，不把默认25°C当测量。输入板号加入日志，A/B分别导出。无Node/npm/CDN运行依赖；Python从串口取数，浏览器不直接使用Web Serial，因此不要求Safari具备Web Serial。

前后/左右倾斜及三个轴的慢转应与实际姿态一致；只平移不会得到位置轨迹。按官方安装变换查看trunk，不把台架平放自动当直立。断开后关闭服务Ctrl+C；随后才能用同端口CLI。

## 6. 单板50Hz长测

先排除WHO和配置问题，再做完整压力；坏板不需要反复运行只接受healthy的soak：

```bash
STAMP=$(date -u +%Y%m%dT%H%M%SZ)
.venv/bin/python src/firmware/tools/imu_probe.py --port "$PORT" sample \
  --ids 200 --count 30000 --hz 50 --log "logs/A-soak-$STAMP.jsonl"
```

记录错误计数增量、采样新鲜度、USB端往返时延。主机p99含USB和macOS调度，不等于MCU响应延迟。未达到20ms软件门槛时保存失败日志并测总线波形，不先提高波特率/降低健康检查。

macOS/U2D2实板链路、Safari/Chrome实际USB采集尚需你现场执行；仓库软件测试、PTY和DEMO不是这些步骤的实測凭证。
