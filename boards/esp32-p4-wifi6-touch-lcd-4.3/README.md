# ESP32-P4-WIFI6-Touch-LCD-4.3

Waveshare 主板适配。公共业务、原版界面和网络协议位于项目根目录 src/，
此目录只保留该主板的硬件适配和配置。

- rev3/：当前支持并实测的 Rev3.x；显示、触摸、SD、Wi-Fi 已验证。
- rev1_3/：仅历史配置参考，未声明可直接构建。
- rev3/features/boot_sleep/：本板 BOOT 熄屏/唤醒，默认启用，保留 ROM 下载功能。

构建：tools/build_firmware.ps1 -Board esp32-p4-wifi6-touch-lcd-4.3/rev3

新增主板时在 boards/<新型号>/ 下新增适配，通过独立 board.cmake 选择
目标芯片、BSP、配置、分区和硬件功能。不要复制公共 UI 和业务逻辑。
