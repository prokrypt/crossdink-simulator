#!/usr/bin/env python3
"""Check real simulator digest/storage implementations without SDL or firmware."""
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = os.environ.get("CXX", "c++")
flags = [compiler, "-std=c++17", "-O1", "-g", "-Wno-deprecated-declarations",
         "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-I" + str(root / "src")]
with tempfile.TemporaryDirectory(prefix="sim-asset-primitives-") as temp:
    temp = Path(temp)
    digest = temp / "digest"
    subprocess.run([*flags, str(root / "tests/sha256_check.cpp"), "-o", str(digest)], check=True)
    vectors = [b"", b"abc", b"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
               b"a" * 1_000_000, bytes(range(256)) * 4097]
    checks = 0
    for data in vectors:
        payload = temp / "payload"
        payload.write_bytes(data)
        for chunk in (1, 7, 55, 56, 63, 64, 65, 1024, 4096):
            actual = subprocess.check_output([str(digest), str(payload), str(chunk)], text=True).strip()
            expected = hashlib.sha256(data).hexdigest()
            assert actual == expected, (len(data), chunk, actual, expected)
            checks += 1
    storage_object = temp / "storage.o"
    subprocess.run([*flags, "-Dreaddir=simulatorTestReadDirectory", "-Dopendir=simulatorTestOpenDirectory", "-c", str(root / "src/HalStorage.cpp"),
                    "-o", str(storage_object)], check=True)
    storage = temp / "storage"
    subprocess.run([*flags, str(root / "tests/storage_iteration_check.cpp"), str(storage_object), "-o", str(storage)], check=True)
    subprocess.run([str(storage), str(temp / "sd")], check=True)
    http = temp / "http"
    subprocess.run([*flags, str(root / "tests/http_mock_check.cpp"), "-o", str(http)], check=True)
    subprocess.run([str(http), str(temp / "mock")], check=True)
    print(f"PASS: {checks} SHA-256 vectors/chunkings, context restart, directory EOF/I/O/stat/open/move/reopen/hidden-file and capacity checks (ASan/UBSan)")
