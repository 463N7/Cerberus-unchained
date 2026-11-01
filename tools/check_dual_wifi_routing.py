"""Compile actual root UI entry blocks so ordinary/special routes stay distinct."""
from pathlib import Path
import os
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
out = root / "artifacts/dual-wifi-routing-checks"
out.mkdir(parents=True, exist_ok=True)
source = (root / "s3_main/main/ui.c").read_text(encoding="utf-8")


def block(needle: str, after: int = 0) -> str:
    start = source.index(needle, after)
    opening = source.index("{", start)
    depth = 1
    index = opening + 1
    while depth:
        depth += (source[index] == "{") - (source[index] == "}")
        index += 1
    return source[start:index]


open_at = source.index("static void open_tool_page(ui_page_t page)\n{")
tap_at = source.index("static void handle_wifi_tap(uint16_t x, uint16_t y)\n{")
guide_at = source.index("    if (current_page == UI_PAGE_GUIDE) {", open_at)
generated = "static void open_tool_page(unsigned page) { current_page=page;\n"
generated += block("if (current_page == UI_PAGE_WIFI) {", open_at) + "\n}\n"
generated += "static void root_wifi_tap(uint16_t x,uint16_t y) {\n"
generated += block("if (wifi_mode == UI_WIFI_MODE_DUAL)", tap_at) + "\n"
generated += block("if (wifi_mode == UI_WIFI_MODE_MENU) {", tap_at) + "\n}\n"
generated += "static void root_guide(unsigned action) {\n"
generated += block("if (action == SENTINEL_GUIDE_OPEN_WIFI ||", guide_at) + "\n}\n"
generated += "static void root_touch(uint16_t x,uint16_t y) {\n"
generated += block("if (!ui_capability_blocked && current_page == UI_PAGE_WIFI && wifi_mode == UI_WIFI_MODE_DUAL)", guide_at)
generated += "\n}\n"
(out / "dual_wifi_root_routes.h").write_text(generated, encoding="utf-8")
env = dict(os.environ, ZIG_GLOBAL_CACHE_DIR=str(out / "zig-global"),
           ZIG_LOCAL_CACHE_DIR=str(out / "zig-local"))
exe = out / ("dual-wifi-routing-check.exe" if os.name == "nt" else "dual-wifi-routing-check")
subprocess.run([sys.executable, "-m", "ziglang", "cc", "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
    "-I", str(out), str(root / "tools/host_checks/dual_wifi_routing_check.c"), "-o", str(exe)],
    check=True, env=env)
subprocess.run([str(exe)], check=True, env=env)
