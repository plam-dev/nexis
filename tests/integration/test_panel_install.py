import importlib.util
from pathlib import Path
import unittest
import os
import json
import subprocess
import tempfile
from unittest.mock import patch

spec=importlib.util.spec_from_file_location('panel_install',Path(__file__).parents[2]/'scripts/install-panel.py')
install=importlib.util.module_from_spec(spec)
spec.loader.exec_module(install)

class PanelInstallTest(unittest.TestCase):
    def test_migration_disables_old_autostart_and_recovers_failures(self):
        inventory=[{'id':200,'location':'top','widgets':[]}]
        for failure, previous in ((None,'nexis.service'),('install','nexis.service'),('start','nexis.service'),('placement','nexis.service'),('placement','nexis-panel.service'),(None,None),('placement',None)):
            with self.subTest(failure=failure, previous=previous), tempfile.TemporaryDirectory() as directory:
                root=Path(directory)
                build=root/'build'
                plugin=build/'plasmoid'/install.PLUGIN/'contents/ui/Nexis/Panel/libnexispanelplugin.so'
                plugin.parent.mkdir(parents=True)
                plugin.touch()
                prefix=root/'local'
                (prefix/'bin').mkdir(parents=True)
                (prefix/'bin/nexis').write_text('previous runtime')
                unit=root/'config/systemd/user/nexis-panel.service'
                unit.parent.mkdir(parents=True)
                if previous is not None: unit.write_text('previous unit')
                calls=[]
                def run(args, **kwargs):
                    calls.append(args)
                    if args[0]=='cmake':
                        (prefix/'bin/nexis').write_text('new runtime')
                        if failure=='install': raise subprocess.CalledProcessError(1,args)
                    if failure=='start' and args==['systemctl','--user','enable','--now','nexis-panel.service']:
                        raise subprocess.CalledProcessError(1,args)
                    if 'is-enabled' in args:
                        return subprocess.CompletedProcess(args,0 if args[-1]==previous else 1,stdout='enabled\n' if args[-1]==previous else ('disabled\n' if previous else 'not-found\n'))
                    if 'is-active' in args:
                        return subprocess.CompletedProcess(args,0 if args[-1]==previous else 3)
                    return subprocess.CompletedProcess(args,0)
                def evaluate(script):
                    if script==install.INVENTORY:return json.dumps(inventory)
                    if failure=='placement':raise RuntimeError('placement failed')
                    return json.dumps({'panel':200,'widget':230,'index':0})
                env={'XDG_CONFIG_HOME':str(root/'config'),'XDG_STATE_HOME':str(root/'state')}
                with patch.dict(os.environ,env), patch.object(install,'evaluate',side_effect=evaluate), patch.object(install.subprocess,'run',side_effect=run), patch.object(install,'stop_owned_runtime',return_value=None):
                    if failure:
                        with self.assertRaises((subprocess.SubprocessError,RuntimeError)):install.apply(build,prefix)
                        self.assertEqual((prefix/'bin/nexis').read_text(),'previous runtime')
                        if previous:
                            self.assertEqual(unit.read_text(),'previous unit')
                            self.assertIn(['systemctl','--user','start',previous],calls)
                        else: self.assertFalse(unit.exists())
                        if previous != 'nexis-panel.service': self.assertIn(['systemctl','--user','disable','nexis-panel.service'],calls)
                    else:
                        install.apply(build,prefix)
                        if previous: self.assertIn(['systemctl','--user','disable','nexis.service'],calls)
                        self.assertIn(['systemctl','--user','enable','--now','nexis-panel.service'],calls)
    def test_insert_between_existing_spacers_preserves_everything(self):
        widgets=[{'id':1,'type':'launcher','index':0},{'id':2,'type':'org.kde.plasma.panelspacer','index':1},{'id':3,'type':'org.kde.plasma.panelspacer','index':2},{'id':4,'type':'tray','index':3}]
        result=install.plan([{'id':200,'location':'top','widgets':widgets}])
        self.assertEqual(result['panel'],200)
        self.assertEqual(result['index'],2)
        self.assertIsNone(result['existing'])
        self.assertEqual(len(widgets),4)
    def test_repeat_reuses_widget_and_never_adds_to_bottom(self):
        panel={'id':200,'location':'top','widgets':[{'id':90,'type':'org.nexis.panel','index':2}]}
        self.assertEqual(install.plan([panel])['existing'],90)
        with self.assertRaises(ValueError):install.plan([{'id':195,'location':'bottom','widgets':[]}])
    def test_multiple_top_panels_prefer_active_screen_zero(self):
        result=install.plan([{'id':2,'location':'top','screen':1,'widgets':[]},{'id':1,'location':'top','screen':0,'widgets':[]}])
        self.assertEqual(result['panel'],1)

if __name__=='__main__':unittest.main()
