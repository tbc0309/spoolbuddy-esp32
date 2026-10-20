# 上游数据快照

此目录只保存从锁定 Bambuddy 提交提取的原始语言资源及页面默认文案，不在这里补译。
来源提交、文件和 SHA-256 位于 i18n/provenance.json；许可证为项目的 AGPL-3.0。

更新上游后运行 node tools/sync_upstream_i18n.cjs，重新提取字典及 React 的 i18n
默认文字。该工具不执行组件，也不修改上游检出目录。

locales/en.json、locales/zh-CN.json 保存平台专用文字和缺失补译；生成器按
“本地明确补充 → 原始字典 → 原版页面英文默认值”选择资源。src/ui 只引用键，
assets/i18n_catalog.h 是生成物，不手工修改。
