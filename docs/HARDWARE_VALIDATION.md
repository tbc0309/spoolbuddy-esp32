# 主板功能与运行方式

[English](en/HARDWARE_VALIDATION.md)

适用主板：Waveshare ESP32-P4-WIFI6-Touch-LCD-4.3 Rev3.x，32 MiB Flash，800×480 横屏。适配 Bambuddy 1.2.5.7。

| 功能 | 当前行为 |
|---|---|
| 显示与触摸 | LVGL 页面、底部导航及触摸操作 |
| 网络 | 板载 C6 Wi-Fi，连接 Bambuddy 获取库存、打印机和 AMS 数据 |
| 配置 | TF 首次导入，保存到板载 NVS，保留上一有效配置，支持无卡启动 |
| 更新 | HTTPS OTA 与 TF 更新共用镜像校验、非活动槽写入及启动确认流程 |
| 熄屏省电 | 超时与 BOOT 熄屏共用可选空闲轻睡眠，保留网络及页面状态 |
| 唤醒 | 触摸或短按并松开 BOOT；唤醒触摸不触发原页面控件 |
| 内存 | 界面及大缓冲优先使用 PSRAM，内部内存保留给必要分配 |
| NFC / 称重 | PN5180 / NAU7802 驱动与业务接口，需要对应外设和接线 |

轻睡眠保留运行时 CPU 频率、PSRAM 刷新和活动 GPIO 状态，不进入整机深睡眠。AP3032 的 EN 接电源，软件不能切断背光驱动供电。实际功耗取决于硬件配置和后台活动。

模拟器运行公共 LVGL 页面和业务逻辑，不模拟 ESP32 外设、电气行为或 Flash 写入。接线见[安装说明](INTEGRATION_NOTES.md)，更新流程见[OTA](OTA.md)。
