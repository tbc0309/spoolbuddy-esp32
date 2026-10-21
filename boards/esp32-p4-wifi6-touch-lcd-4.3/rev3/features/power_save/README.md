# 熄屏省电 / Screen-off power saving

本功能属于 Rev3.x 主板适配，不改动上游 API。设置 → 显示中的开关保存在主板 NVS；超时熄屏与 BOOT 熄屏共用同一省电流程。关闭开关保留普通熄屏。

进入流程先显示黑场并等待两秒，然后使用 LVGL adapter 的 `sleep_prepare` 等待刷新完成、暂停渲染并解绑帧缓冲，发送 LCD 休眠指令，最后删除 LCD/DSI 硬件。释放显示驱动的 CPU 频率锁后，由 ESP-IDF tickless 电源管理根据其他驱动的锁决定空闲轻睡眠。运行时保留原有 CPU 频率，不动态降频；PSRAM 保持刷新，正在使用的 GPIO 保留工作状态。网络及后台任务继续运行。不会主动调用整机深睡眠或手动越过驱动锁强制轻睡眠。

这块板的 GT911 中断未接线，因此暂停 LVGL 后独立任务每 100 ms 轮询触摸。BOOT 检测使用独立任务及系统时钟，不依赖 LVGL 定时器。恢复时重新初始化显示硬件并绑定到原来的 LVGL 页面；用于唤醒的触摸会被吞掉，避免触发隐藏控件。

显示恢复失败会重试，不主动复位主板。若休眠期间发生异常复位，下次启动会暂时关闭本次启动的省电休眠，避免连续崩溃；设置中的开关会反映这个状态。该保护不修改保存的开关偏好。

硬件边界：AP3032 的 EN 接电源，软件不能切断背光驱动供电；DSI PHY 电源保留。空闲休眠不等于整板断电，也不保证特定电池续航。实际功耗取决于供电、外设和后台活动。频繁后台活动或驱动锁可能缩短或阻止轻睡眠。

`apply_bsp.py` 仅修复本板 BSP 的显示生命周期：共享 PHY 电源只申请一次，失败恢复时初始化清理句柄。脚本可重复执行，遇到未知源码结构会停止构建。

---

This Rev3.x board feature leaves the upstream API unchanged. Its switch in Settings → Display is stored in board NVS. Idle timeout and BOOT blanking use the same flow; disabling it retains ordinary blanking.

After a two-second black-frame grace period, the adapter drains pending flushes, pauses rendering and detaches framebuffers. The LCD receives its sleep command before LCD/DSI hardware is deleted, releasing the display CPU-frequency lock. ESP-IDF tickless power management chooses idle light sleep according to remaining driver locks. The active CPU frequency is retained without dynamic frequency scaling; PSRAM refresh and active GPIO levels are preserved. Networking and background tasks continue. The feature does not enter deep sleep or force manual light sleep past driver locks.

GT911 INT is not wired on this board. A separate task polls touch every 100 ms while LVGL is paused. BOOT uses its own task and system clock. Wake recreates display hardware and rebinds the existing LVGL page; the waking touch is consumed.

Failed display recovery is retried without deliberately resetting the board. If a reset occurs during sleep, the next boot temporarily disables power saving to avoid a crash loop; the switch reflects that state. The saved preference is unchanged.

AP3032 EN is tied to the supply, and the DSI PHY supply remains enabled. Idle sleep is not board power-off. Power consumption depends on the supply, peripherals and background activity; driver locks can prevent light sleep.

The idempotent board-local BSP adaptation makes shared PHY acquisition repeatable and initializes failure-cleanup handles. Unknown source layouts stop the build.
