# 发布文件与构建

[English](en/RELEASING.md)

当前版本适配 Bambuddy **1.2.5.7**，目标主板为 Waveshare Rev3.x。

| 文件 | 用途 |
|---|---|
| `*.factory.bin` | Initial merged USB image at `0x0` / 首次安装合并镜像 |
| `spoolbuddy-*.bin` | OTA application / OTA 应用镜像 |
| `*.tf.zip` | Offline TF update / TF 离线更新包 |
| `SpoolBuddy-Windows-*.zip` | Simulator, configuration, launcher and licenses / 模拟器、配置与许可证 |
| `licenses.zip` | Firmware/dependency notices / 固件与依赖许可声明 |
| `release-manifest.json` | Board, SDK and validation metadata / 主板、SDK 与验证信息 |
| `SHA256SUMS.txt` | SHA-256 checksums / 校验和 |

对应源代码可从同一 Release 的 Source code 或版本标签获取。构建使用 ESP-IDF v6.1，组件由 `dependencies.lock` 固定。构建与测试命令见 [README](../README.md)。

完成主板构建、模拟器交互测试、后端隔离测试和硬件冒烟后，运行 `python tools/package_port.py`。打包器检查应用身份、主板、分区容量及报告。本地硬件记录 `.tools/release-hardware.json` 须对应待发布应用的 SHA-256。

上传应用后，以真实资产 URL 生成清单：

```powershell
python tools/create_ota_manifest.py --build-dir build --asset-url "https://api.github.com/repos/tbc0309/spoolbuddy-esp32/releases/assets/ASSET_ID"
```

公开版本对应 Bambuddy 基线，`SB_FIRMWARE_BUILD` 为 OTA 比较使用的内部修订。发布镜像、清单、校验和与源码标签需一致。验证范围见 [硬件验证](HARDWARE_VALIDATION.md)。
