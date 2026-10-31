"""Exercise the real portable BLE archive codec without SDK, filesystem, or RF."""
from pathlib import Path
import os
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
out = root / "artifacts/ble-archive-checks"
out.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, ZIG_GLOBAL_CACHE_DIR=str(out / "zig-cache"),
           ZIG_LOCAL_CACHE_DIR=str(out / "zig-local-cache"))
exe = out / ("ble-archive-check.exe" if os.name == "nt" else "ble-archive-check")
subprocess.run([sys.executable, "-m", "ziglang", "cc", "-std=c11", "-O1",
    "-Wall", "-Wextra", "-Werror", "-I", str(root / "s3_main/main"),
    str(root / "tools/host_checks/ble_archive_check.c"),
    str(root / "s3_main/main/ble_archive.c"), "-o", str(exe)], check=True, env=env)
subprocess.run([str(exe)], check=True, env=env)
