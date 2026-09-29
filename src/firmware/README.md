> 新增DBG2字段、SPI_MHZ参数、错误路径和下载镜像请先读 [诊断v2说明](../../docs/FIRMWARE_DIAGNOSTICS.md)。官方0.15.0必须关闭Fast Sync Read，见 [兼容性](../../docs/MICRODUCK_COMPATIBILITY.md)。

# STM32G031 IMU-DXL首板候选固件

本次修改了启动诊断与SPI对照配置，重新编译并复测。不是原厂或完整DXL实现，也没有实板通过证据。

本包使用说明：[根README](../../README.md)；接线：[图中ST-LINK三线方案](../../docs/FIRMWARE_DIAGNOSTICS.md)；结果：[审计](../../reports/VALIDATION.md)。**本图探针没有可确认的VTref，7/8/9/10供电脚不接、J3.6不接此探针。**

构建：`make TOOLCHAIN=clang`（本次实测）或`make`（GNU Arm GCC路径，本次未执行）。`CLOCK=hse`选择有源16MHz bypass，`TRANSPORT=uart1`为独立J3数据版；默认FAULTS=0/WATCHDOG=0。旧CLI可作显式scratch/reboot测试，新Web后端只读且不暴露这些功能。

支持：单播Ping/Read、受限RAM Write、Reboot、标准SyncRead，ID200/1Mbps。未实现：广播Ping、Bulk/Fast、RegWrite/Action、全EEPROM控制表。私有模型0x7D00，不应套用XL330原厂控制表。

## 寄存器契约（全部小端）

|地址(十进制/十六进制)|长度|内容|
|---|---|---|
|0 / 0x00|2|私有型号0x7D00|
|6 / 0x06|1|本项目固件版本2|
|7、8、9|各1|ID200、baud编码3、return_delay=10(2µs单位)；只读|
|13、68|各1|protocol2、status_return_level2；只读|
|124 / 0x7C|6|gyro xyz int16原始计数，17.5mdps/LSB|
|130 / 0x82|6|SFLP xyz IEEE binary16，主控恢复w|
|136–143|8|保留0；**未声称复刻官方20字节诊断区后8字节**|
|256 / 0x100|4|ASCII IMU1|
|260 / 0x104|2|扩展ABI版本1|
|262 / 0x106|2|flags，定义见下|
|264 / 0x108|4|uptime_ms|
|268 / 0x10C|4|gyro_seq|
|272 / 0x110|4|quat_seq|
|276 / 0x114|4|gyro_age_ms，无样本0xFFFFFFFF|
|280 / 0x118|4|quat_age_ms，无样本0xFFFFFFFF|
|284 / 0x11C|4|SPI错误计数|
|288 / 0x120|4|FIFO溢出计数|
|292 / 0x124|4|UART硬件错误计数|
|296 / 0x128|4|RX软件环缓冲丢弃计数|
|300 / 0x12C|4|协议CRC错误计数|
|304 / 0x130|4|协议帧/长度/字节间隔错误计数|
|308 / 0x134|4|SyncRead前驱等待超时|
|312 / 0x138|4|TX硬件等待超时|
|316 / 0x13C|4|无效/全零四元数计数|
|320 / 0x140|6|原始accel xyz int16；±4g，0.122mg/LSB|
|326 / 0x146|2|原始温度int16，25+raw/256°C|
|328 / 0x148|1|WHO_AM_I|
|329 / 0x149|1|GPIO采样位0=INT1、1=INT2、2=DE、3=RX_EN|
|330 / 0x14A|1|clock_flags:bit0请求HSE、1实际HSE、2启动失败回退|
|331 / 0x14B|1|transport:0=DXL，1=UART1桥接|
|332 / 0x14C|4|系统频率Hz|
|336 / 0x150|8|CTRL1/2/3/6/8、FIFO_CTRL4、SFLP_ODR、EMB_FIFO_EN配置读回|
|344 / 0x158|4|启动时RCC_CSR复位原因原值，按STM32手册解码|
|348、349|各1|是否启用WATCHDOG、FAULTS|
|384 / 0x180|32|测试RAM可读写，重启清零|
|496 / 0x1F0|写3字节|仅FAULTS=1接受DE AD 01冻结/DE AD 00恢复；读取并不是命令回显|

flags位0=配置通过、1=见过gyro、2=见过有效quat、3=当前healthy、4=人为freeze、5=最近quat有效、6=初始化错误。只记录真实更新；冻结不修改核心值造假，但healthy清零。核心12字节任一重叠Read不健康时Alert，诊断仍正常应答。有效四元数只检查格式/范围，不替代方向与标度实测。

诊断扩展为本项目自定义，不改变官方124/12契约。原厂工具可能不认识私有模型；应使用本包单播/原始读工具。CRC坏包静默（不回应无可信ID的帧）；非法数据长度/地址/写权限返回对应错误。
