# ESP-Hosted SDIO RX allocation retry

Dependency compatibility fix for ESP-Hosted 2.13.0, independent of the upstream
SpoolBuddy UI and board GPIO definitions. Applied after IDF resolves components.
The checked-in diff documents the exact two changes; `apply.py` checks anchors
and applies them idempotently. Dependency upgrades require reviewing the fix.

The RX loop clears NEW_PACKET before allocating the streaming receive buffer.
When allocation fails, it retains `pending`, but the following iteration rejects
the cleared interrupt bit and stops retrying unread bytes. Preserve the retry
flag across that check. Register/read errors still use the existing error paths.

Related upstream discussion: https://github.com/espressif/esp-hosted-mcu/issues/184
This code defect alone does not prove the cause of every Wi-Fi outage.
