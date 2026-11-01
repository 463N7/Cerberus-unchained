"""Exercise actual focused BLE cards, frozen selection, detail and Fox Hunt routes."""
from pathlib import Path
import os
import re
import struct
import subprocess
import sys
import zlib

root = Path(__file__).resolve().parents[1]
native = "--native-live" in sys.argv
subprocess.run([sys.executable, str(root / "tools/check_s3_findmy_monitor_ui.py")], check=True)
out = root / "artifacts/ble-discovery-ui-checks"
out.mkdir(parents=True, exist_ok=True)
ui = (root / "s3_main/main/ui.c").read_text()
live = (root / "s3_main/main/sentinel_live_ui.h").read_text()
discovery = (root / "s3_main/main/s3_ble_discovery_live_ui.h").read_text()
companion = (root / "s3_main/main/dual_ble_ui.h").read_text()
(out / "discovery_back_geometry.h").write_text("\n".join(
    re.search(r"^#define " + name + r"\s+\d+", ui, re.M).group(0)
    for name in ("UI_BACK_Y", "UI_BACK_HEIGHT")) + "\n")
assert "!ble_discovery_defers_c5_telemetry()" in ui, "Periodic C5 telemetry must use the production discovery gate"
assert "!(wifi_relation_ui && wifi_relation_ui->reading)" in ui, "Relation transfers must defer optional C5 telemetry"
def between(source, first, after):
    begin = source.index(first)
    return source[begin:source.index(after, begin)]

def block(source, start):
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]

def function(source, name):
    match = re.search(r"^static [^\n]*\b" + re.escape(name) + r"\([^;]*?\)\s*\{", source, re.M)
    assert match, name
    return block(source, match.start())

# Use production copied-selection/state helpers. RF lifecycle remains the
# counted boundary, while committed row/provenance getters are linked below.
companion_parts = between(companion, "enum {", "static uint64_t dual_ble_ui_clock(")
companion_parts += "\n".join(function(companion, name) for name in (
    "dual_ble_ui_get_view", "dual_ble_ui_open", "dual_ble_ui_select_frozen",
    "dual_ble_ui_selected_peer", "dual_ble_ui_tap"))
(out / "discovery_companion_impl.h").write_text(companion_parts)

