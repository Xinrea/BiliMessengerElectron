#!/usr/bin/env python3
"""Regenerate app icons from box.svg using Python 3 and rsvg-convert."""

import shutil
import struct
import subprocess
from pathlib import Path


def main():
    renderer = shutil.which("rsvg-convert")
    if renderer is None:
        raise SystemExit("rsvg-convert is required (brew install librsvg / apt install librsvg2-bin)")

    directory = Path(__file__).resolve().parent
    source = directory / "box.svg"
    sizes = (16, 24, 32, 48, 64, 128, 256, 512, 1024)
    images = {
        size: subprocess.check_output(
            [renderer, "--width", str(size), "--height", str(size), str(source)]
        )
        for size in sizes
    }

    # PNG-compressed ICO entries are supported by Windows Vista and later.
    ico_sizes = (16, 24, 32, 48, 64, 128, 256)
    offset = 6 + 16 * len(ico_sizes)
    entries = []
    for size in ico_sizes:
        image = images[size]
        dimension = size if size < 256 else 0
        entries.append(struct.pack("<BBBBHHII", dimension, dimension, 0, 0, 1, 32, len(image), offset))
        offset += len(image)
    ico = struct.pack("<HHH", 0, 1, len(ico_sizes))
    ico += b"".join(entries) + b"".join(images[size] for size in ico_sizes)

    # Modern ICNS PNG chunks, including the corresponding Retina sizes.
    icns_types = (
        (b"icp4", 16), (b"icp5", 32), (b"icp6", 64),
        (b"ic07", 128), (b"ic08", 256), (b"ic09", 512), (b"ic10", 1024),
        (b"ic11", 32), (b"ic12", 64), (b"ic13", 256), (b"ic14", 512),
    )
    chunks = b"".join(
        kind + struct.pack(">I", 8 + len(images[size])) + images[size]
        for kind, size in icns_types
    )
    icns = b"icns" + struct.pack(">I", 8 + len(chunks)) + chunks

    (directory / "box.png").write_bytes(images[256])
    (directory / "icon.ico").write_bytes(ico)
    (directory / "icon.icns").write_bytes(icns)


if __name__ == "__main__":
    main()
