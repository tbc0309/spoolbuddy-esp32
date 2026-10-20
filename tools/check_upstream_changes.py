"""Report relevant upstream changes without modifying the port or applying local patches."""
from pathlib import Path
import argparse
import hashlib
import json

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--source", type=Path, default=root / "upstream/bambuddy")
args = parser.parse_args()
baseline = json.loads((root / "upstream-map.json").read_text(encoding="utf-8"))
changed = []
for entry in baseline["tracked_sources"]:
    path = args.source / entry["path"]
    digest = hashlib.sha256(path.read_bytes()).hexdigest() if path.exists() else None
    if digest != entry["sha256"]:
        changed.append({"area":entry["area"], "path":entry["path"], "state":"changed" if path.exists() else "missing"})
print(json.dumps({"baseline":baseline["commit"], "review_required":bool(changed), "files":changed}, indent=2))
raise SystemExit(1 if changed else 0)
