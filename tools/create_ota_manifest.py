"""Generate a board-specific OTA entry from a verified ESP-IDF build and Release asset URL."""
import argparse
import hashlib
import json
import re
import shutil
from pathlib import Path
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parents[1]


def release_entry(build: Path, asset_url: str | None) -> dict:
    url = urlsplit(asset_url or "")
    if asset_url and (url.scheme != "https" or not url.hostname or url.username or url.password or url.fragment):
        raise ValueError("Release asset URL must be HTTPS without credentials or a fragment")
    cache = (build / "CMakeCache.txt").read_text(encoding="utf-8")
    revision = re.search(r"^SB_FIRMWARE_BUILD:STRING=(\d+)$", cache, re.M)
    if not revision:
        raise ValueError("Rebuild with an SB_FIRMWARE_BUILD revision")
    project = json.loads((build / "project_description.json").read_text(encoding="utf-8"))
    version = project["project_version"]
    expected = json.loads((ROOT / "upstream-versions.json").read_text())["bambuddy"]["latest_release"].removeprefix("v")
    if version != expected:
        raise ValueError("Firmware version differs from the project's upstream baseline")
    binary = (build / project["app_bin"]).resolve()
    if not binary.is_relative_to(build.resolve()):
        raise ValueError("Application image must be inside the selected build directory")
    board = (build / "sb-board-id.txt").read_text().strip()
    if not (ROOT / "boards" / board / "board.cmake").is_file():
        raise ValueError("Unknown board profile")
    data = binary.read_bytes()
    if not 1024 <= len(data) <= 12 * 1024 * 1024 or data[0] != 0xE9:
        raise ValueError("Expected an ESP application .bin, not a merged flash image")
    identity = f"SB_OTA_ID:{board}:{int(revision[1])}".encode() + b"\0"
    if identity not in data:
        raise ValueError("Rebuild: application board/build identity does not match metadata")
    entry={"board": board, "version": version, "build": int(revision[1]),
           "file":"spoolbuddy.bin", "size": len(data), "sha256": hashlib.sha256(data).hexdigest()}
    if asset_url:entry["url"]=asset_url
    return entry


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build")
    parser.add_argument("--asset-url")
    parser.add_argument("--sdcard-dir",type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if not args.asset_url and not args.sdcard_dir:parser.error("Provide --asset-url or --sdcard-dir")
    if args.sdcard_dir:
        if not args.sdcard_dir.is_dir():parser.error("TF destination must exist")
        project=json.loads((args.build_dir/'project_description.json').read_text(encoding='utf-8'))
        binary=(args.build_dir/project['app_bin']).resolve()
        # Validate before copying any image to the card.
        entry=release_entry(args.build_dir.resolve(),args.asset_url)
        shutil.copy2(binary,args.sdcard_dir/entry['file'])
    if args.output is None:args.output=(args.sdcard_dir/'ota.json') if args.sdcard_dir else ROOT/'ota.json'
    entry = release_entry(args.build_dir.resolve(), args.asset_url)
    document = json.loads(args.output.read_text()) if args.output.exists() else {"schema_version": 1, "releases": []}
    if document.get("schema_version") != 1 or not isinstance(document.get("releases"), list):
        raise ValueError("Unsupported manifest schema")
    document["releases"] = [r for r in document["releases"] if r.get("board") != entry["board"]] + [entry]
    args.output.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
    print(f"Manifest generated: {entry['board']} {entry['version']} build {entry['build']}")


if __name__ == "__main__":
    main()
