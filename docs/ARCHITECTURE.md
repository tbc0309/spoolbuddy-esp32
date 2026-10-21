# 公共实现、平台、机型和补丁边界

[English](en/ARCHITECTURE.md)

本项目以 C/LVGL 和 ESP-IDF 实现 SpoolBuddy 的嵌入式客户端，公共业务、平台服务、主板适配及可选扩展分别维护。

```text
src/
  core/                    配置解析、API 模型/队列、校准数学、统计序列化
  ui/                      原版页面的 LVGL 实现和实时状态
  ports/                   显示、板级服务和可选扩展的接口
  platform/esp_idf/         ESP-IDF 任务、HTTP、真实硬件统计采集
  drivers/esp_idf/          PN5180、NAU7802 的 ESP-IDF 驱动
boards/
  esp32-p4-wifi6-touch-lcd-4.3/
    rev3/                  唯一当前可构建的硬件机型
      board.cmake          芯片目标和板组件位置
      sdkconfig.defaults   私有引脚、PSRAM、C6 SDIO、Flash 配置
      partitions.csv       本机型的分区
      sb_board/            私有 BSP、屏幕旋转、I²C/SD、依赖 manifest
      features/boot_sleep/ 本板 BOOT 按键、消抖及测试，主板默认开启
      features/power_save/ 本板熄屏省电设置、BSP 生命周期修复及说明
    rev1_3/                历史配置参考，不声明为已支持机型
patches/                   默认关闭的产品扩展，以及独立依赖兼容修复
simulator/                 Windows 宿主、构建和原生交互测试
tests/                     ESP32 统计、目录边界及真实后端隔离测试
cmake/                     公共源文件清单和补丁选择
main/                      ESP-IDF 注册入口、公共 Kconfig/依赖，无板级实现
upstream-map.json          对应的上游文件、提交及内容哈希
vendor/spoolbuddy/         锁定提交的上游语言资源和页面默认文案，只作快照
locales/                   本地补译、平台专用文案，不修改上游资源
```

公共 `src/core`、`src/ui` 不包含 BSP 头或具体引脚。ESP-IDF 平台可依赖 SDK，但只能通过 `ports/board.h` 调用板服务。板依赖留在该机型的 `sb_board/idf_component.yml`；只有选中的板组件进入构建。分区、芯片最低修订版、PSRAM 和 Wi-Fi 协处理器均归机型，不放到公共默认值。

主板硬件功能放在机型目录中：BOOT 休眠实现属于当前板的 features，
由 board.cmake 选择源文件和编译定义，默认开启，可用 SB_ENABLE_BOOT_SLEEP=OFF 关闭。
公共 UI 仅调用能力接口，模拟器和未来其他主板不会编译这块板的 GPIO 处理。
patches 继续只存与主板无关的非原版产品扩展，例如手动称重。

本板的[熄屏省电能力](../boards/esp32-p4-wifi6-touch-lcd-4.3/rev3/features/power_save/README.md)通过 `optional_power.h` 接口接入初始化和显示设置；公共代码不处理具体休眠时序、触摸轮询或显示硬件生命周期。

## 新增机型

1. 新建 `boards/<型号>/<修订版>/`，提供 `board.cmake`、`sdkconfig.defaults`、分区以及名为 `sb_board` 的组件。
2. 实现 `src/ports/board.h` 和 `ui_platform.h` 中的板服务，不在公共页面增加型号分支。
3. `board.cmake` 声明 SB_BOARD_TARGET、SB_BOARD_COMPONENT_DIR、SB_BOARD_DEFAULTS；依赖放私有 manifest，引脚通过本机型 sdkconfig 设置。
   主板额外能力可声明 SB_BOARD_FEATURE_SOURCES 和 SB_BOARD_FEATURE_DEFINITIONS，由公共入口选择编译。不要复用另一主板的引脚或默认开启它的 BOOT 处理；无此能力时保持接口空实现。
4. 使用独立构建目录和 sdkconfig。更换机型复用构建目录会被拒绝，避免缓存中的引脚/芯片配置污染另一块板。
5. 测试公共序列化、驱动、界面与后端接口，再单独进行新机型的真实显示/触摸/联网/外设验收。不复制另一套业务代码。

