# 原版功能与适配边界

[English](en/UPSTREAM_PARITY.md)

基准：upstream-versions.json 的 Bambuddy 提交。原始 React 页面在
upstream/bambuddy/frontend/src/pages/spoolbuddy，控件在 components/spoolbuddy。
此检出目录为参考，不混入移植修改；vendor/spoolbuddy 是可追溯的资源快照。

| 原版入口 | 公共移植实现 | 当前核对情况 |
| --- | --- | --- |
| 首页 /spoolbuddy | src/ui/ui_original.c | 标签、耗材、称重状态、链接与同步操作有原生回调测试；真实 NFC/称重仍需接外设验收 |
| /ams | ui_original.c、ui_ams_config.h | 槽位、关联、配置及 PA 选择使用原版接口，有隔离回归；实机打印机动作未全部物理验收 |
| /write-tag | ui_original.c、ui_form_catalog.h | 三种写入模式、表单、目录和 PA 功能有模拟器及真实隔离后端测试；实际写标签仍需外设 |
| /inventory | ui_original.c、ui_inventory_detail.h | 库存、搜索、详情、编辑、分配和解绑有回归；用户自定义名称和后端错误不强制翻译 |
| 设置 Device/Display/Scale | ui_original.c | 显示和校准接口已接入；补齐当前重量、偏移、系数。原版独立 /calibration 页面仍由 LVGL 校准弹窗承载，布局尚非逐像素一致 |
| 设置 Updates | ui_updates.h + ports/update.h | 平台必要差异：本项目 HTTPS OTA / TF 更新替代 Pi SSH；真实安装与回滚须单列实板记录 |
| 设置 System | ui_system.h + core/system_stats.c | 按原网页卡片与字段；FreeRTOS 不伪造 Linux loadavg/Python。仅上报原网页使用的字段 |
| 顶栏、底栏、快捷菜单 | ui_original.c | 原始图标资源、i18n 键、时间格式；Pi 服务/浏览器动作不能直接等同 MCU 行为，按能力处理 |

控件通过 i18n 键读取文本。原字典和 React 的英文默认文案按提交
保存于 vendor，缺失中文及平台专用内容仅补充到 locales。底部已显示的已知状态
提示也会随语言切换。不得将本表理解为已完成所有实物测试或全部像素级一致。

## 后续上游更新顺序

1. 更新锁定参考提交，并运行 tools/check_upstream_changes.py 识别涉及的原版文件。
2. 更新 vendor 的语言快照，审查 locales 补译是否仍需要，再生成 i18n_catalog.h。
3. 在公共 UI/core 适配业务变化；不要复制到每个主板目录。
4. 对每个 boards 配置分别编译。板级 BSP、引脚、显示/触摸、分区和硬件功能留在 boards。
5. 运行原生交互、目录边界、配置/更新、后端隔离测试，再做各主板实物验收。

手动输入重量等非原版产品扩展仍位于 patches，默认关闭。NVS 配置、时区和
OTA 属于平台适配；BOOT 熄屏属于具体板级功能，不混进上游资源快照。
