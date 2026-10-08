#!/usr/bin/env python3
"""Exercise the exact ShadowMount module with bounded mocked API/filesystems."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="fc27-shadowmount-test-") as temporary:
    binary = Path(temporary) / "shadowmount-test"
    subprocess.run([
        os.environ.get("CC", "cc"), "-O1", "-g", "-std=gnu11",
        "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
        "-DTR_HOST_TEST", "-DTR_SHADOWMOUNT_TEST", "-I", str(ROOT / "src"),
        str(ROOT / "src/shadowmount.c"), str(ROOT / "tests/shadowmount_unit.c"),
        "-o", str(binary),
    ], check=True)
    subprocess.run([str(binary)], check=True)
