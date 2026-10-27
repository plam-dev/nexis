#!/usr/bin/env python3
"""Measure a native NEXIS process at rest. No synthetic activity or user config edits."""
import argparse
import json
import os
from pathlib import Path
import platform
import statistics
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary', type=Path, default=Path(__file__).resolve().parents[1] / 'build/nexis')
parser.add_argument('--duration', type=int, default=600)
parser.add_argument('--output', type=Path, default=Path('docs/benchmark-idle.json'))
args = parser.parse_args()
if args.duration < 1: parser.error('duration must be positive')

def sample(pid):
    text = Path(f'/proc/{pid}/stat').read_text().rsplit(')', 1)[1].split()
    ticks = int(text[11]) + int(text[12])
    rss = int(text[21]) * os.sysconf('SC_PAGE_SIZE') / 1024 / 1024
    return ticks, rss

process = subprocess.Popen([str(args.binary.resolve()), '--run-for', str((args.duration + 15) * 1000)], stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
try:
    time.sleep(2)
    if process.poll() is not None: raise RuntimeError(process.stderr.read().decode())
    start = time.monotonic()
    first, rss = sample(process.pid)
    memory = [rss]
    while time.monotonic() - start < args.duration:
        time.sleep(min(1, args.duration - (time.monotonic() - start)))
        final, rss = sample(process.pid)
        memory.append(rss)
    elapsed = time.monotonic() - start
    result = {'platform': platform.platform(), 'mode': 'Native Wayland capsule, no injected events; shell and collector share one process',
              'duration_seconds': round(elapsed, 3), 'cpu_percent_one_core': round((final - first) / os.sysconf('SC_CLK_TCK') / elapsed * 100, 4),
              'rss_mean_mib': round(statistics.mean(memory), 2), 'rss_peak_mib': round(max(memory), 2),
              'animation_fps': None, 'ui_event_p95_ms': None,
              'limitations': 'CPU/RSS include the combined UI/collector process, not GPU allocations or KWin helper cost. FPS and accepted-event-to-render latency need frame instrumentation.'}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
finally:
    process.terminate()
    _, errors = process.communicate(timeout=3)
    if errors: print(errors.decode())
