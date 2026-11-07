"""Exercise the production observation FIFO and advertisement decoder."""
from pathlib import Path
import subprocess, sys, os
root=Path(__file__).resolve().parents[1]
out=root/'artifacts/wardrive-checks/stream-check.exe'
out.parent.mkdir(parents=True,exist_ok=True)
env=dict(os.environ)
env.setdefault('ZIG_GLOBAL_CACHE_DIR',str(out.parent/'zig-global'))
env.setdefault('ZIG_LOCAL_CACHE_DIR',str(out.parent/'zig-local'))
subprocess.run([sys.executable,'-m','ziglang','cc','-std=c11','-O1','-Wall','-Wextra','-Werror',
    '-I',str(root/'shared'),str(root/'tools/host_checks/wardrive_stream_check.c'),'-o',str(out)],check=True,env=env)
subprocess.run([str(out)],check=True)
