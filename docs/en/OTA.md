# Project OTA

[简体中文](../OTA.md) · [English documentation](../../README.en.md)

P4 Settings → Updates uses this project's OTA rather than Raspberry Pi SSH updates. Root `ota.json` indexes the project's matching Release application. No matching published board image results in an unavailable-release message; placeholder URLs/hashes are not used.

## Configuration

Keep existing Wi-Fi/Bambuddy fields in `spoolbuddy.json`; optionally add:

```json
"ota": {
  "manifest_url": "https://api.github.com/repos/tbc0309/spoolbuddy-esp32/contents/ota.json?ref=main",
  "github_token": ""
}
```

`manifest_url` defaults to the URL above. This repository is public; leave github_token empty. Private mirrors can use repository read credentials, persisted to NVS on initial import. Private images use `https://api.github.com/repos/tbc0309/spoolbuddy-esp32/releases/assets/ASSET_ID` with `Accept: application/octet-stream`; public images can use HTTPS download URLs.

Credentials are sent only to exact GitHub API/GitHub/raw.githubusercontent.com hosts and are not forwarded to redirected asset CDNs. TLS uses the system CA bundle; HTTP downgrade and certificate bypass are rejected.

## Publishing and installation

1. Keep the public version equal to the Bambuddy baseline. Increment the actual firmware's internal revision, for example `idf.py -DSB_FIRMWARE_BUILD=6 build`; changing only manifest `build` is insufficient.
2. Test the target board and upload `build/spoolbuddy_port.bin`. Never use the merged bootloader/partition image for OTA.
3. Generate the index from the real asset URL:

```powershell
python tools/create_ota_manifest.py --build-dir build --asset-url "https://api.github.com/repos/tbc0309/spoolbuddy-esp32/releases/assets/ASSET_ID"
```

4. The tool reads board, version, revision, size and SHA-256 and updates root `ota.json`, preserving other boards. Review and synchronize it.
5. Checking for updates automatically downloads/installs a newer version or revision. An up-to-date result only displays status. The separate TF entry permits same-version reinstall but rejects downgrades. Installation writes the inactive slot and verifies size, SHA-256, ESP image structure, project identity, version and embedded board/revision before selecting the next boot slot.

## Partitions and recovery

Rev3 retains NVS at 0x9000 and factory at 0x10000, with a 6 MiB factory slot. `otadata` is at 0x610000; two 12 MiB OTA slots start at 0x620000 and 0x1220000. The 64 KiB `sb_config` partition starts at 0x1E20000; total fits within 32 MiB.

Partition migration requires USB installation of bootloader, partition table, factory and otadata. Normal OTA cannot migrate layout.

First boot confirms the new image after display initialization, Wi-Fi and Bambuddy registration. If unconfirmed, rollback is requested after 180 seconds, allowing reconnection after the two-minute configuration fallback. An abnormal reboot before confirmation also triggers bootloader rollback. Keep Bambuddy reachable while updating. Retain USB recovery for older factory images without rollback support.

SHA-256 verifies integrity against the HTTPS manifest; it is not an independent signature. Secure Boot and signing-key provisioning are not enabled.

The Windows simulator displays update controls without writing Flash. Firmware installation runs on the board.

## Language and TF updates

There is no extra language selector in SpoolBuddy settings. With `language` omitted, follow Bambuddy General → Language: Chinese variants become Simplified Chinese, all others English. Explicit configuration overrides backend; startup defaults to Chinese and later disconnection retains the last language.

```powershell
python tools/create_ota_manifest.py --build-dir build --sdcard-dir X:/
```

This writes the actual application and verified `ota.json` to the TF root without a GitHub URL. Insert the card and select Settings → Updates → Update from TF card. TF and HTTPS share inactive-slot installation, verification, boot confirmation and rollback. Path traversal, merged images and downgrades are rejected. UI displays only the public version; hidden revision controls updates.

See [release policy](RELEASING.md). Protocol references: [GitHub Contents API](https://docs.github.com/en/rest/repos/contents), [Release assets API](https://docs.github.com/en/rest/releases/assets).
