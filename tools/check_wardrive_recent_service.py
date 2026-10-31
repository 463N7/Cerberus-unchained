"""Exercise the whole production Wardriving service with fake SDK/SD/radio edges.

Only includes are replaced. Lifecycle, capture, cursor, preview, formatting,
dedupe, outcome and cleanup function bodies remain the actual service source.
"""
from pathlib import Path
import os
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
out = root / "artifacts/wardrive-recent-service-checks"
out.mkdir(parents=True, exist_ok=True)
source = (root / "s3_main/main/wardrive_service.c").read_text(encoding="utf-8")
assert source.count("recent_begin_session();") == 1
body = re.sub(r'^#include .*\n', '', source, flags=re.M)
(out / "wardrive_recent_service_impl.h").write_text(body, encoding="utf-8")
storage = (root / "s3_main/main/storage.h").read_text(encoding="utf-8")
declarations = re.findall(r'esp_err_t cerberus_storage_wardrive\w*\([^;]*;', storage)
assert len(declarations) == 10
(out / "wardrive_recent_storage_decls.h").write_text("\n".join(declarations), encoding="utf-8")
exe = out / "wardrive-recent-service-check.exe"
env = dict(os.environ, ZIG_GLOBAL_CACHE_DIR=str(out / "zig-global"),
           ZIG_LOCAL_CACHE_DIR=str(out / "zig-local"))
subprocess.run([sys.executable, "-m", "ziglang", "cc", "-std=gnu11", "-O1",
    "-Wall", "-Wextra", "-Werror", "-I", str(out),
    "-I", str(root / "tools/host_checks/stubs"),
    "-I", str(root / "s3_main/main"), "-I", str(root / "shared"),
    str(root / "tools/host_checks/wardrive_recent_service_check.c"),
    *[str(root / "s3_main/main" / name) for name in
      ("wardrive_core.c", "wardrive_wigle.c", "wardrive_plan.c", "wardrive_recent.c")],
    "-o", str(exe)], check=True, env=env)
subprocess.run([str(exe)], check=True, env=env)
