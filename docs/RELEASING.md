# 固件发布与本地构建

[English](en/RELEASING.md)

Release 仅包含目标主板的固件文件。

| 文件 | 用途 |
|---|---|
| `*.factory.bin` | 首次 USB 安装，从 `0x0` 烧录 |
| `spoolbuddy-*.bin` | OTA 应用镜像 |
| `*.tf.zip` | TF 离线更新包，解压到卡根目录 |

源码、协议和依赖声明保存在仓库及版本标签中。TF 固件包包含许可证文本。模拟器从源码构建，不作为 Release 附件。

`python tools/package_port.py` 将三项固件输出到 `outputs/release/<版本>/`，模拟器、许可证包、元数据和校验报告保留在相邻的 `<版本>-local/` 本地目录。只上传固件目录中的三个文件。

构建使用 ESP-IDF v6.1，依赖由 `dependencies.lock` 固定。公开版本与 Bambuddy 基线一致，`SB_FIRMWARE_BUILD` 为内部 OTA 修订。根目录 `ota.json` 指向 Release 中的应用镜像；更新清单与镜像大小、SHA-256、板型及内部修订保持一致。
