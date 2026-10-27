#!/usr/bin/env python3
"""Run under dbus-run-session: measure bridge and collector, not rendered UI latency."""
import argparse
import json
import os
from pathlib import Path
import statistics
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=Path('build'))
parser.add_argument('--output', type=Path, default=Path('docs/benchmark-bridge.json'))
args = parser.parse_args()
app, bridge = (args.build.resolve() / name for name in ('nexis', 'nexis-hook-bridge'))

def p95(values): return round(sorted(values)[int((len(values) - 1) * 0.95)], 3)

with tempfile.TemporaryDirectory() as directory:
    os.chmod(directory, 0o700)
    env = {**os.environ, 'XDG_RUNTIME_DIR': directory, 'XDG_CONFIG_HOME': directory + '/config'}
    payload = {'session_id': 'benchmark-session', 'cwd': '/benchmark/fixture', 'hook_event_name': 'UserPromptSubmit'}
    def send():
        start = time.perf_counter()
        result = subprocess.run([bridge, '--provider', 'qoder'], input=json.dumps(payload).encode(), capture_output=True, env=env, timeout=1)
        if result.returncode or result.stdout or result.stderr: raise RuntimeError('Bridge violated fail-open/output contract')
        return (time.perf_counter() - start) * 1000
    absent = [send() for _ in range(50)]
    daemon = subprocess.Popen([app, '--headless'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    try:
        for _ in range(100):
            if Path(directory + '/nexis/events.sock').exists(): break
            time.sleep(.01)
        healthy, latency = [], []
        for index in range(50):
            payload['hook_event_name'] = 'UserPromptSubmit' if index % 2 == 0 else 'Stop'
            wall = time.time_ns() // 1000000
            start = time.perf_counter()
            healthy.append(send())
            for _ in range(100):
                snapshot = json.loads(subprocess.check_output([app, '--snapshot'], env=env, timeout=1))
                if snapshot['sessions'] and snapshot['sessions'][0]['lastEvent'] == payload['hook_event_name'] and snapshot['sessions'][0]['received_ms'] >= wall: break
                time.sleep(.001)
            else: raise RuntimeError('Collector failed to publish accepted event')
            latency.append((time.perf_counter() - start) * 1000)
        result = {'samples_per_case': 50, 'healthy_bridge_p95_ms': p95(healthy), 'absent_collector_bridge_p95_ms': p95(absent),
                  'event_to_snapshot_roundtrip_p95_ms': p95(latency), 'rendered_ui_latency_ms': None,
                  'notes': 'Private session D-Bus and temporary runtime; includes process startup and snapshot query overhead. No events enter the live island. Not event-to-frame latency.'}
        args.output.write_text(json.dumps(result, indent=2) + '\n');print(json.dumps(result, indent=2))
    finally:
        daemon.terminate();daemon.communicate(timeout=2)
