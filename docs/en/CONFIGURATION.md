# Configuration import, confirmation and backup

[简体中文](../CONFIGURATION.md) · [English documentation](../../README.en.md)

Place `spoolbuddy.json` in the TF card root for initial setup. At boot, the firmware validates JSON, fields, size and file modification time. It confirms and persists the candidate only after connecting successfully to Wi-Fi and Bambuddy. Subsequent boots can use onboard configuration without a card.

The dedicated `sb_config` NVS partition holds two complete records with format version, increasing generation and source modification time. A new record is committed to the other bank and read back for verification, retaining the previous valid bank. If one bank is corrupt, the other remains available; the partition is not erased.

- A newer TF file with changed content is tried and saved after a successful connection.
- FAT timestamps have two-second resolution; changed content with an equal timestamp is also detected.
- Identical content avoids repeated Flash writes. Older files do not overwrite confirmed configuration.
- Missing or invalid TF configuration falls back to the confirmed record.
- If a candidate cannot connect within two minutes, the previous confirmed configuration is restored and reconnection starts. Unconfirmed data is not persisted as valid.
- With no previous record, keep the initial TF file available for correction; a failed connection is not recorded as success.

The date is the filesystem modification time; no date field is required in JSON. Checks occur at boot, so restart after changing the card through a reader.

Do not add `language` by default. When omitted, the UI follows Bambuddy's main language setting. An explicit value overrides the backend. Chinese variants including `zh`, `zh-CN`, `zh-TW`, `zh-HK`, `zh-Hans` and `zh-Hant` map to Simplified Chinese; all others use English. Before the backend supplies a language, the startup default is Simplified Chinese.

Optional `timezone` uses POSIX TZ syntax and defaults to `CST-8` (UTC+8), independently of language; `UTC0` selects UTC. If NTP is unavailable, Bambuddy's HTTP Date can initialize UTC. Local timezone is applied for display; 12/24-hour format follows backend `time_format`.

See `boards/esp32-p4-wifi6-touch-lcd-4.3/rev3/partitions.csv` for storage placement. Application updates preserve this partition; changing partition layouts requires USB installation.
