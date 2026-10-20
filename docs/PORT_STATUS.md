# 1.2.5.7 移植状态

[English](en/PORT_STATUS.md)

发布范围为 Waveshare ESP32-P4-WIFI6-Touch-LCD-4.3 Rev3.x。与 Bambuddy 1.2.5.7 的集成 SpoolBuddy 对照，使用原版资源、i18n 键和 API；界面用 LVGL 重建，尚不宣称所有动态状态逐像素一致。

| 范围 | 软件验证 | 实板验证 |
|---|---|---|
| 首页、AMS、写卡、库存、设置 | 生产控件交互、双语 28 状态截图 | 显示、触摸、切页已通过 |
| 库存、打印机、AMS 数据 | 原版 API 与隔离后端联调 | 实际读取两卷库存并注册/心跳 |
| NFC / 称重、去皮、校准、写卡 | 业务、驱动编译、请求与错误路径 | PN5180 / NAU7802 未接线验收 |
| Wi-Fi / 时间 / 语言 | 后端同步、配置优先级、时区处理 | C6 联网与心跳已通过短时测试 |
| TF 初次配置、NVS 保存及备份 | 日期比较、双银行和失败回退 | 保存成功后无卡重启通过 |
| TF 固件更新 | 镜像、大小、SHA-256、板型、版本校验 | 安装、重启、启动确认为 VALID |
| HTTPS OTA | 同一安装器、TLS、清单、凭据隔离 | 完整网络下载安装及失败回滚仍待验收 |
| BOOT 熄屏与触摸唤醒 | 板级独立实现，保留 ROM 下载 | 实板验证全黑、触摸恢复正常 |

默认不启用手动重量、虚拟 NFC 和额外确认等产品补丁。ESP-Hosted 兼容修复是构建必需的依赖修复；BOOT 功能及 PSRAM 配置属于当前主板。

- 当前硬件记录：[HARDWARE_VALIDATION.md](HARDWARE_VALIDATION.md)
- 界面截图：[SCREENSHOTS.md](SCREENSHOTS.md)
- 配置：[CONFIGURATION.md](CONFIGURATION.md)
- 版本与发布：[RELEASING.md](RELEASING.md)
