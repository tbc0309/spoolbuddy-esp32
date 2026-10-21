# Firmware releases and local builds

[简体中文](../RELEASING.md)

Releases contain firmware files for the target board only.

| File | Purpose |
|---|---|
| `*.factory.bin` | Initial USB installation at `0x0` |
| `spoolbuddy-*.bin` | OTA application image |
| `*.tf.zip` | Offline TF update package; extract to card root |

Source, licensing and dependency notices remain in the repository and version tag. TF firmware packages include license texts. Build the simulator from source; it is not a Release attachment.

`python tools/package_port.py` writes three firmware assets to `outputs/release/<version>/`. The simulator, license archive, metadata and checksums remain in the adjacent `<version>-local/` directory. Upload only the three firmware files.

Builds use ESP-IDF v6.1 and `dependencies.lock`. The public version follows the Bambuddy baseline; `SB_FIRMWARE_BUILD` is the internal OTA revision. Root `ota.json` points to the Release application image and records its size, SHA-256, board and revision.
