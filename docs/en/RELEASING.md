# Release files and packaging

[简体中文](../RELEASING.md)

Current release targets Bambuddy **1.2.5.7** and Waveshare Rev3.x.

| File | Purpose |
|---|---|
| `*.factory.bin` | Initial merged USB image at `0x0` / 首次安装合并镜像 |
| `spoolbuddy-*.bin` | OTA application / OTA 应用镜像 |
| `*.tf.zip` | Offline TF update / TF 离线更新包 |
| `SpoolBuddy-Windows-*.zip` | Simulator, configuration, launcher and licenses / 模拟器、配置与许可证 |
| `licenses.zip` | Firmware/dependency notices / 固件与依赖许可声明 |
| `release-manifest.json` | Board, SDK and validation metadata / 主板、SDK 与验证信息 |
| `SHA256SUMS.txt` | SHA-256 checksums / 校验和 |

Corresponding source is available through Source code on the same Release or its tag. Builds use ESP-IDF v6.1 and dependencies pinned in `dependencies.lock`. See [README](../../README.en.md) for build/test commands.

After firmware compilation, native interaction tests, isolated backend tests and hardware smoke checks, run `python tools/package_port.py`. It checks application identity, board, capacity and reports. Local `.tools/release-hardware.json` must match the application SHA-256.

After uploading the application, generate the OTA index with its actual asset URL:

```powershell
python tools/create_ota_manifest.py --build-dir build --asset-url "https://api.github.com/repos/tbc0309/spoolbuddy-esp32/releases/assets/ASSET_ID"
```

Public version identifies the Bambuddy baseline; `SB_FIRMWARE_BUILD` is the internal OTA revision. Images, manifest, checksums and source tag must agree. See [validation scope](HARDWARE_VALIDATION.md).
