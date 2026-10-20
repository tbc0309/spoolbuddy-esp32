"""Reuse upstream's guarded disposable-database fixtures; never load local service data."""
from pathlib import Path
import sys

upstream = Path(__file__).resolve().parents[2] / "upstream/bambuddy"
if not (upstream / "backend/tests/conftest.py").exists():
    raise RuntimeError("Fetch the pinned Bambuddy reference first")
sys.path.insert(0, str(upstream))
# Re-export upstream fixtures within this subtree. pytest_plugins in a nested
# conftest leaks globally and prevents collecting the complete project suite.
from backend.tests.conftest import *  # noqa: E402,F403
