"""Compile production manifest/config parsing; test isolated release metadata and publisher."""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[2]
BOARD = "esp32-p4-wifi6-touch-lcd-4.3/rev3"


@pytest.fixture(scope="module")
def runner(tmp_path_factory):
    work = tmp_path_factory.mktemp("ota-native")
    exe = work / "manifest.exe"
    zig = ROOT / ".tools/sim-venv/Lib/site-packages/ziglang/zig.exe"
    cjson = ROOT / "managed_components/espressif__cjson/cJSON"
    subprocess.run([str(zig), "cc", "-O1", "-fno-sanitize=undefined", "-target", "x86_64-windows-gnu",
                    "-I" + str(ROOT / "src/core"), "-I" + str(cjson),
                    str(ROOT / "tests/ota/manifest_runner.c"), str(ROOT / "src/core/ota_manifest.c"),
                    str(ROOT / "src/core/settings.c"), str(ROOT / "src/core/clock.c"), str(cjson / "cJSON.c"), "-o", str(exe)],
                   check=True, timeout=120)

    def run(payload, version="1.2.5.7", build=1):
        result = subprocess.run([str(exe), version, str(build)], input=json.dumps(payload),
                                text=True, capture_output=True, check=True, timeout=10)
        return json.loads(result.stdout)

    return run


def entry(**changes):
    return dict(board=BOARD, version="1.2.5.7", build=2, size=4096,
                url="https://api.github.com/repos/tbc0309/spoolbuddy-esp32/releases/assets/123",
                sha256="A" * 64) | changes


def manifest(*entries):
    return {"schema_version": 1, "releases": list(entries)}


def test_numeric_version_and_build_order(runner):
    assert runner(manifest(entry()))["newer"]
    assert runner(manifest(entry(build=1)))["newer"] is False
    assert runner(manifest(entry(version="1.2.5.10")), "1.2.5.9")["newer"]
    assert runner(manifest(entry(version="1.2.4.99", build=100)))["newer"] is False
    assert runner(manifest(entry()))["sha256"] == "a" * 64


def test_empty_unpublished_and_other_board_never_offer_update(runner):
    assert runner(manifest())["result"] == 0
    assert runner(manifest(entry(board="other-board")))["result"] == 0
    assert runner(manifest(entry(version="1.2.5.8"), entry(build=3)))["newer"]


@pytest.mark.parametrize("changes", [
    {"size": 0}, {"size": 12 * 1024 * 1024 + 1}, {"size": 4096.5}, {"build": -1},
    {"build": 1.5}, {"sha256": "f" * 63}, {"sha256": "z" * 64},
    {"url": "http://example.invalid/image.bin"}, {"url": "https://user:token@example.invalid/image.bin"},
    {"url": "https://example.invalid/image.bin#fragment"}, {"url": "https:///image.bin"},
    {"url": "https://example.invalid/\nimage.bin"}, {"version": "1.2.5.7evil"},
    {"version": "1.2"}, {"version": "1.2.3.4.5"},
])
def test_invalid_release_rejected_before_flash(runner, changes):
    assert runner(manifest(entry(**changes)))["result"] == -1


@pytest.mark.parametrize("ota,valid", [
    ({"manifest_url": "https://example.invalid/ota.json", "github_token": ""}, True),
    ({"manifest_url": "http://example.invalid/ota.json"}, False),
    ({"github_token": "fake\r\nInjected: header"}, False),
])
def test_config_ota_validation(runner, ota, valid):
    payload = {"wifi": {"ssid": "fixture", "password": ""},
               "bambuddy": {"url": "http://fixture.invalid:8000", "api_key": ""}, "ota": ota}
    assert runner(payload, "settings")["valid"] is valid


