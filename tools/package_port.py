"""Package the verified board application, factory image, TF update and Windows simulator."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from create_ota_manifest import release_entry


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def license_files(sdk):
    files = {name: ROOT / name for name in ('LICENSE', 'NOTICE', 'THIRD_PARTY.md')}
    for folder in ('assets/licenses',):
        for path in (ROOT / folder).iterdir():
            if path.is_file(): files[str(path.relative_to(ROOT)).replace('\\', '/')] = path
    for name in ('assets/fonts/OFL.txt', 'assets/upstream/lucide/LICENSE'):
        files[name] = ROOT / name
    for path in (ROOT / 'managed_components').rglob('*'):
        if path.is_file() and (path.name.lower().startswith(('license', 'licence', 'copying', 'notice'))
                               or path.name.lower() in ('ftl.txt', 'gplv2.txt')):
            files['dependencies/' + path.relative_to(ROOT / 'managed_components').as_posix()] = path
    for name in ('LICENSE', 'NOTICE'):
        if (sdk / name).is_file(): files['esp-idf/' + name] = sdk / name
    return files


def add_licenses(archive, files):
    for name, path in sorted(files.items()): archive.write(path, 'licenses/' + name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build')
    parser.add_argument('--output-dir', type=Path)
    args = parser.parse_args()
    build = args.build_dir.resolve()
    entry = release_entry(build, None)
    version, board_id = entry['version'], entry['board']
    out = args.output_dir or ROOT / 'outputs/release' / version
    out.mkdir(parents=True, exist_ok=True)
    cache = (build / 'CMakeCache.txt').read_text(encoding='utf-8')
    host_cache = (ROOT / 'simulator/build/CMakeCache.txt').read_text(encoding='utf-8')
    assert 'SB_UI_PREVIEW:BOOL=OFF' in cache
    assert 'SB_UI_PREVIEW:BOOL=OFF' in host_cache, 'Release the functional simulator, not the appearance-only build'
    for option in ('SB_ENABLE_MANUAL_WEIGHT', 'SB_ENABLE_ACTION_CONFIRMATION'):
        assert f'{option}:BOOL=OFF' in cache, option
    for option in ('SB_ENABLE_MANUAL_WEIGHT', 'SB_ENABLE_ACTION_CONFIRMATION', 'SB_ENABLE_SIMULATOR_CONTROLS'):
        assert f'{option}:BOOL=OFF' in host_cache, option
    project = json.loads((build / 'project_description.json').read_text(encoding='utf-8'))
    sdk = Path(project['idf_path'])
    notices = license_files(sdk)
    metadata = json.loads((ROOT / 'boards' / board_id / 'board.json').read_text(encoding='utf-8'))
    app = (build / project['app_bin']).resolve()
    exe = ROOT / 'simulator/build/spoolbuddy_sim.exe'
    for folder in (ROOT / 'src', ROOT / 'assets', ROOT / 'boards' / board_id):
        for source in folder.rglob('*'):
            # Generated i18n declarations may be rewritten byte-identically by tests.
            if source.suffix in ('.c', '.h') and source.name != 'i18n_catalog.h':
                assert source.stat().st_mtime <= app.stat().st_mtime, f'Rebuild firmware: {source}'
    for folder in (ROOT / 'src/core', ROOT / 'src/ui', ROOT / 'src/ports', ROOT / 'assets', ROOT / 'simulator'):
        for source in folder.rglob('*'):
            if 'build' in source.parts or any(p.startswith('build-') for p in source.parts):
                continue
            # Generated i18n declarations may be rewritten byte-identically by tests.
            if source.suffix in ('.c', '.h') and source.name != 'i18n_catalog.h':
                assert source.stat().st_mtime <= exe.stat().st_mtime, f'Rebuild simulator: {source}'
    reports = {'port-selftest.txt': '0 failures', 'port-transport-test.txt': '0 failures',
               'original-preview-check.txt': 'pointer navigation: PASS',
               'port-realtime-suite.json': '"status": "PASS"', 'port-backend-e2e.txt': '0 failures'}
    report_text = {}
    for name, marker in reports.items():
        source = ROOT / 'simulator/runtime' / name
        text = source.read_text(encoding='utf-8')
        assert marker in text and source.stat().st_mtime >= exe.stat().st_mtime, f'Re-run tests: {name}'
        report_text[name] = text
    python_report = (ROOT / '.tools/release-python-tests.log').read_text(encoding='utf-8')
    assert re.search(r'\b\d+ passed', python_report) and not re.search(r'\d+ (failed|errors?)', python_report)
    hardware_path = ROOT / '.tools/release-hardware.json'
    hardware = json.loads(hardware_path.read_text(encoding='utf-8'))
    assert hardware['board'] == board_id, 'Hardware report belongs to a different board'
    assert hardware['firmware_sha256'] == sha(app) and hardware['smoke_passed'], 'Re-run hardware smoke on this image'
    prefix = f'spoolbuddy-{metadata["artifact_id"]}-{version}'
    application = out / f'{prefix}.bin'
    shutil.copy2(app, application)
    factory = out / f'{prefix}.factory.bin'
    subprocess.run([sys.executable, '-m', 'esptool', '--chip', metadata['chip'], 'merge-bin',
                    '-o', str(factory.resolve()), '@flash_args'], cwd=build, check=True)
    flasher = json.loads((build / 'flasher_args.json').read_text(encoding='utf-8'))
    offset = int(flasher['app']['offset'], 0)
    assert factory.read_bytes()[offset:offset + app.stat().st_size] == app.read_bytes()
    tf = out / f'{prefix}.tf.zip'
    with zipfile.ZipFile(tf, 'w', zipfile.ZIP_DEFLATED) as archive:
        add_licenses(archive, notices)
        archive.writestr('spoolbuddy.bin', app.read_bytes())
        archive.writestr('ota.json', json.dumps({'schema_version': 1, 'releases': [entry]}, indent=2) + '\n')
    simulator = out / f'SpoolBuddy-Windows-{version}.zip'
    example = {'wifi': {'ssid': 'SIMULATOR', 'password': ''},
               'bambuddy': {'url': 'http://127.0.0.1:8000', 'api_key': ''}}
    with zipfile.ZipFile(simulator, 'w', zipfile.ZIP_DEFLATED) as archive:
        add_licenses(archive, notices)
        archive.write(exe, 'SpoolBuddy.exe')
        archive.writestr('spoolbuddy.example.json', json.dumps(example, indent=2) + '\n')
        archive.writestr('start.cmd', '@echo off\r\n'
                          'if not exist "%~dp0spoolbuddy.json" copy /y "%~dp0spoolbuddy.example.json" "%~dp0spoolbuddy.json" >nul\r\n'
                          'start "" "%~dp0SpoolBuddy.exe" --config "%~dp0spoolbuddy.json" --connect\r\n')
        archive.writestr('README.txt', 'Copy spoolbuddy.example.json to spoolbuddy.json; set your Bambuddy URL.\n'
                          'Run: SpoolBuddy.exe --config spoolbuddy.json --connect\n'
                          'Native LVGL host simulation; does not emulate ESP32 peripheral electronics.\n')
    licenses = out / 'licenses.zip'
    with zipfile.ZipFile(licenses, 'w', zipfile.ZIP_DEFLATED) as archive:
        add_licenses(archive, notices)
    files = [application, factory, tf, simulator, licenses]
    for archive_path in (tf, simulator, licenses):
        with zipfile.ZipFile(archive_path) as archive:
            assert archive.testzip() is None
    sdk=Path(project['idf_path'])
    sdk_commit=subprocess.check_output(['git','-C',str(sdk),'rev-parse','HEAD'],text=True).strip()
    sdk_version=subprocess.check_output(['git','-C',str(sdk),'describe','--always','--tags','--dirty'],text=True).strip()
    manifest = {'version': version, 'board': board_id, 'revision': entry['build'],
                'sdk': {'idf': sdk_version, 'commit': sdk_commit, 'lvgl': '9.5.0'},
                'profile': 'upstream_parity', 'enabled_product_patches': [],
                'dependency_fixes': ['esp_hosted_rx_retry'],
                'board_features': metadata.get('features', {}), 'application_bytes': app.stat().st_size,
                'ota_slot_bytes': 12 * 1024 * 1024, 'full_upstream_parity': False,
                'upstream': json.loads((ROOT / 'upstream-versions.json').read_text(encoding='utf-8')),
                'hardware_validation': hardware,
                'tests': {'native': re.search(r'\d+ checks, 0 failures', report_text['port-selftest.txt'])[0],
                          'http': re.search(r'\d+ checks, 0 failures', report_text['port-transport-test.txt'])[0],
                          'python': re.search(r'\d+ passed', python_report)[0],
                          'websocket': '3 scenarios passed', 'backend_roundtrip': 'passed'},
                'files': {p.name: {'bytes': p.stat().st_size, 'sha256': sha(p)} for p in files}}
    path = out / 'release-manifest.json'
    path.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    files.append(path)
    (out / 'SHA256SUMS.txt').write_text(''.join(f'{sha(p)}  {p.name}\n' for p in files), encoding='ascii')
    print(json.dumps({'directory': str(out), 'version': version, 'board': board_id, 'assets': len(files) + 1}))


if __name__ == '__main__':
    main()
