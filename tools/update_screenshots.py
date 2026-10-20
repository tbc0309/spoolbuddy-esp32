"""Refresh the documented LVGL fixture screenshots after simulator/test_simulator.py."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
runtime = ROOT / "simulator/runtime"
destination = ROOT / "docs/screenshots"
states = ["dashboard", "dashboard-idle", "ams", "write", "write-1", "write-2",
          "write-full-1", "write-full-2", "inventory", "settings", "settings-1",
          "settings-2", "settings-3", "settings-4"]
try:
    font = ImageFont.truetype("arial.ttf", 15)
except OSError:
    font = ImageFont.load_default()
for source_lang, language in [("en", "en"), ("zh", "zh-CN")]:
    folder = destination / language
    folder.mkdir(parents=True, exist_ok=True)
    sheet = Image.new("RGB", (1200, 5 * 270), "#181c1f")
    draw = ImageDraw.Draw(sheet)
    for i, state in enumerate(states):
        source = runtime / f"original-{source_lang}-{state}.bmp"
        image = Image.open(source).convert("RGB")
        assert image.size == (800, 480), source
        image.save(folder / f"{state}.png", optimize=True)
        x, y = (i % 3) * 400, (i // 3) * 270
        sheet.paste(image.resize((400, 240)), (x, y + 25))
        draw.text((x + 8, y + 4), state, fill="white", font=font)
    sheet.save(destination / f"overview-{language}.png", optimize=True)
rows = ["# 界面截图 / Screenshot gallery", "", "由生产 LVGL 界面的隔离演示数据生成，尺寸 800×480。",
        "它们用于布局复查，不代表实板 NFC、称重或所有动态状态已验收。", "",
        "Generated from production LVGL widgets with isolated demonstration data at 800×480. These images support layout review; they do not certify physical NFC, weighing or every dynamic state.", "",
        "更新方式：先运行 `simulator/test_simulator.py`，再运行 `tools/update_screenshots.py`（需要 Pillow）。", "",
        "Update: run `simulator/test_simulator.py`, then `tools/update_screenshots.py` (requires Pillow).", ""]
for state in states:
    rows += [f"## {state}", "", "### 简体中文", "",
             f"![简体中文 {state}](screenshots/zh-CN/{state}.png)", "",
             "### English", "", f"![English {state}](screenshots/en/{state}.png)", ""]
extra_states=["material-filter","write-search","color-catalog","pa-profiles","power-menu",
              "network-connected","system-en-top","system-en-bottom","system-zh-top",
              "system-zh-bottom","updates-en","topbar-en-12h","topbar-en-24h",
              "topbar-zh-12h","topbar-zh-24h","updates-en-progress","updates-zh-progress",
              "updates-en-error","updates-zh-error"]
folder=destination/"states";folder.mkdir(exist_ok=True)
sheet=Image.new("RGB",(1200,((len(extra_states)+2)//3)*270),"#181c1f");draw=ImageDraw.Draw(sheet)
rows += ["", "## 交互及系统状态 / Interaction and system states", ""]
for i,state in enumerate(extra_states):
    source=runtime/f"integration-{state}.bmp"
    assert source.stat().st_mtime >= (ROOT/"simulator/build/spoolbuddy_sim.exe").stat().st_mtime,source
    image=Image.open(source).convert("RGB");image.save(folder/f"{state}.png",optimize=True)
    x,y=i%3*400,i//3*270;sheet.paste(image.resize((400,240)),(x,y+25))
    draw.text((x+8,y+4),state,fill="white",font=font)
    rows += [f"### {state}", "", f"![{state}](screenshots/states/{state}.png)", ""]
sheet.save(destination/"overview-states.png",optimize=True)
(ROOT / "docs/SCREENSHOTS.md").write_text("\n".join(rows).rstrip() + "\n", encoding="utf-8")
print("Refreshed 47 full-resolution screenshots and three overview sheets")
