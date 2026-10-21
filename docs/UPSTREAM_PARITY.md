# 原版功能与适配边界

[English](en/UPSTREAM_PARITY.md)

基准：upstream-versions.json 的 Bambuddy 提交。原始 React 页面在
upstream/bambuddy/frontend/src/pages/spoolbuddy，控件在 components/spoolbuddy。
此检出目录为参考，不混入移植修改；vendor/spoolbuddy 是可追溯的资源快照。

| 原版入口 | 公共移植实现 | 当前实现 |
| --- | --- | --- |
| 首页 /spoolbuddy | src/ui/ui_original.c | 标签、耗材、称重状态、链接和同步 |
| /ams | ui_original.c、ui_ams_config.h | AMS 槽位、关联、配置和 PA 选择 |
| /write-tag | ui_original.c、ui_form_catalog.h | 三种写入模式、表单、耗材目录和 PA 功能 |
| /inventory | ui_original.c、ui_inventory_detail.h | 库存、搜索、详情、编辑、分配和解绑；保留用户名称及后端错误文本 |
| 设置 Device/Display/Scale | ui_original.c | 设备与显示设置、当前重量、偏移和校准系数；独立校准入口由 LVGL 弹窗承载 |
| 设置 Updates | ui_updates.h + ports/update.h | HTTPS OTA 与 TF 更新替代 Pi SSH 更新 |
| 设置 System | ui_system.h + core/system_stats.c | 原网页卡片与字段，提供 ESP32 统计；不提供 Linux loadavg 或 Python 信息 |
| 顶栏、底栏、快捷菜单 | ui_original.c | 原始图标资源、i18n 键和时间格式；按平台能力提供快捷操作 |

控件通过 i18n 键读取文本。原字典和 React 的英文默认文案按提交
保存于 vendor，缺失中文及平台专用内容仅补充到 locales。底部已显示的已知状态
提示也会随语言切换。嵌入式页面保留原版交互结构，按屏幕与平台能力适配布局。

## 后续上游更新顺序

1. 更新锁定参考提交，并运行 tools/check_upstream_changes.py 识别涉及的原版文件。
2. 更新 vendor 的语言快照，审查 locales 补译是否仍需要，再生成 i18n_catalog.h。
3. 在公共 UI/core 适配业务变化；不要复制到每个主板目录。
4. 对每个 boards 配置分别编译。板级 BSP、引脚、显示/触摸、分区和硬件功能留在 boards。
5. 运行原生交互、目录边界、配置/更新、后端隔离测试，再做各主板实物验收。

手动输入重量等非原版产品扩展仍位于 patches，默认关闭。NVS 配置、时区和
OTA 属于平台适配；BOOT 熄屏属于具体板级功能，不混进上游资源快照。
