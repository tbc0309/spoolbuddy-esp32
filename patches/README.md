# 本地补丁，不属于默认原版复刻

产品扩展默认关闭，通过构建选项启用。ESP-Hosted RX 依赖修复自动应用；完整清单见 `manifest.json`。

| 目录 | 内容 | 默认 |
|---|---|---|
| manual_weight | 手动总重输入和读数覆盖 | 关闭 |
| simulator_controls | Windows F3/F4 虚拟 NFC 输入 | 关闭 |
| confirm_actions | 本地额外增加的统一操作确认弹窗 | 关闭 |
| legacy_ui | 历史自定义界面、旧字库、旧宿主和打包脚本 | 归档，不参与任何构建 |
| macpit | macpit 参考内容的归属审查记录；当前无额外功能默认引入 | 不编译 |

Windows 补丁版：`./simulator/build.ps1 -LocalPatches`。默认版：`./simulator/build.ps1`，会显式把三个选项设为 OFF，避免复用缓存时意外混入扩展。固件可用 `tools/build_firmware.ps1 -LocalPatches` 显式选择手动重量和额外确认；虚拟 NFC 仅存在于 Windows。

补丁实现必须放在自己的目录中。公共层只提供稳定的可选接口和关闭时的空实现，不放扩展的状态、控件、键盘处理代码。上游已具备的功能不应仅因实现曾参考第三方而被当作新功能，例如库存搜索、材质筛选和颜色目录；对应来源见 `upstream-map.json`。

后续补丁应包含来源、目的、编译选项、默认值、与上游的差异及测试方法；先登记 manifest，再加入 `cmake/patches.cmake`，并同时验证默认版不泄漏扩展入口。
