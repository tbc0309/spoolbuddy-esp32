"""Package the native original-layout preview. Run with the ESP-IDF Python."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import sys
import zipfile

root = Path(__file__).resolve().parent.parent
out = root / 'outputs'
out.mkdir(exist_ok=True)
for cache in (root/'build/CMakeCache.txt', root/'simulator/build/CMakeCache.txt'):
    assert 'SB_UI_PREVIEW:BOOL=ON' in cache.read_text(encoding='utf-8'), cache
report = root/'simulator/runtime/original-preview-check.txt'
assert 'pointer navigation: PASS' in report.read_text(encoding='utf-8')
binary = out/'spoolbuddy-p4-rev3-ui-preview.bin'
subprocess.run([sys.executable, '-m', 'esptool', '--chip', 'esp32p4', 'merge-bin',
                '-o', str(binary), '@flash_args'], cwd=root/'build', check=True)
app = (root/'build/spoolbuddy_p4.bin').read_bytes()
assert binary.read_bytes()[0x10000:0x10000+len(app)] == app
exe = out/'SpoolBuddy-Original-UI.exe'
source_exe=root/'simulator/build/spoolbuddy_sim.exe'
if not exe.exists() or exe.read_bytes()!=source_exe.read_bytes():
    shutil.copy2(source_exe,exe)
screens = out/'original-ui-screens'
screens.mkdir(exist_ok=True)
images = sorted((root/'simulator/runtime').glob('original-*.png'))
assert len(images) == 28, len(images)
for image in images:
    shutil.copy2(image, screens/image.name)
shutil.copy2(report, out/'original-preview-check.txt')
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
manifest = out/'original-ui-preview.json'
manifest.write_text(json.dumps(dict(
    target='appearance-only native LVGL preview', idf='6.1', lvgl='9.5.0',
    board='ESP32-P4-WIFI6-Touch-LCD-4.3 Rev3.x', resolution=[800,480],
    languages=['zh-CN','en'], hardware_tested=False,
    backend_actions=False, sensor_actions=False, application_bytes=len(app),
    preview_sha256=sha(exe), firmware_sha256=sha(binary), pointer_navigation='PASS',
    captured_states=len(images)), indent=2), encoding='utf-8')
files = [root/p for p in ('CMakeLists.txt','README.md','THIRD_PARTY.md','LICENSE',
         '.gitignore','.clang-format','dependencies.lock','sdkconfig.defaults',
         'sdkconfig.rev1_3','partitions.csv')]
for folder in ('main','assets','tools','docs','.github'):
    files += [p for p in (root/folder).rglob('*') if p.is_file() and '__pycache__' not in p.parts]
files += [p for p in (root/'simulator').iterdir() if p.is_file() and
          p.suffix in ('.c','.h','.ps1','.py','.txt','.md') and p.name!='spoolbuddy.json']
files += [root/'sdcard/spoolbuddy.example.json', binary, exe, manifest,
          out/'original-preview-check.txt'] + list(screens.glob('*.png'))
archive = out/'spoolbuddy-p4-rev3-original-ui-preview.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
    for file in files:
        z.write(file,file.relative_to(root))
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    assert not any('/build/' in p or '/runtime/' in p or p.endswith('/spoolbuddy.json') for p in z.namelist())
print(json.dumps(dict(package=str(archive),bytes=archive.stat().st_size,
                     application_bytes=len(app),screens=len(images)),ensure_ascii=False))
