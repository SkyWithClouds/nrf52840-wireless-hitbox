# NOLOGO ProMicro nRF52840 · 两块实板记录

使用者在 #1 和 #2 的 UF2 引导盘上分别核对了以下字段。

| INFO_UF2.TXT 字段 | 两块板的实测值 |
|---|---|
| Bootloader | UF2 Bootloader 0.6.0 |
| Model | nice!nano |
| SoftDevice | S140 version 6.1.1 |

已确认应用起址 `0x26000`、UF2 Family ID `0xADA52840` 与这两块板匹配。使用者报告阶段 1 UF2 启动成功，Windows 自动绑定 Xbox 360 驱动。joy.cpl 依次观察到 A、A+B、右、LT、RT、左摇杆右下与右摇杆左下，最后一步显示 A，对应主机 1P 玩家灯命令。阶段 2 的无线和 SPI 验收单独记录。
