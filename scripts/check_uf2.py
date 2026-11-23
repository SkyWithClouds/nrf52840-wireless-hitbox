"""Check the application's UF2 against its linked binary and target memory map."""
import struct
import sys
from pathlib import Path

APP_START, APP_LIMIT = 0x26000, 0xE8000
FAMILY = 0xADA52840


def check(uf2, binary, pico=False):
    start, limit, family_id = (0x10000000, 0x10200000, 0xE48BFF56) if pico else (APP_START, APP_LIMIT, FAMILY)
    vector_offset = 256 if pico else 0  # RP2040 boot2 occupies the first page
    if not uf2 or len(uf2) % 512 or len(binary) < vector_offset + 8:
        raise ValueError("empty or truncated build output")
    payload = bytearray()
    blocks = len(uf2) // 512
    for i in range(blocks):
        block = uf2[i * 512:(i + 1) * 512]
        magic0, magic1, flags, addr, size, index, count, family = struct.unpack_from("<8I", block)
        if (magic0, magic1, block[-4:]) != (0x0A324655, 0x9E5D5157, b"\x30\x6f\xb1\x0a"):
            raise ValueError(f"bad magic in block {i}")
        if (flags, index, count, family) != (0x2000, i, blocks, family_id):
            raise ValueError(f"bad UF2 metadata in block {i}")
        if size != 256 or addr != start + len(payload) or addr + size > limit:
            raise ValueError(f"unexpected write at {addr:#x} in block {i}")
        payload.extend(block[32:32 + size])
    if len(payload) != (len(binary) + 255) // 256 * 256 or payload[:len(binary)] != binary:
        raise ValueError("UF2 payload does not match zephyr.bin")
    sp, reset = struct.unpack_from("<2I", binary, vector_offset)
    if not (0x20000000 < sp <= (0x20042000 if pico else 0x20040000) and sp % 8 == 0 and reset & 1 and
            start + vector_offset <= reset - 1 < start + len(binary)):
        raise ValueError("invalid linked stack/reset vector")
    print(f"Verified {blocks} blocks, family {family_id:#x}, start {start:#x}, reset {reset:#x}")


if __name__ == "__main__":
    check(Path(sys.argv[1]).read_bytes(), Path(sys.argv[2]).read_bytes(), sys.argv[3:] == ["--pico"])
