# 上游基线与复验

[English](en/UPSTREAM_COMPATIBILITY.md)

本次发布对应 Bambuddy **1.2.5.7**，精确提交记录在根目录 `upstream-versions.json`，逐文件哈希在 `upstream-map.json`。后端接口按 1.2.5.7 的 OpenAPI 检查。这里的版本描述是已测试基线，不表示未来上游仍是这一版本。

SpoolBuddy 来源为 maziggy/bambuddy 中集成的 React 页面、Python daemon 和 FastAPI 路由。ESP32 使用共享 C / LVGL 适配，不运行 Pi/Linux daemon，也不依赖 Raspberry Pi Pico。macpit 的参考审查独立保留在 patches/macpit。

复验入口：

```powershell
python tools/check_upstream_changes.py --source upstream/bambuddy
python tools/check_backend_contract.py --expected-version 1.2.5.7
python -m pytest tests/backend/test_native_roundtrip.py -q -o asyncio_mode=auto
```

后端测试使用一次性内存数据库，不写入本机库存。测试依赖来自锁定的 Bambuddy 开发环境；不把没有运行的硬件外设或 Linux 专有测试计作 ESP32 通过。
