# 单板无线 Hitbox

18 个常开按键、一块 NOLOGO ProMicro nRF52840。插数据线可在 Windows 用 XInput 玩游戏，同时在浏览器改键；拔线后，同一套 SOCD、方向设置和映射通过 2.4 GHz ESB 发给现有 v1.0 接收器。固件采用 MIT 许可证。

[打开 WebHID 配置台](https://skywithclouds.github.io/nrf52840-wireless-hitbox/) · [固件构建与下载](https://github.com/SkyWithClouds/nrf52840-wireless-hitbox/actions/workflows/build.yml) · [Pages 部署记录](https://github.com/SkyWithClouds/nrf52840-wireless-hitbox/actions/workflows/pages.yml)

## 三个仓库，四种组合

| 发射端 ↓ / 接收端 → | A · Windows PC 迷你接收器 | B · GP2040-CE 主机接收盒 |
|---|---|---|
| A · 成品 GP2040-CE Hitbox 外挂 | ① Pico Host + ② radio-tx → ④ usb-rx | ① Pico Host + ② radio-tx → rx-bridge + ⑤ gp2040-rx |
| B · 本仓库单板 18 键 | ③ hitbox-nrf52840 → ④ usb-rx | ③ hitbox-nrf52840 → rx-bridge + ⑤ gp2040-rx |

编号对应五份核心固件。接收端 B 的 NOLOGO #2 还需单独刷其配套的 **rx-bridge**。PS4/PS5 等主机模式与实体认证器由接收盒处理，PS5 游戏须支持相应街机摇杆模式。[外挂发射端和 PC 接收器](https://github.com/SkyWithClouds/gp2040-wireless) · [主机接收盒与认证器接线](https://github.com/SkyWithClouds/gp2040-wireless-console)


## 无线匹配与双打（v1.1 预览）

出厂网络 ID 为 `B74632DA`，信道 40，速率 2 Mbps。槽位 1 在本机使用 Pipe 1，其基础地址与前缀 C5 保留旧 Pipe 0 的空口地址，原版 v1.0 板可继续配合使用。Slot 2–4 需要升级对应发射板和 PC 接收器。玩家槽位决定无线输入路由到哪个 XInput 接口；Windows 和游戏自行分配控制器序号。

1. 在桌面 Chrome / Edge 用 USB 数据线连接要配置的无线板，打开[WebHID 配置台](https://skywithclouds.github.io/nrf52840-wireless-hitbox/)。修改外挂发射板或主机盒无线桥前，断开板间供电和信号线，单独给 NOLOGO 插 USB。
2. 在「无线匹配与接收器管理」生成一次网络 ID，给同一组设备填写相同的八位 ID 和信道。信道 2–80 对应 2402–2480 MHz。不同比赛台可分别选 ID；同频设备仍共享空口时间。
3. PC 接收器默认启用 2 个槽位，可设 1–4。两台发射端分别选 1、2。主机盒固定使用槽位 1，允许修改 ID、信道。每台保存后等待回读，再完全断电重启；单板 Hitbox 拔线进入无线即加载已保存的设置。
4. PC 接收器网页每约 150 ms 显示每槽状态、收到的有效帧、漏收心跳、重复输入和超时次数，计数从开机开始。漏收心跳按同一会话的心跳编号缺口累计，真实射频丢包率需借助空口测量。
5. 双打实测时让两人同时点按，按住 1P 后断开 1P 电源，检查 1P 释放、2P 仍可操作。交换角色重复，再测修改信道、恢复出厂参数和断电保存。USB 枚举、射频吞吐及实际输入延迟待实板验证。

## 接线与刷机

每个按键一端接表中 GPIO，另一端接 GND，按下为低电平。板上启用内部上拉。省略的排针包括连接地、电源或复位的脚，请按实板 Pin 1 标记数脚。

| 键 | NP1 排针 | GPIO | 出厂动作 | 键 | NP2 排针 | GPIO | 出厂动作 |
|---:|---:|---|---|---:|---:|---|---|
| 1 | 2 | P0.06 | 左 | 11 | 1 | P0.09 | RT |
| 2 | 3 | P0.08 | 下 | 12 | 2 | P0.10 | LT |
| 3 | 6 | P0.17 | 右 | 13 | 3 | P1.11 | Back |
| 4 | 7 | P0.20 | 上 | 14 | 4 | P1.13 | Start |
| 5 | 8 | P0.22 | X | 15 | 5 | P1.15 | LS 点击 |
| 6 | 9 | P0.24 | Y | 16 | 6 | P0.02 | RS 点击 |
| 7 | 10 | P1.00 | RB | 17 | 7 | P0.29 | Guide |
| 8 | 11 | P0.11 | LB | 18 | 8 | P0.31 | AUX |
| 9 | 12 | P1.04 | A | | | | |
| 10 | 13 | P1.06 | B | | | | |

AUX 对应生态按钮位 11，Windows 游戏对该保留位的支持各异；可在网页重新分配。NP3 的 P1.01、P1.02、P1.07 留作扩展。P0.13 输出低，关闭外部 EXTVCC。电池使用板厂支持的单节 3.7 V 锂电池，正负极接 B+ / B−；充电由板载电路管理。**严禁把 USB 5 V 接 BAT 或 GPIO。** [板厂引脚与供电说明](https://www.nologo.tech/product/otherboard/NRF52840.html)

1. 在 **Actions → Hitbox CI** 打开同一提交的成功运行，下载并解压 Artifact。使用其中的 `hitbox-nrf52840-nologo-26000.uf2`。
2. 用 USB 数据线连接发射板，把 RST 与 GND 快速短接两次，进入 UF2 引导盘。核对 `INFO_UF2.TXT` 的 Bootloader 0.6.0、nice!nano、S140 6.1.1，拖入 UF2。启用 NFC 引脚作 GPIO 的首次启动可能自动复位一次。
3. Windows 打开 `joy.cpl` 测有线输入。保持 USB 连接，用桌面 Chrome 或 Edge 打开配置台，点击「连接设备」并选择发射板。网页连接的是 Interface 1，游戏使用 Interface 0。
4. 点击面板上的物理按键后选择动作；选项卡切换编辑对象，「启动配置档」决定保存后启用的配置。点击「保存到设备」，等写入和回读完成。JSON 导入后也需保存。映射为「下一配置」的实体键可临时循环切换，松开所有键后即可再次切换；下次开机使用保存的启动配置。
5. 装电池、拔掉 USB，给一台接收器通电。测试另一种组合时关闭上一台接收器。充电宝或纯充电线也会拉高 VBUS，此时板子进入有线模式。

四种 SOCD 为中立、上优先（左右仍中立）、后输入优先、先输入优先。同一消抖扫描内同时按下的相反方向没有先后，先/后优先模式输出中立。消抖为各键独立 1–10 ms，默认 2 ms。方向可输出 D-Pad、LS 或 RS。多个物理键可以映射到同一动作，四个配置档在一次 NVS 事务中保存。

## 电池与休眠

内部 SAADC 采 P0.04/AIN2。不同批次可能未装该采样支路；确认本板分压比后，可在网页设置电压倍率（电池电压除以 ADC 引脚电压）。默认未校准时，只在电池供电期间从 VDDH/5 估算。插 USB 后展示最近一次拔线读数；USB 冷启动无历史值时显示「待测」。百分比按单节锂电电压估算，充电或负载会影响读数。[板级差异与原理图](https://github.com/sasodoma/nrf52840-promicro)

无线连续 5 分钟没有物理按键活动即停止 ESB、释放高频晶振并进入 **System ON 无滴答低功耗休眠**，CPU 阻塞等待 GPIO PORT/SENSE 或 USB 电源中断，保留 RAM。按住任一键继续游戏时保持唤醒。此选择可避免唤醒后重跑 UF2 引导程序。GPIO 唤醒、重新建立无线会话和主机收到首份报告各有独立耗时，实测时请分别记录。

## 协议、存储与构建

无线基线为原仓库的提交 `1967a24c27c0dbc7fd60f2adc065cb4a09c3a980`。协议编解码、Pico 源状态机和 PTX 滑动窗口直接复用。在同一 MCU 上把本地快照送入两段状态机，RF 数据保持 24 字节、64 位 session_id、CRC-8/SMBUS、出厂地址和默认 RF_CH 40、2 Mbps。保留原八类消息、32 帧源队列、8 帧窗口、usb_done 累计确认、1–2 ms 主动轮询、10 ms 心跳和 50 ms 看门狗。无线 ACK 中的玩家灯仍由状态机接收；单板没有四盏玩家灯，心跳不会伪报已点灯。[v1.0 规范](https://github.com/SkyWithClouds/gp2040-wireless/blob/1967a24c27c0dbc7fd60f2adc065cb4a09c3a980/docs/PROTOCOL_SPEC.md)

有线复合设备使用样机 VID/PID `1209:0003`。Interface 0 为 `XUSB10`，IN `0x81`（1 ms）、OUT `0x02`；Interface 1 为 HID Usage Page `0xFF60`，64 字节 Feature 报告双向配置，64 字节 Input 报告提供实时按键和电池遥测。出货前需使用产品自己的 USB 标识并同步修改网页过滤条件。

应用范围 `[0x26000,0xE8000)`，自建 NVS 占 `[0xE8000,0xEC000)`，引导程序原有的保留区从 `0xEC000` 开始。反复刷本固件会保留 NVS。UF2 使用 Family ID `0xADA52840`。SDK 依赖由 west 清单固定为 nRF Connect SDK v2.9.2。

- `scripts/test.sh` 使用 C 编译器和 Node.js 22 运行线格式、消抖、SOCD、配置事务，以及与两个现有接收器的互通测试。模拟链路包含丢包、ACK 丢失、停滞和重连。接收盒基线为 `9857d79a9078d2160d0cb4344fb29a4af9d2392e`。
- `.github/workflows/build.yml` 在 Nordic 工具链容器中运行 west，逐块比对 UF2 和链接后的二进制，检查目标地址、家族和复位向量。
- `.github/workflows/pages.yml` 部署 `web-configurator/`。本地预览可运行 `python3 -m http.server 8000 --directory web-configurator`，在浏览器打开 `http://localhost:8000`。

## 实板验收

- [ ] 先用 USB 同时打开 joy.cpl 和配置台，逐一按 18 键，核对引脚与网页高亮，再测快速点按、A+B 同按和长按。特别核对已释放 NFC 功能的 P0.09/P0.10。
- [ ] 分别测左→右、右→左、上下同按和松开一侧，核对四种 SOCD；将方向分别切到 LS、RS，核对轴和符号。
- [ ] 保存四个不同配置，完全断电再上电，核对启动档、映射、消抖、JSON 往返。网页应在有线游戏中持续显示按键。
- [ ] 分两次接 PC 迷你接收器和主机接收盒。每次测试快速点按、同时按、蓄力并在长按时断开发射板电源。重连只同步当前状态。主机盒先在 PC XInput 测，再按其说明接实体认证器与目标游戏。
- [ ] 用电池测试有线/无线来回切换；无线空闲超过 5 分钟后分别用各键、再用插 USB 唤醒。用同一时基记录 GPIO、首个有效无线包与主机输入，测待机电流。核对网页电压来源并与电池实测值比较。

自动化测试和云端构建覆盖软件路径。此款复合 USB 在 Windows 的实际驱动绑定、两种组合的真实射频表现、睡眠电流及首键延迟待实板验收。
