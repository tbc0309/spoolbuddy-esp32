"""Apply the narrow ESP-Hosted 2.13.0 RX retry fix after dependency resolution."""
from pathlib import Path
import sys


def apply(component: Path) -> None:
    source = component / "host/drivers/transport/sdio/sdio_drv.c"
    text = source.read_text(encoding="utf-8")
    old_reset = "\t\tpending = false;\n\n\t\tSDIO_DRV_LOCK();"
    new_reset = ("\t\t/* Retrying unread bytes must not require a new, already cleared interrupt. */\n"
                 "\t\tbool retry_pending = pending;\n" + old_reset)
    old_gate = "if (!(BIT(SDIO_INT_NEW_PACKET) & interrupts)) {"
    new_gate = "if (!(BIT(SDIO_INT_NEW_PACKET) & interrupts) && !retry_pending) {"
    if new_reset in text and new_gate in text:
        return
    metadata = (component / "idf_component.yml").read_text(encoding="utf-8")
    if "version: 2.13.0" not in metadata or text.count(old_reset) != 1 or text.count(old_gate) != 1:
        raise RuntimeError("ESP-Hosted RX retry fix needs review for this dependency source")
    source.write_text(text.replace(old_reset, new_reset).replace(old_gate, new_gate), encoding="utf-8")


if __name__ == "__main__":
    apply(Path(sys.argv[1]))
