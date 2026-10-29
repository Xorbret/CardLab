#!/usr/bin/env python3
"""Build data/cert/x509_crt_bundle.bin from the Mozilla root store (certifi).

The firmware embeds this file and uses it to verify HTTPS servers (content
updates). Format matches ESP-IDF's gen_crt_bundle.py, which Arduino-ESP32's
arduino_esp_crt_bundle_set() reads: a big-endian cert count, then for each
cert (sorted by subject) its subject DER and public key DER, each prefixed
by a big-endian length.

Usage: pip install certifi cryptography && python tools/gen_crt_bundle.py
"""
import pathlib
import struct

import certifi
from cryptography import x509
from cryptography.hazmat.primitives.serialization import Encoding, PublicFormat


def main() -> None:
    pem = pathlib.Path(certifi.where()).read_bytes()
    certs = x509.load_pem_x509_certificates(pem)
    entries = []
    for cert in certs:
        name = cert.subject.public_bytes()
        key = cert.public_key().public_bytes(Encoding.DER, PublicFormat.SubjectPublicKeyInfo)
        entries.append((name, key))
    entries.sort(key=lambda e: e[0])
    out = bytearray(struct.pack(">H", len(entries)))
    for name, key in entries:
        out += struct.pack(">HH", len(name), len(key)) + name + key
    dest = pathlib.Path(__file__).resolve().parent.parent / "data" / "cert" / "x509_crt_bundle.bin"
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(out)
    print(f"wrote {len(entries)} certificates ({len(out)} bytes) from certifi {certifi.__version__}")


if __name__ == "__main__":
    main()
