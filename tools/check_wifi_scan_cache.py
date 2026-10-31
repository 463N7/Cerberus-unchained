"""Exercise actual no-I/O AP publication hydration for legacy detail views."""
from pathlib import Path
import os
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
out = root / 'artifacts/wifi-scan-cache-checks'
out.mkdir(parents=True, exist_ok=True)
source = (root / 's3_main/main/ui.c').read_text(encoding='utf-8')

def function(signature):
    start = source.index(signature)
    body = source.index('{', start)
    depth = 1
    end = body + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

(out / 'cache_impl.h').write_text('\n\n'.join(function(s) for s in (
    'static void make_ssid_display_safe(',
    'static void record_wifi_channel_history(',
    'static void wifi_commit_cached_results(',
)), encoding='utf-8')
env = os.environ.copy()
env['ZIG_GLOBAL_CACHE_DIR'] = str(out / 'zig-cache')
exe = out / 'wifi-scan-cache-check.exe'
subprocess.run([sys.executable, '-m', 'ziglang', 'cc', '-std=c11', '-O1',
    '-Wall', '-Wextra', '-Werror', '-I', str(out),
    '-I', str(root / 'tools/host_checks/stubs'), '-I', str(root / 'shared'),
    '-I', str(root / 's3_main/main'), str(root / 'tools/host_checks/wifi_scan_cache_check.c'),
    '-o', str(exe)], check=True, env=env)
subprocess.run([str(exe)], check=True, env=env)