def test_publication_uses_actual_binary_hash_and_board(tmp_path):
    spec = importlib.util.spec_from_file_location("ota_publisher", ROOT / "tools/create_ota_manifest.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    marker = f"SB_OTA_ID:{BOARD}:8".encode() + b"\0"
    binary = b"\xe9" + marker + bytes(4095 - len(marker))
    (tmp_path / "firmware.bin").write_bytes(binary)
    (tmp_path / "CMakeCache.txt").write_text("SB_FIRMWARE_BUILD:STRING=8\n")
    (tmp_path / "sb-board-id.txt").write_text(BOARD)
    (tmp_path / "project_description.json").write_text(json.dumps({"project_version": "1.2.5.7", "app_bin": "firmware.bin"}))
    result = module.release_entry(tmp_path, "https://example.invalid/release.bin")
    assert result["sha256"] == hashlib.sha256(binary).hexdigest()
    assert result["size"] == len(binary) and result["build"] == 8 and result["board"] == BOARD
    with pytest.raises(ValueError):
        module.release_entry(tmp_path, "http://example.invalid/release.bin")
    (tmp_path / "CMakeCache.txt").write_text("SB_FIRMWARE_BUILD:STRING=9\n")
    with pytest.raises(ValueError,match="identity"):
        module.release_entry(tmp_path,"https://example.invalid/release.bin")


@pytest.mark.parametrize("url,allowed", [
    ("https://api.github.com/repos/test", True),
    ("https://raw.githubusercontent.com/owner/repo/main/ota.json", True),
    ("https://release-assets.githubusercontent.com/private.bin", False),
    ("https://api.github.com.attacker.invalid/private.bin", False),
    ("https://example.invalid/?url=https://api.github.com/", False),
])
def test_private_credentials_do_not_follow_asset_redirect_hosts(runner, url, allowed):
    assert runner(manifest() | {"probe_url": url})["github_auth"] is allowed


@pytest.mark.parametrize("marker,valid", [
    (f"paddingSB_OTA_ID:{BOARD}:2",True),
    (f"SB_OTA_ID:{BOARD}:20",False),
    ("SB_OTA_ID:another-board:2",False),
    (f"SB_OTA_ID:{BOARD}:1",False),
    (f"SB_OTA_ID:noiseSB_OTA_ID:{BOARD}:2",True),
])
def test_image_identity_survives_fragmentation_and_rejects_wrong_board_build(runner, marker, valid):
    assert runner(manifest(entry()) | {"marker":marker})["identity"] is valid

@pytest.mark.parametrize('language,chinese', [('zh',True),('zh-CN',True),('zh-TW',True),('zh-HK',True),('zh-Hant',True),('ZH_hans',True),('ja',False),('fr',False),('en',False)])
def test_config_language_override_and_chinese_family(runner,language,chinese):
    payload={'wifi':{'ssid':'fixture','password':''},'bambuddy':{'url':'http://fixture.invalid','api_key':''},'language':language}
    result=runner(payload,'settings')
    assert result['valid'] and result['override'] and result['chinese'] is chinese
    del payload['language']
    assert runner(payload,'settings')['override'] is False

@pytest.mark.parametrize('filename,valid', [('spoolbuddy.bin',True),('../image.bin',False),('a/b.bin',False),('a\\b.bin',False),('/image.bin',False),('image.bin:stream',False),('.hidden',False),('a..bin',False)])
def test_tf_update_paths_are_confined_to_card_root(runner,filename,valid):
    record=entry(file=filename);del record['url']
    assert (runner(manifest(record))['result']==1) is valid

def test_twelve_mib_images_fit_the_new_dual_ota_slots(runner):
    assert runner(manifest(entry(size=12*1024*1024)))['result']==1

@pytest.mark.parametrize('date,epoch', [('Tue, 20 Oct 2026 04:00:00 GMT',1792468800),('Thu, 29 Feb 2024 00:00:00 GMT',1709164800),('Wed, 29 Feb 2023 00:00:00 GMT',-1),('Tue, 20 Oct 2026 04:00:00',-1),('Tue, 20 Oct 2026 25:00:00 GMT',-1)])
def test_backend_date_bootstraps_utc_without_timezone_shift(runner,date,epoch):
    assert runner({'date':date},'clock')['epoch']==epoch
