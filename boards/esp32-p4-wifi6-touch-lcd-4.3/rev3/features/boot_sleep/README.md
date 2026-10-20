# BOOT screen sleep (board feature)

Enabled by default for this Rev3.x board. It is kept with the board adapter,
outside the shared upstream UI and business logic. To disable it explicitly:

```
idf.py -DSB_ENABLE_BOOT_SLEEP=OFF build
```

The selected board supplies the BOOT GPIO (Waveshare P4 Rev3.x: GPIO35,
active low, external pull-up, per the vendor schematic). No output drive,
strapping/eFuse change, deep sleep, or bootloader change is performed.
Holding BOOT while resetting still selects ROM download mode.

After UI initialization, a stable release arms the button. A subsequent
debounced press/release toggles screen blanking. Holding the button never
repeats; a button held at startup is ignored until released. Screen blanking
keeps Wi-Fi, backend heartbeats and sensors running. A second BOOT press or
the first touch wakes the display; the waking touch is consumed. Existing
upstream sensor wake events also wake the display. Brightness and idle timeout
remain controlled by the original settings. Button polling runs on the LVGL
timer task, independently of the backend HTTP worker.

Implementation and button state tests live here; common sources contain only
capability hooks. The simulator and other boards do not compile this board's
button handler. New boards select their own features in their board.cmake.

Host regression: compile and run `test_button.c` with a C compiler (assertions
enabled). Screen blanking and touch wake have been checked on Rev3.x.
Electrical power consumption has not been measured.

Display blanking uses an opaque black system-layer mask and zero PWM brightness,
while keeping the DSI panel scan and frame-complete interrupts active. This avoids
turning the panel off underneath the adapter's asynchronous double-buffer waits.
It does not disconnect backlight power: AP3032 EN is tied to the supply. Physical
black level and long-duration wake reliability must be checked on the target board.

The board logs a low-frequency blank-health message independently of LVGL and
reports stalled LVGL progress / prolonged mutex waits. These diagnostics are
serial-only and do not extend the backend statistics payload.


Reference: https://files.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-4.3/ESP32-P4-WIFI6-Touch-LCD-4.3-schematic.pdf
