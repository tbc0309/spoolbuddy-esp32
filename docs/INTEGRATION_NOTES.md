# Rev3.x 安装与接线

[English](en/INTEGRATION_NOTES.md)

支持机型为 Waveshare ESP32-P4-WIFI6-Touch-LCD-4.3 **Rev3.x**，32 MiB Flash，800×480 横屏。Rev1.3 仅保留历史参考，不提供本次发布镜像。

1. 首次安装使用 Releases 中该机型的 factory 合并镜像，从地址 `0x0` 烧录。
2. 将 `sdcard/spoolbuddy.example.json` 改为实际 Wi-Fi 与 Bambuddy 局域网地址，命名为 `spoolbuddy.json`，放到 TF 根目录。
3. 插卡启动，连接 Bambuddy 成功后配置自动确认并保存到独立 NVS。之后可无卡启动。
4. 后续更新使用应用镜像配合 OTA / TF 清单，不使用 factory 合并镜像。

| PN5180 信号 | P4 GPIO |
|---|---|
| SCK / MOSI / MISO | 2 / 3 / 4 |
| CS / BUSY / RST | 5 / 21 / 22 |

以上仅是当前板目录中的软件默认引脚，PN5180 与称重模块仍需实物接线和电气验收。不要把编译通过或模拟器传感器夹具当成外设验收。NAU7802 复用 BSP I²C，由 `sb_board` 提供总线；接线应按板卡原理图核对。
