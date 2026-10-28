import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time
import unittest

BUILD = Path(os.environ.get('NEXIS_BUILD_DIR', Path(__file__).parents[2] / 'build'))
APP, BRIDGE = BUILD / 'nexis', BUILD / 'nexis-hook-bridge'

class TransportTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        os.chmod(self.temp.name, 0o700)
        self.env = {**os.environ, 'XDG_RUNTIME_DIR': self.temp.name, 'XDG_CONFIG_HOME': self.temp.name + '/config'}
        self.daemon = None

    def tearDown(self):
        if self.daemon:
            self.daemon.terminate()
            self.daemon.communicate(timeout=3)
        self.temp.cleanup()

    def bridge(self, event, provider='qoder'):
        return subprocess.run([BRIDGE, '--provider', provider], input=json.dumps(event).encode(), env=self.env, capture_output=True, timeout=1)

    def start(self):
        self.daemon = subprocess.Popen([APP, '--headless'], env=self.env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        for _ in range(100):
            if Path(self.temp.name + '/nexis/events.sock').exists(): return
            if self.daemon.poll() is not None: self.fail(self.daemon.communicate()[1].decode())
            time.sleep(0.01)
        self.fail('Collector failed to start')

    def snapshot(self):
        output = subprocess.check_output([APP, '--snapshot'], env=self.env, timeout=1)
        return json.loads(output)

    def test_missing_daemon_never_blocks_and_emits_no_output(self):
        begin = time.monotonic()
        result = self.bridge({'hook_event_name': 'Stop', 'session_id': 'one'})
        self.assertEqual((result.returncode, result.stdout, result.stderr), (0, b'', b''))
        self.assertLess(time.monotonic() - begin, 0.3)

    def test_stalled_stdin_and_oversized_input_exit_success(self):
        process = subprocess.Popen([BRIDGE, '--provider', 'qoder'], env=self.env, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        begin = time.monotonic()
        self.assertEqual(process.wait(timeout=0.3), 0)
        self.assertLess(time.monotonic() - begin, 0.3)
        self.assertEqual(process.communicate(), (b'', b''))
        result = self.bridge({'hook_event_name': 'Stop', 'prompt': 'x' * 70000})
        self.assertEqual((result.returncode, result.stdout, result.stderr), (0, b'', b''))

    def test_real_socket_redaction_and_parallel_sessions(self):
        self.start()
        for sid in ['one', 'two']:
            result = self.bridge({'hook_event_name': 'PermissionRequest', 'session_id': sid, 'cwd': '/private/secret/project', 'prompt': 'TOPSECRET', 'tool_input': {'command': 'TOPSECRET'}})
            self.assertEqual(result.returncode, 0)
        for _ in range(40):
            snapshot = self.snapshot()
            if len(snapshot['sessions']) == 2: break
            time.sleep(0.01)
        self.assertEqual(len(snapshot['sessions']), 2)
        self.assertNotIn('TOPSECRET', json.dumps(snapshot))
        self.assertNotIn('/private', json.dumps(snapshot))
        self.assertTrue(all(s['provider'] == 'qoder_app' for s in snapshot['sessions']))
        self.assertTrue(all(s['status'] == 'waiting_permission' for s in snapshot['sessions']))
        mode = Path(self.temp.name + '/nexis/events.sock').stat().st_mode & 0o777
        self.assertEqual(mode, 0o600)
        # A duplicate instance must not unlink the collector's live socket.
        duplicate = subprocess.run([APP, '--headless'], env=self.env, capture_output=True, timeout=1)
        self.assertNotEqual(duplicate.returncode, 0)
        self.assertEqual(len(self.snapshot()['sessions']), 2)

    def test_stalled_client_is_evicted_and_raw_events_are_rejected(self):
        self.start()
        client = socket.socket(socket.AF_UNIX)
        client.connect(self.temp.name + '/nexis/events.sock')
        client.sendall(b'{"prompt":"TOPSECRET"}\n')
        client.close()
        time.sleep(0.03)
        self.assertEqual(self.snapshot()['sessions'], [])
        client = socket.socket(socket.AF_UNIX)
        client.connect(self.temp.name + '/nexis/events.sock')
        client.sendall(b'{')
        client.settimeout(1)
        self.assertEqual(client.recv(1), b'')
        client.close()
        self.assertEqual(self.bridge({'hook_event_name': 'Stop', 'session_id': 'valid'}).returncode, 0)

    def test_claude_permission_resolves_across_directory_changes(self):
        self.start()
        base = {'session_id': 'claude-one', 'cwd': '/work/project', 'tool_name': 'Bash',
                'tool_input': {'command': 'echo TOPSECRET'}}
        for completion in ('PostToolUse', 'PostToolUseFailure'):
            with self.subTest(completion=completion):
                for name, cwd, tool_id, status in (
                    ('PreToolUse', '/work/project', 'tool-one', 'tool_running'),
                    ('PermissionRequest', '/work/project', None, 'waiting_permission'),
                    (completion, '/work/project/dist', 'tool-one', 'running'),
                    ('Stop', '/work/project/dist', None, 'turn_complete'),
                ):
                    raw = {**base, 'hook_event_name': name, 'cwd': cwd}
                    if tool_id:
                        raw['tool_use_id'] = tool_id
                    result = self.bridge(raw, provider='claude')
                    self.assertEqual((result.returncode, result.stdout, result.stderr), (0, b'', b''))
                    for _ in range(40):
                        snapshot = self.snapshot()
                        sessions = snapshot['sessions']
                        if len(sessions) == 1 and sessions[0]['lastEvent'] == name:
                            break
                        time.sleep(0.01)
                    self.assertEqual(len(sessions), 1)
                    self.assertEqual(sessions[0]['status'], status)
                    if name == 'PermissionRequest':
                        self.assertEqual(snapshot['presentation']['status'], 'waiting_permission')
                    else:
                        self.assertNotEqual(snapshot['presentation']['status'], 'waiting_permission')
                    self.assertNotIn('TOPSECRET', json.dumps(snapshot))
                self.assertEqual(snapshot['presentation']['status'], 'turn_complete')

if __name__ == '__main__': unittest.main()
