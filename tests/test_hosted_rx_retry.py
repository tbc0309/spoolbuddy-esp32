"""The dependency fix must survive clean resolution and repeated configuration."""
import importlib.util
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("rx_retry", ROOT / "patches/esp_hosted_rx_retry/apply.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def test_clean_dependency_and_reconfigure(tmp_path):
    source = tmp_path / "host/drivers/transport/sdio/sdio_drv.c"
    source.parent.mkdir(parents=True)
    source.write_text("\t\tpending = false;\n\n\t\tSDIO_DRV_LOCK();\n"
                      "if (!(BIT(SDIO_INT_NEW_PACKET) & interrupts)) {\n", encoding="utf-8")
    (tmp_path / "idf_component.yml").write_text("version: 2.13.0\n", encoding="utf-8")
    module.apply(tmp_path)
    patched = source.read_text(encoding="utf-8")
    assert patched.index("bool retry_pending = pending;") < patched.index("pending = false;")
    assert "&& !retry_pending" in patched
    module.apply(tmp_path)
    assert source.read_text(encoding="utf-8") == patched


def test_changed_dependency_requires_review(tmp_path):
    source = tmp_path / "host/drivers/transport/sdio/sdio_drv.c"
    source.parent.mkdir(parents=True)
    source.write_text("unknown implementation", encoding="utf-8")
    (tmp_path / "idf_component.yml").write_text("version: 3.0.0\n", encoding="utf-8")
    with pytest.raises(RuntimeError, match="needs review"):
        module.apply(tmp_path)
    assert source.read_text(encoding="utf-8") == "unknown implementation"
