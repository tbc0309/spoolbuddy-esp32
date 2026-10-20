"""Package the already-built firmware and Windows simulator (run in ESP-IDF Python)."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import sys
import zipfile

root=Path(__file__).resolve().parent.parent
if 'SB_UI_PREVIEW:BOOL=ON' in (root/'build/CMakeCache.txt').read_text(encoding='utf-8'):
    raise SystemExit('Appearance preview: use tools/package_ui_preview.py instead.')
out=root/'outputs'
out.mkdir(exist_ok=True)
version='0.2.0'
binary=out/f'spoolbuddy-p4-rev3-{version}-merged.bin'
subprocess.run([sys.executable,'-m','esptool','--chip','esp32p4','merge-bin','-o',str(binary),'@flash_args'],cwd=root/'build',check=True)
app=(root/'build/spoolbuddy_p4.bin').read_bytes()
assert binary.read_bytes()[0x10000:0x10000+len(app)]==app
sim=out/'SpoolBuddySimulator.exe'
source_sim=root/'simulator/build/spoolbuddy_sim.exe'
if not sim.exists() or sim.read_bytes()!=source_sim.read_bytes():
    shutil.copy2(source_sim,sim)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
manifest=out/'SHA256SUMS-0.2.0.txt'
manifest.write_text(''.join(f'{sha(p)}  {p.name}\n' for p in (binary,sim)),encoding='utf-8')
report=root/'simulator/runtime/sim-self-test.txt'
assert report.exists() and 'Failures: 0' in report.read_text(encoding='utf-8')
release=out/'RELEASE-0.2.0.json'
release.write_text(json.dumps(dict(version=version,idf='6.1',board='ESP32-P4-WIFI6-Touch-LCD-4.3 Rev3.x',
    flash_offset='0x0',languages=['zh-CN','en'],weight_sources=['NAU7802','manual'],
    sd_config='/sdcard/spoolbuddy.json',application_bytes=len(app),
    firmware_sha256=sha(binary),simulator_sha256=sha(sim),
    simulator_checks=sum(line.startswith('PASS ') for line in report.read_text(encoding='utf-8').splitlines()),
    ui_resolution=[800,480],ui_implementation='native LVGL reconstruction; see README for missing upstream features',
    live_backend_inventory_unchanged=True,hardware_tested=False),indent=2),encoding='utf-8')
files=[root/name for name in ('.clang-format','.gitignore','CMakeLists.txt','LICENSE','README.md','THIRD_PARTY.md','dependencies.lock','partitions.csv','sdkconfig.defaults','sdkconfig.rev1_3')]
for folder in ('main','tools','assets','.github'):
    files.extend(p for p in (root/folder).rglob('*') if p.is_file() and '__pycache__' not in p.parts)
files.extend(p for p in (root/'simulator').iterdir() if p.is_file() and p.suffix in ('.c','.h','.ps1','.py','.md','.txt') and p.name!='spoolbuddy.json')
files.append(root/'sdcard/spoolbuddy.example.json')
files.extend([binary,sim,manifest,release])
for name in ('sim-manual-zh.png','sim-keyboard-en.png','sim-en.png','live-dashboard.png','live-manual.png','live-page-1.png','live-page-2.png','live-page-3.png','live-page-4.png'):
    source=root/'simulator/runtime'/name
    if source.exists():
        target=out/name;shutil.copy2(source,target);files.append(target)
archive_path=out/f'spoolbuddy-p4-rev3-{version}.zip'
with zipfile.ZipFile(archive_path,'w',zipfile.ZIP_DEFLATED) as archive:
    for p in files:archive.write(p,p.relative_to(root))
with zipfile.ZipFile(archive_path) as archive:
    assert archive.testzip() is None
    assert not any('/build/' in name or '/runtime/' in name or name.endswith('/spoolbuddy.json') for name in archive.namelist())
print(f'Package verified: {archive_path.name}, {archive_path.stat().st_size} bytes, {len(files)} files')
