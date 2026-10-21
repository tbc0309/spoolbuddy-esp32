# 功能范围 · 1.2.5.7

[English](en/PORT_STATUS.md)

面向 Waveshare ESP32-P4-WIFI6-Touch-LCD-4.3 Rev3.x，使用 Bambuddy 1.2.5.7 集成 SpoolBuddy 的资源、i18n 键和 API，以 LVGL 实现嵌入式界面。

| 范围 | 当前实现 |
|---|---|
| 页面 | 首页、AMS、写卡、库存和设置 |
| 数据 | 库存、搜索、详情、编辑、分配、解绑、打印机与 AMS 槽位 |
| NFC / 称重 | 读写标签、重量、去皮和校准；需要连接对应外设 |
| Wi-Fi / 时间 / 语言 | 板载 C6 联网、后端同步、配置优先级及 POSIX 时区 |
| 配置 | TF 导入、NVS 双槽保存、日期比较和失败回退 |
| 更新 | HTTPS OTA、TF 更新、镜像和板型校验、启动确认 |
| 熄屏 | BOOT 与超时熄屏、可选空闲轻睡眠、触摸或 BOOT 唤醒 |

手动重量、虚拟 NFC 和额外确认等产品补丁默认关闭。ESP-Hosted 兼容修复属于构建依赖适配；BOOT、熄屏省电和 PSRAM 配置属于当前主板。

[主板功能](HARDWARE_VALIDATION.md) · [截图](SCREENSHOTS.md) · [配置](CONFIGURATION.md) · [发布文件](RELEASING.md)
