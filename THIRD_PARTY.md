# Third-party notices / 第三方声明

This is an independent community port of Bambuddy's integrated SpoolBuddy, distributed under GNU AGPL-3.0-or-later. Third-party materials retain their original copyrights and licenses. See [LICENSE](LICENSE) and [NOTICE](NOTICE).

本项目为 Bambuddy 集成版 SpoolBuddy 的社区移植，采用 GNU AGPL-3.0-or-later；第三方代码、字体与图标保留原始版权及许可证。

| Source | Use and license |
|---|---|
| [Bambuddy / SpoolBuddy](https://github.com/maziggy/bambuddy) | UI structure, translations, logo, APIs and adapted PN5180/key-derivation/NAU7802 behavior. GNU AGPL v3; see root LICENSE. Reference commit `0d4275107a7349eb34019c7db3e809b20dc4a927`. |
| [Waveshare examples](https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-4.3) | Board/display reference at `dc1b2085381c9cc3a2badf8d317673cca9079c1f`. Managed BSP retains its own license. |
| [ESP-IDF](https://github.com/espressif/esp-idf) | v6.1 framework, primarily Apache-2.0 with component-specific notices. Dependency versions are pinned in `dependencies.lock`. |
| [LVGL](https://github.com/lvgl/lvgl) | LVGL 9.5.0, MIT. [License](assets/licenses/LVGL-MIT.txt). |
| [cJSON](https://github.com/DaveGamble/cJSON) | JSON parsing, MIT. [License](assets/licenses/cJSON-MIT.txt). |
| [Inter](https://github.com/rsms/inter) | Converted upstream font, SIL OFL 1.1. [License](assets/licenses/Inter-OFL.txt). |
| [Noto CJK](https://github.com/notofonts/noto-cjk) | Simplified Chinese font data, SIL OFL 1.1. [License](assets/fonts/OFL.txt). |
| [Montserrat](https://github.com/JulietaUla/Montserrat) | LVGL fonts, SIL OFL 1.1. [License](assets/licenses/Montserrat-OFL.txt). |
| [Font Awesome Free](https://fontawesome.com/license/free) | LVGL symbol fonts use SIL OFL 1.1; other formats retain the terms in the [upstream notice](assets/licenses/FontAwesome5.txt). |
| [Lucide](https://github.com/lucide-icons/lucide) | Wi-Fi-off SVG geometry from 0.555.0, ISC. [License](assets/upstream/lucide/LICENSE). |
| [MinGW-w64](https://www.mingw-w64.org/) | Simulator runtime/toolchain notices. [License collection](assets/licenses/MinGW-w64-COPYING.txt). |
| [macpit/spoolbuddy](https://github.com/macpit/spoolbuddy) | Native interaction and host architecture reference, MIT; commit `dca6cd61444a3cb7b5963606e204b83287ee3257`. Independent backend, CrowPanel firmware and Pico bridge are not included. |

Upstream provenance and hashes are recorded in `vendor/spoolbuddy/i18n/provenance.json`, `upstream-versions.json` and `upstream-map.json`. The original React/Python application is adapted to C/LVGL and ESP-IDF; board integration, storage and firmware updates are project modifications dated 2026-10-20.

The TF firmware package includes project notices and license files. Licensing and dependency notices are also available in the repository and matching source archive. Corresponding source/build scripts are available through the matching Release source archive and tag; SDK/dependency sources are resolved from pinned upstream locations. Names identify compatibility and provenance and do not imply endorsement.