# Full production view builder and live tap handler: no second index map,
# row binding, or hit-test implementation in the fixture.
parts = between(live, "typedef struct {", "static sentinel_scan_session_t live_session;")
(out / "discovery_live_types.h").write_text(parts)
(out / "discovery_native_types.h").write_text(between(discovery, "typedef struct {", "static ble_discovery_live_store_t *"))
parts = between(ui, "static const char *wifi_scan_coverage_text(", "static bool wifi_history_same_identity(")
parts += between(ui, "static const char *ble_filter_name(", "static esp_err_t load_ble_results(")
parts += between(live, "static live_scan_record_t *live_display_record(", "static bool live_has_snapshot(")
parts += between(live, "static bool live_is_view(", "static void live_status(")
parts += between(live, "static uint32_t live_ble_recent_mask(", "static void live_make_view(")
parts += between(live, "static void live_make_view(", "static void live_scan_tick(")
parts += between(live, "static void live_scan_tick(", "static void live_reset_observations(")
parts += between(live, "static void handle_live_scan_tap(", "static esp_err_t live_connect_selected_wifi(")
# Preserve all dispatcher guards before Fox Hunt, then the exact Detail
# branches through TRACK RSSI. Unrelated menu/GATT controls are outside scope.
parts += between(ui, "static void handle_ble_tap(", "    if (ble_mode == UI_BLE_MODE_MENU)")
start = ui.index("    if (ble_mode == UI_BLE_MODE_DETAIL) {", ui.index("static void handle_ble_tap("))
end = ui.index("            bool selected_connected =", start)
parts += ui[start:end] + "        }\n    }\n}\n"
parts += between(ui, "static void ble_fox_hunt_finish_exit(", "static void ble_analyzer_finish_exit(")
parts += between(ui, "static void ble_analyzer_finish_exit(", "static bool ble_sound_terminal(")
parts += between(ui, "static esp_err_t draw_ble_analyzer(", "static esp_err_t draw_ble_feature_menu(")
metadata_start = ui.index("    if (cerberus_ble_rssi_valid(", ui.index("    format_ble_service(&ble_selected_result, service, sizeof(service));"))
metadata_end = ui.index('\n', ui.index('TAG, "BLE detail metadata failed");', metadata_start))
parts += "static esp_err_t actual_ble_peak_metadata(void) { char line[64]; bool connectable=(ble_selected_result.flags&CERBERUS_BLE_RESULT_CONNECTABLE)!=0;\n" + ui[metadata_start:metadata_end] + "\nreturn ESP_OK; }\n"
back_start = ui.index("            } else if (current_page == UI_PAGE_BLE && ble_mode == UI_BLE_MODE_FOX_HUNT)", ui.index("static void handle_tap("))
back_end = ui.index("            } else if", back_start + 20)
assert "UI_BLE_MODE_ANALYZER" in ui[back_end:back_end + 130]
back_end = ui.index("            } else if", back_end + 20)
detail_start = ui.index("            } else if (current_page == UI_PAGE_BLE && ble_mode == UI_BLE_MODE_DETAIL)", back_end)
detail_end = ui.index("            } else if", detail_start + 20)
list_start = ui.index("            } else if (current_page == UI_PAGE_BLE && ble_mode == UI_BLE_MODE_LIST)", detail_end)
list_end = ui.index("            } else if", list_start + 20)
parts += "static void actual_back(uint16_t y) { if(y < UI_BACK_Y)return;\n" + ui[back_start:back_end].replace("} else if", "if", 1) + ui[detail_start:detail_end] + ui[list_start:list_end] + "\n}\n}\n"
live_back_start = ui.index("            } else if (live_is_view()) {", ui.index("static void handle_tap("))
live_back_end = ui.index("            } else if", live_back_start + 20)
parts += "static void actual_live_list_back(uint16_t y) { if(y < UI_BACK_Y)return;\n" + ui[live_back_start:live_back_end].replace("} else if", "if", 1) + "\n}\n}\n"
footer_start = ui.index("    } else if (current_page == UI_PAGE_BLE && ble_mode == UI_BLE_MODE_ANALYZER)", ui.index("static esp_err_t draw_detail_page("))
footer_end = ui.index("    } else if", footer_start + 20)
parts += "static esp_err_t actual_analyzer_footer(void) { const char *back_label = \"BACK\";\n" + ui[footer_start:footer_end].replace("} else if", "if", 1) + "}\n"
parts += re.search(r"    return draw_button\(64, UI_BACK_Y, 192, UI_BACK_HEIGHT, back_label\);", ui).group(0) + "\n}\n"
dual_live = (root / "s3_main/main/dual_ble_live_ui.h").read_text()
parts += between(dual_live, "static bool dual_ble_live_discovery_released(", "static void dual_ble_live_tick(")
footer_start = ui.index("if (!ui_capability_blocked && current_page == UI_PAGE_BLE && ble_mode == UI_BLE_MODE_C5 &&")
parts += "\nstatic void actual_c5_footer_tap(uint16_t x,uint16_t y) {\n" + block(ui, footer_start) + "\n}\n"
footer_start = ui.index("if (current_page == UI_PAGE_BLE && ble_mode == UI_BLE_MODE_C5) {")
parts += "\nstatic esp_err_t actual_c5_footer_draw(void) {\n" + block(ui, footer_start) + "\nreturn ESP_OK;\n}\n"
(out / "discovery_routes_impl.h").write_text(parts)
exe = out / ("ble-discovery-native-check.exe" if native else "ble-discovery-ui-check.exe")
env = os.environ.copy()
env["ZIG_GLOBAL_CACHE_DIR"] = str(out / "zig-cache")
env["ZIG_LOCAL_CACHE_DIR"] = str(out / "zig-local")
subprocess.run([sys.executable, "-m", "ziglang", "cc", "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
    "-I", str(out), "-I", str(root / "artifacts/findmy-monitor-ui-checks"),
    "-I", str(root / "tools/host_checks/stubs"), "-I", str(root / "s3_main/main"), "-I", str(root / "shared"),
    str(root / "tools/host_checks" / ("s3_ble_discovery_live_check.c" if native else "s3_ble_discovery_ui_check.c")), str(root / "s3_main/main/privacy.c"),
    str(root / "s3_main/main/sentinel_scan_view.c"), str(root / "s3_main/main/sentinel_scan_session.c"),
    str(root / "s3_main/main/sentinel_watch_detect.c"),
    str(root / "s3_main/main/s3_wifi_list.c"),
    str(root / "s3_main/main/ble_analyzer.c"),
    str(root / "s3_main/main/s3_ble_discovery_session.c"),
    str(root / "s3_main/main/s3_ble_discovery_inventory.c"),
    str(root / "s3_main/main/s3_ble_snapshot.c"),
    str(root / "s3_main/main/dual_ble_scan.c"),
    str(root / "shared/cerberus_watch_capture.c"),
    str(root / "shared/sentinel_watch_protocol.c"),
    "-o", str(exe)], check=True, env=env)
subprocess.run([str(exe), str(out / ("live" if native else "flipper"))], check=True)

def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
for path in out.glob(("live" if native else "flipper") + "-*.ppm"):
    data = path.read_bytes(); header = b"P6\n320 480\n255\n"
    assert data.startswith(header) and len(data) == len(header) + 320 * 480 * 3
    pixels = data[len(header):]
    path.with_suffix(".png").write_bytes(b"\x89PNG\r\n\x1a\n" +
        chunk(b"IHDR", struct.pack(">IIBBBBB", 320, 480, 8, 2, 0, 0, 0)) +
        chunk(b"IDAT", zlib.compress(b"".join(b"\0" + pixels[y*960:(y+1)*960] for y in range(480)))) + chunk(b"IEND", b""))
print(out)
