"""Assemble the tested dual-application layout from a local factory application."""

import argparse
from hashlib import sha256
from pathlib import Path
import struct
from zlib import crc32

from keystone import KS_ARCH_ARM, KS_MODE_THUMB, Ks


FACTORY_BASE = 0x1C000
ZMK_BASE = 0x34000
ZMK_LIMIT = 0x7F000
IMAGE_LIMIT = 0x80000
FACTORY_SHA256 = "a36bf594b11375679ab061ac82745f4a43ae1b53254cf86914758a05c6e1dcc3"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--factory-app", type=Path, required=True)
    parser.add_argument("--zmk-app", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    factory = bytearray(args.factory_app.read_bytes())
    zmk = args.zmk_app.read_bytes()
    if sha256(factory).hexdigest() != FACTORY_SHA256:
        parser.error("Expected the raw Zaopin Z1 Pro V2.17 application with the documented SHA-256")
    if not 256 <= len(zmk) <= ZMK_LIMIT - ZMK_BASE:
        parser.error("ZMK application does not fit the secondary slot")
    stack, entry = struct.unpack_from("<II", zmk)
    if not (0x20000000 < stack <= 0x20040000 and stack % 8 == 0
            and entry & 1 and ZMK_BASE < entry < ZMK_BASE + len(zmk)):
        parser.error("Expected an nRF52840 ZMK application linked at 0x34000")

    boot = Path(__file__).resolve().parents[1] / "boot"
    assembler = Ks(KS_ARCH_ARM, KS_MODE_THUMB)
    selector = bytes(assembler.asm((boot / "hybrid_selector.s").read_text(), addr=0x1C100)[0])
    relay = bytes(assembler.asm((boot / "rf_to_ble_relay.s").read_text(), addr=0x7FE00)[0])
    if len(selector) > 0x100 or len(relay) > 0x1F0:
        parser.error("Selector or relay exceeds its allocation")
    if factory[0x100:0x200] != bytes(0x100) or factory[0x634C:0x6350] != bytes.fromhex("50d14ce0"):
        parser.error("Factory selector and relay sites do not match the tested revision")

    struct.pack_into("<I", factory, 4, 0x1C101)
    factory[0x100:0x100 + len(selector)] = selector
    factory[0x634C:0x6350] = bytes(assembler.asm("b.w 0x7fe00", addr=0x2234C)[0])
    image = bytearray(b"\xff" * (IMAGE_LIMIT - FACTORY_BASE))
    image[:len(factory)] = factory
    image[ZMK_BASE - FACTORY_BASE:ZMK_BASE - FACTORY_BASE + len(zmk)] = zmk
    image[0x7FE00 - FACTORY_BASE:0x7FE00 - FACTORY_BASE + len(relay)] = relay
    struct.pack_into("<4I", image, 0x7FFF0 - FACTORY_BASE,
                     0x31454C42, ZMK_BASE, len(zmk), crc32(zmk))

    with args.output.open("xb") as output:
        output.write(image)
    print(f"Raw application region at 0x{FACTORY_BASE:X}, {len(image)} bytes")
    print(f"SHA-256 {sha256(image).hexdigest()}")


if __name__ == "__main__":
    main()
