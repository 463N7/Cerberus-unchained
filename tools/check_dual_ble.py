"""Exercise the production bounded C5 companion reader and owner protocol."""
from pathlib import Path
import os
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
out = root / "artifacts/dual-ble-checks"
out.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, ZIG_GLOBAL_CACHE_DIR=str(out / "zig-cache"))
source = (root / "c5_firmware/main/ble_scanner.c").read_text()
begin = source.index("static void record_advertisement(")
end = source.index("static void finish_scan_for_epoch(", begin)
(out / "c5_ble_callback_impl.h").write_text(source[begin:end])
for fixture, extra in (("dual_ble_check", [root / "s3_main/main/dual_ble_scan.c"]),
                       ("dual_ble_owner_check", [])):
    exe = out / f"{fixture}.exe"
    subprocess.run([
        sys.executable, "-m", "ziglang", "cc", "-std=c11", "-O1", "-UNDEBUG",
        "-Wall", "-Wextra", "-Werror", "-I", str(out),
        "-I", str(root / "tools/host_checks/stubs"), "-I", str(root / "shared"),
        "-I", str(root / "s3_main/main"), "-I", str(root / "c5_firmware/main"),
        str(root / f"tools/host_checks/{fixture}.c"), *map(str, extra), "-o", str(exe),
    ], check=True, env=env)
    subprocess.run([str(exe)], check=True)
