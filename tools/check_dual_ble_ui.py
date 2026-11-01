"""Render and exercise the actual read-only C5 companion UI and target binding."""
from pathlib import Path
import os
import subprocess
import sys
root = Path(__file__).resolve().parents[1]
subprocess.run([sys.executable, str(root / "tools/check_s3_findmy_monitor_ui.py")], check=True)
out = root / "artifacts/dual-ble-ui-checks"
out.mkdir(parents=True, exist_ok=True)
(out / "esp_heap_caps.h").write_text("#pragma once\n")
ui = (root / "s3_main/main/ui.c").read_text()
begin = ui.index("static esp_err_t draw_ble_fox_hunt(void)")
(out / "dual_ble_fox_draw_impl.h").write_text(ui[begin:ui.index("static esp_err_t draw_ble_analyzer(void)", begin)])
exe = out / "check.exe"
env = dict(os.environ, ZIG_GLOBAL_CACHE_DIR=str(out / "zig-cache"))
subprocess.run([sys.executable, "-m", "ziglang", "cc", "-std=c11", "-O1", "-UNDEBUG",
    "-Wall", "-Wextra", "-Werror", "-I", str(out),
    "-I", str(root / "artifacts/findmy-monitor-ui-checks"),
    "-I", str(root / "tools/host_checks/stubs"), "-I", str(root / "s3_main/main"), "-I", str(root / "shared"),
    str(root / "tools/host_checks/dual_ble_ui_check.c"), str(root / "s3_main/main/dual_ble_scan.c"),
    str(root / "s3_main/main/privacy.c"), "-o", str(exe)], check=True, env=env)
subprocess.run([str(exe), str(out / "companion")], check=True)
