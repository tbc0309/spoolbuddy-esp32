"""Read-only compatibility check against a running Bambuddy OpenAPI document.

Does not register a device, read inventory contents, or change backend data.
"""
import argparse
import json
from urllib.request import urlopen


def check(spec):
    paths = spec.get("paths", {})
    required_posts = [
        "/devices/register",
        "/devices/{device_id}/heartbeat",
        "/nfc/tag-scanned",
        "/nfc/tag-removed",
        "/nfc/write-result",
        "/scale/reading",
        "/devices/{device_id}/calibration/set-tare",
        "/devices/{device_id}/system/command-result",
        "/diagnostics/{device_id}/result",
    ]
    problems = []
    for path in required_posts:
        full = "/api/v1/spoolbuddy" + path
        if "post" not in paths.get(full, {}):
            problems.append(f"Missing POST {full}")
    if "get" not in paths.get("/api/v1/inventory/spools/{spool_id}", {}):
        problems.append("Missing inventory detail GET")
    ui_routes = [
        ("get", "/api/v1/settings/spoolman"),
        ("get", "/api/v1/inventory/spools"),
        ("get", "/api/v1/inventory/colors/search"),
        ("post", "/api/v1/inventory/spools"),
        ("patch", "/api/v1/inventory/spools/{spool_id}/link-tag"),
        ("get", "/api/v1/inventory/assignments"),
        ("post", "/api/v1/inventory/assignments"),
        ("delete", "/api/v1/inventory/assignments/{printer_id}/{ams_id}/{tray_id}"),
        ("get", "/api/v1/printers/"),
        ("get", "/api/v1/printers/{printer_id}/status"),
        ("get", "/api/v1/spoolbuddy/devices"),
        ("put", "/api/v1/spoolbuddy/devices/{device_id}/display"),
        ("post", "/api/v1/spoolbuddy/devices/{device_id}/calibration/set-factor"),
        ("post", "/api/v1/spoolbuddy/nfc/write-tag"),
        ("post", "/api/v1/spoolbuddy/devices/{device_id}/cancel-write"),
        ("post", "/api/v1/spoolbuddy/scale/update-spool-weight"),
        ("post", "/api/v1/spoolbuddy/diagnostics/{device_id}/run"),
        ("get", "/api/v1/spoolbuddy/diagnostics/{device_id}/result"),
        ("get", "/api/v1/spoolman/inventory/spools"),
        ("post", "/api/v1/spoolman/inventory/slot-assignments"),
        ("delete", "/api/v1/spoolman/inventory/slot-assignments/{spoolman_spool_id}"),
    ]
    for method, path in ui_routes:
        if method not in paths.get(path, {}):
            problems.append(f"Missing {method.upper()} {path}")
    schemas = spec.get("components", {}).get("schemas", {})
    expected = {
        "HeartbeatResponse": {
            "pending_command", "pending_write_payload", "tare_offset",
            "calibration_factor", "display_brightness", "display_blank_timeout",
        },
        "DeviceRegisterRequest": {
            "device_id", "hostname", "ip_address", "has_nfc", "has_scale",
            "has_backlight", "tare_offset", "calibration_factor",
        },
        "TagScannedRequest": {"device_id", "tag_uid", "sak", "tag_type", "tray_uuid"},
        "WriteTagResultRequest": {"device_id", "spool_id", "tag_uid", "success", "message"},
        "ScaleReadingRequest": {"device_id", "weight_grams", "stable", "raw_adc"},
        "SpoolResponse": {"id", "brand", "material", "label_weight", "weight_used"},
    }
    for name, fields in expected.items():
        missing = fields - schemas.get(name, {}).get("properties", {}).keys()
        if missing:
            problems.append(f"{name}: missing fields {', '.join(sorted(missing))}")
    return problems


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default="http://127.0.0.1:8000")
    parser.add_argument("--expected-version")
    parser.add_argument("--report")
    args = parser.parse_args()
    with urlopen(args.url.rstrip("/") + "/openapi.json", timeout=15) as response:
        spec = json.load(response)
    problems = check(spec)
    version = spec.get("info", {}).get("version")
    if args.expected_version and version != args.expected_version:
        problems.append(f"Expected Bambuddy {args.expected_version}, got {version}")
    if problems:
        raise SystemExit("\n".join(problems))
    print(f"PASS: Bambuddy {version}; hardware and native UI routes plus 6 schemas match (read-only).")
    if args.report:
        from pathlib import Path
        Path(args.report).write_text(json.dumps({"version":version,"contract":"PASS","read_only":True},indent=2),encoding="utf-8")
