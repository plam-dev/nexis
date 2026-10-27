import importlib.util
import json
import pathlib
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('hooks', pathlib.Path(__file__).parents[2] / 'integrations/hooks.py')
hooks = importlib.util.module_from_spec(spec)
spec.loader.exec_module(hooks)

class HooksTest(unittest.TestCase):
    def test_install_preserves_hooks_is_idempotent_and_uninstalls_only_owned(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / 'settings.json'
            original = {'theme': 'dark', 'hooks': {'Stop': [{'hooks': [{'type': 'command', 'command': 'echo mine'}]}]}}
            path.write_text(json.dumps(original))
            hooks.update(path, 'qoder', '/tmp/Nexis Folder/nexis-hook-bridge', 'install', dry_run=True)
            self.assertEqual(json.loads(path.read_text()), original)
            hooks.update(path, 'qoder', '/tmp/Nexis Folder/nexis-hook-bridge', 'install')
            installed = path.read_bytes()
            hooks.update(path, 'qoder', '/tmp/Nexis Folder/nexis-hook-bridge', 'install')
            self.assertEqual(path.read_bytes(), installed)
            doc = json.loads(installed)
            self.assertEqual(doc['hooks']['Stop'][0], original['hooks']['Stop'][0])
            self.assertEqual(len(list(path.parent.glob('settings.json.nexis-*.bak'))), 1)
            hooks.update(path, 'qoder', '/tmp/Nexis Folder/nexis-hook-bridge', 'uninstall')
            self.assertEqual(json.loads(path.read_text()), original)

    def test_malformed_config_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as directory:
            p = pathlib.Path(directory) / 'settings.json'
            for raw in ['bad', '[]', '{"hooks": []}', '{"hooks":{"Stop":{}}}', '{"hooks":{"Stop":[{"hooks":null}]}}']:
                p.write_text(raw)
                with self.assertRaises((ValueError, TypeError)):
                    hooks.update(p, 'claude', '/tmp/bridge', 'install')
                self.assertEqual(p.read_text(), raw)

    def test_shared_config_provider_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            p = pathlib.Path(directory) / 'settings.json'
            hooks.update(p, 'qoder', '/tmp/bridge', 'install')
            doc = json.loads(p.read_text())
            self.assertIn('--provider qoder', doc['hooks']['Stop'][0]['hooks'][0]['command'])
            self.assertNotIn('qoder_cli', p.read_text())

if __name__ == '__main__': unittest.main()
