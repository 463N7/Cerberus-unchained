"""Actual credential codec/NVS adapter with controlled persistence and failures."""
from pathlib import Path
import os
import struct
import subprocess
import sys
import zlib

root = Path(__file__).resolve().parents[1]
out = root / "artifacts/wardrive-upload-config-checks"
out.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, ZIG_GLOBAL_CACHE_DIR=str(out / "zig-global"), ZIG_LOCAL_CACHE_DIR=str(out / "zig-local"))
exe = out / ("config-check.exe" if os.name == "nt" else "config-check")
subprocess.run([sys.executable, "-m", "ziglang", "cc", "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
    "-I", str(root / "tools/host_checks/upload_stubs"), "-I", str(root / "tools/host_checks/stubs"),
    "-I", str(root / "s3_main/main"), str(root / "s3_main/main/wardrive_upload_config.c"),
    str(root / "s3_main/main/wardrive_upload_settings.c"), str(root / "tools/host_checks/wardrive_upload_config_check.c"),
    "-o", str(exe)], check=True, env=env)
wire = out / "empty-config.bin"
subprocess.run([str(exe), str(wire)], check=True, env=env)
expected = bytearray(512)
expected[:8] = b"CBUPCFG1"
struct.pack_into("<H", expected, 8, 487)
struct.pack_into("<I", expected, 12, zlib.crc32(expected))
assert wire.read_bytes() == expected
print("Upload configuration independent Python wire/CRC cross-check passed")
