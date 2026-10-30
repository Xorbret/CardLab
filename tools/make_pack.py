#!/usr/bin/env python3
"""Bundle content/lessons and content/scenarios into one CardLab content pack.

Usage: make_pack.py <content dir> <version> <output file>
The format is documented in lib/cardlab_core/src/pack.h.
"""
import pathlib
import sys


def main() -> None:
    content, version, output = pathlib.Path(sys.argv[1]), sys.argv[2], pathlib.Path(sys.argv[3])
    out = bytearray(f"CARDLAB-PACK 1 {version}\n".encode())
    for folder in ("lessons", "scenarios"):
        for f in sorted((content / folder).glob("*.lua")):
            data = f.read_bytes()
            out += f"FILE {folder}/{f.name} {len(data)}\n".encode() + data + b"\n"
    out += b"END\n"
    output.write_bytes(out)


if __name__ == "__main__":
    main()