## 原版与补丁

手动重量、虚拟 NFC 热键和额外确认弹窗位于独立扩展目录。原版也有的搜索、颜色目录和校准仍是公共功能。正确的校准单位、整数克提交及防止选错耗材属于实现原版语义所需的正确性修复，不应默认关闭。

ESP32 系统统计属于必要的平台适配：用芯片核心数、内存/PSRAM、SDK 信息、运行时间和已挂载 SD 信息替代 Pi 的 Linux 探测。不伪造 `/proc`、Linux loadavg 或把裸 Flash 容量当作根文件系统。

上报只保留原版网页消费的 CPU/温度、memory、disk、os、system_uptime_s。
不增加 hardware、network、Flash 分区、任务数、低水位或 build 标识到统计载荷。
配置记录格式和时间/语言策略属于公共 core；NVS 读写在 ESP-IDF 平台；配置分区名、
位置及大小由各自 boards 提供。详见 CONFIGURATION.md。

更新语言资源后运行 tools/generate_i18n.py 生成 LVGL 字典；不得重新把中英文条件
文字写回控件。上游语言快照、本地补译、公共控件和板级 BSP 四层独立更新。

OTA 的清单解析与版本比较放在 src/core/ota_manifest，更新能力接口放在
src/ports/update.h，HTTPS/Flash/启动确认实现放在 src/platform/esp_idf/update_esp.c。
分区布局仍由各主板自行提供；不复制主板 GPIO 到 OTA 或公共 UI。
根目录 ota.json 只记录发布元数据，不包含访问凭据；镜像位于后续 Releases。
模拟器使用明确的不可刷写实现，测试生产解析与更新入口，不伪装完成 ESP32 OTA。

## 跟进上游

`upstream-versions.json` 记录版本；`upstream-map.json` 记录相关源码哈希。获取新版参考后运行：

```powershell
python tools/check_upstream_changes.py --source upstream/bambuddy
```

工具只报告需复核的 UI/API/硬件源码，不覆盖移植代码，也不自动启用补丁。根据变更分类更新公共适配，运行原生和后端测试，最后经审查更新基线哈希。机型私有文件和本地补丁分别复验，避免把旧扩展重新揉进原版实现。

原版 Linux 测试文件保持上游原样。ESP32 统计由 SDK 采集器和公共序列化器提供；对应测试位于 `tests/platform/test_esp32_system_stats.py`。

上游 React/Python 与嵌入式 C/LVGL 属于不同实现，变更检查用于定位需要复核的来源，不承诺跨语言页面的一键自动合并。原版更新先适配公共层，再单独复验各机型和 opt-in 补丁。

`patches/esp_hosted_rx_retry` 是 ESP-Hosted 2.13.0 的依赖兼容修复，区别于默认关闭的产品扩展：固件配置阶段自动应用，修复 SDIO 接收分配失败后的重试条件。它不修改上游 SpoolBuddy 资源，也不包含机型 GPIO。依赖升级后必须重新复核；补丁脚本遇到未知源码结构会终止构建。

Rev3 的 SDIO 缓冲优先使用 PSRAM 属于硬件配置，单独放在该主板 `sdkconfig.defaults` 的 `CONFIG_ESP_HOSTED_MEMPOOL_PREFER_SPIRAM`，不默认推广到其他主板。主板的 TF 扇区缓冲仍在板级 SD 初始化时预留内部 DMA 内存。

通用分配策略位于 `src/platform/esp_idf/memory_esp.c`：LVGL 使用其支持的自定义分配接口，cJSON 在首次解析前安装同一分配策略，HTTP / WebSocket / 配置 / OTA 大缓冲显式优先 PSRAM，失败时回退默认堆。无 PSRAM 的目标回退普通分配。Rev3 额外配置 TLS、Wi-Fi/LwIP 和普通大块分配优先外部内存，内部预留 64 KiB 给必要的底层分配；不把 ISR、任务栈及特定硬件 DMA 缓冲强行移到外部内存。
