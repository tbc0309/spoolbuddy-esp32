# 本项目 OTA

[English](en/OTA.md)

更新入口是 P4 的“设置 → 更新”，不调用 Raspberry Pi 的 SSH 更新。
根目录 ota.json 是发布索引，指向本项目对应版本的 Releases 应用镜像。
没有匹配主板的已发布镜像时提示无可用发布；不使用占位 hash 或下载地址。

## 配置

SD 根目录 spoolbuddy.json 中原有 Wi-Fi/Bambuddy 配置保持原样，可额外加入：

```json
"ota": {
  "manifest_url": "https://api.github.com/repos/tbc0309/spoolbuddy-esp32/contents/ota.json?ref=main",
  "github_token": ""
}
```

manifest_url 可省略，默认上述本项目 GitHub Contents API。本仓库公开，默认无需 github_token。使用私有镜像仓库时可配置只读凭据，随 TF 导入后保存到 NVS。
私人 Releases 的镜像 URL 使用 GitHub Release asset API：
https://api.github.com/repos/tbc0309/spoolbuddy-esp32/releases/assets/实际资产ID。
下载时 Accept 为 application/octet-stream。公共镜像也可使用 HTTPS 下载 URL。
访问凭据只发给 GitHub API、GitHub 或 raw.githubusercontent.com 的精确主机，
不会随重定向传给资产 CDN。TLS 使用系统 CA 证书包，禁止降级到 HTTP。

## 发布流程

1. 固件版本继续等于 Bambuddy 基线；同一版本下的镜像修订用
   `idf.py -DSB_FIRMWARE_BUILD=6 build` 等内部修订单独递增，不能只改 ota.json 的 build。
2. 完成目标主板测试，将 build/spoolbuddy_port.bin 作为 Releases 资产上传。
   OTA 使用单独 application bin，不能使用包含 bootloader/分区的合并镜像。
3. 取得 Release asset URL，生成索引：

```powershell
python tools/create_ota_manifest.py --build-dir build --asset-url "https://api.github.com/repos/tbc0309/spoolbuddy-esp32/releases/assets/实际资产ID"
```

4. 工具读取实际板型、版本、build、文件大小和 SHA-256，更新根目录 ota.json，
   保留其他板型。审查后同步该索引至仓库。
5. P4 点击“检查更新”后，有新版本/新 build 即自动下载安装，无需再次点击应用。
   已是最新版只提示结果；独立 TF 卡入口允许同版本重装，但拒绝降级。下载写入非活动分区，验证长度、SHA-256、ESP 镜像结构、
   项目名称、应用版本、镜像内板型/build 标记通过后才选择下次启动分区。

## 分区与恢复

本板保留原 NVS 0x9000 和 factory 0x10000 的位置；factory 调整为 6 MiB，
otadata 位于 0x610000，两个 12 MiB OTA 槽位于 0x620000 和 0x1220000。
另有 64 KiB sb_config 配置区（0x1E20000），总体小于 32 MiB。
从旧分区迁移必须通过 USB 更新 bootloader、分区表、factory 和 otadata；
普通 OTA 不能迁移分区布局。

升级首启通过显示初始化、Wi-Fi 与 Bambuddy 注册后确认镜像；未能确认时，
180 秒后请求回滚，为两分钟的新配置回退预留重连时间；确认前异常重启也由
bootloader 回滚。更新时请保持后端可达。
首次旧工厂镜像不支持 OTA 回滚判定时保留 USB 恢复途径。

SHA-256 用于与 HTTPS 索引核对完整性，不是独立签名。设备不接受 HTTP 或
跳过证书校验；未启用芯片 Secure Boot 或签名密钥烧录。

协议依据：[GitHub Contents API](https://docs.github.com/en/rest/repos/contents)、
[GitHub Release assets API](https://docs.github.com/en/rest/releases/assets)。

Windows 模拟器展示更新入口，不写 ESP32 Flash。实际固件更新由主板安装器完成。

## 界面语言

默认不配置 language，SpoolBuddy 跟随 Bambuddy 主设置“常规 → 语言”。各类
中文语言统一显示简体中文，其余使用英语。显式配置 language 时优先级高于后端。
开机尚未读到后端语言时默认简体中文，后续连接中断保留最近语言。
SpoolBuddy 设置页不另加一个与原版不同的语言入口。

## TF 卡更新

运行 `python tools/create_ota_manifest.py --build-dir build --sdcard-dir X:/`，
将实际编译的 application bin 和带大小/SHA-256/板型/修订的 ota.json 写到卡根目录。
无需 GitHub URL 即可制作离线包。插回后在设置 → 更新点击“从 TF 卡更新”。
TF 与 HTTPS OTA 共用非活动槽写入、镜像校验、首启确认与回滚；不接受路径穿越、
合并烧录文件或降级镜像。界面只显示 Bambuddy 版本号，内部修订仅用于更新比较。

发布文件说明见 [RELEASING.md](RELEASING.md)。
