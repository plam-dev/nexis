#!/usr/bin/env python3
"""Install NEXIS into the existing top Plasma panel, preserving other widgets."""
import argparse
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import time
import tempfile

PLUGIN = 'org.nexis.panel'
SPACER = 'org.kde.plasma.panelspacer'
ROOT = Path(__file__).resolve().parents[1]
INVENTORY = '''print(JSON.stringify(panels().map(function(p){return {id:p.id,location:p.location,screen:p.screen,height:p.height,widgets:p.widgets().map(function(w){return {id:w.id,type:w.type,index:w.index}})}})))'''

def plan(panels):
    top = [p for p in panels if p.get('location') == 'top']
    if not top: raise ValueError('No top panel found. Add a top KDE panel first.')
    panel = sorted(top, key=lambda p: (p.get('screen', 0) != 0, p['id']))[0]
    widgets = sorted(panel['widgets'], key=lambda w: w['index'])
    existing = next((w for w in widgets if w['type'] == PLUGIN), None)
    spacers = [w for w in widgets if w['type'] == SPACER]
    if existing: index = existing['index']
    elif len(spacers) >= 2: index = spacers[0]['index'] + 1
    elif spacers: index = spacers[0]['index'] + 1
    else: index = len(widgets) // 2
    return {'panel': panel['id'], 'index': index, 'existing': existing['id'] if existing else None}

def environment():
    env = dict(os.environ)
    env.setdefault('XDG_RUNTIME_DIR', '/run/user/' + str(os.getuid()))
    env.setdefault('DBUS_SESSION_BUS_ADDRESS', 'unix:path=' + env['XDG_RUNTIME_DIR'] + '/bus')
    return env

def evaluate(script):
    qdbus = shutil.which('qdbus6') or '/usr/lib64/qt6/bin/qdbus'
    if not Path(qdbus).exists(): qdbus = shutil.which('qdbus')
    if not qdbus: raise RuntimeError('Qt6 qdbus is required for live panel placement.')
    return subprocess.check_output([qdbus, 'org.kde.plasmashell', '/PlasmaShell', 'org.kde.PlasmaShell.evaluateScript', script], env=environment(), text=True, timeout=10).strip()

SERVICES = ('nexis.service', 'nexis-panel.service')

def service_states():
    result = {}
    for name in SERVICES:
        enabled = subprocess.run(['systemctl','--user','is-enabled',name], env=environment(), capture_output=True, text=True, timeout=5)
        active = subprocess.run(['systemctl','--user','is-active','--quiet',name], env=environment(), timeout=5)
        result[name] = {'enabled': enabled.stdout.strip(), 'active': active.returncode == 0}
    return result

def restore_services(states):
    for name, previous in states.items():
        if previous['enabled'] in ('', 'not-found'): continue
        command = ['systemctl','--user']
        if previous['enabled'] in ('enabled', 'enabled-runtime'):
            command += ['enable']
            if previous['enabled'] == 'enabled-runtime': command += ['--runtime']
        else: command += ['disable']
        subprocess.run(command+[name], env=environment(), check=True, timeout=10)
    for name, previous in states.items():
        if previous['active']:
            subprocess.run(['systemctl','--user','start',name], env=environment(), check=True, timeout=10)

def stop_owned_runtime(prefix, build):
    subprocess.run(['systemctl','--user','stop','nexis-panel.service'], env=environment(), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=5)
    active = subprocess.run(['systemctl','--user','is-active','--quiet','nexis.service'], env=environment(), timeout=5)
    if active.returncode == 0:
        subprocess.run(['systemctl','--user','stop','nexis.service'], env=environment(), check=True, timeout=5)
    lock = Path(environment()['XDG_RUNTIME_DIR']) / 'nexis/instance.lock'
    try:
        pid = int(lock.read_text().splitlines()[0])
        executable = Path(os.readlink(f'/proc/{pid}/exe').removesuffix(' (deleted)'))
    except (OSError, ValueError, IndexError): return
    if executable not in {(build/'nexis').resolve(), (prefix/'bin/nexis').resolve()}:
        raise RuntimeError('Another NEXIS executable owns the collector. Close it before migration.')
    invocation = Path(f'/proc/{pid}/cmdline').read_bytes().rstrip(b'\0').split(b'\0')
    os.kill(pid, signal.SIGTERM)
    for _ in range(100):
        if not Path(f'/proc/{pid}').exists(): return invocation
        # A terminated, unreaped child is no longer holding sockets or locks.
        try:
            if Path(f'/proc/{pid}/stat').read_text().rsplit(')',1)[1].split()[0] == 'Z': return invocation
        except OSError: return invocation
        time.sleep(.02)
    raise RuntimeError('The old NEXIS runtime did not exit; panel changes were not applied.')

def apply(build, prefix):
    inventory = json.loads(evaluate(INVENTORY))
    insertion = plan(inventory)
    package = build/'plasmoid'/PLUGIN
    if not (package/'contents/ui/Nexis/Panel/libnexispanelplugin.so').is_file():
        raise RuntimeError('Build the nexispanelplugin target before installing the panel.')
    # Back up before changing the live layout. Backups are private and never printed.
    config = Path(os.environ.get('XDG_CONFIG_HOME', Path.home()/'.config'))/'plasma-org.kde.plasma.desktop-appletsrc'
    backup = config.with_name(config.name + '.nexis-' + str(time.time_ns()) + '.bak')
    if config.exists():
        with backup.open('xb') as handle:
            os.chmod(backup, 0o600); handle.write(config.read_bytes())
    states = service_states()
    unit_path = Path(os.environ.get('XDG_CONFIG_HOME', Path.home()/'.config'))/'systemd/user/nexis-panel.service'
    previous_unit = unit_path.read_bytes() if unit_path.exists() else None
    executable_path = prefix/'bin/nexis'
    unmanaged = None
    with tempfile.TemporaryDirectory(prefix='nexis-panel-recovery-') as recovery_directory:
        saved_executable = Path(recovery_directory)/'nexis'
        if executable_path.exists(): shutil.copy2(executable_path, saved_executable)
        try:
            unmanaged = stop_owned_runtime(prefix, build)
            subprocess.run(['cmake','--install',str(build),'--prefix',str(prefix)],check=True)
            service_directory = Path(os.environ.get('XDG_CONFIG_HOME', Path.home()/'.config'))/'systemd/user'
            service_directory.mkdir(parents=True, exist_ok=True)
            unit = (ROOT/'packaging/systemd-user/nexis-panel.service').read_text()
            # ExecStart is one quoted executable token, even when a custom prefix contains spaces.
            executable = str(prefix/'bin/nexis').replace('\\','\\\\').replace('"','\\"').replace('%','%%')
            unit = unit.replace('%h/.local/bin/nexis', '"' + executable + '"')
            (service_directory/'nexis-panel.service').write_text(unit)
            subprocess.run(['systemctl','--user','daemon-reload'], env=environment(), check=True, timeout=10)
            if states['nexis.service']['enabled'] not in ('', 'not-found'):
                subprocess.run(['systemctl','--user','disable','nexis.service'], env=environment(), check=True, timeout=10)
            subprocess.run(['systemctl','--user','enable','--now','nexis-panel.service'], env=environment(), check=True, timeout=10)
            panel_id, index = insertion['panel'], insertion['index']
            script = f'''var p=panelById({int(panel_id)});if(!p)throw Error("Top panel disappeared");var found=p.widgets().filter(function(w){{return w.type==="{PLUGIN}"}});var w=found.length?found[0]:p.addWidget("{PLUGIN}");w.index={int(index)};print(JSON.stringify({{panel:p.id,widget:w.id,index:w.index}}));'''
            placed = json.loads(evaluate(script))
            after = json.loads(evaluate(INVENTORY))
            before_ids = {w['id'] for p in inventory for w in p['widgets']}
            after_ids = {w['id'] for p in after for w in p['widgets']}
            if not before_ids.issubset(after_ids): raise RuntimeError('Panel preservation check failed; private backup retained.')
            state_directory = Path(os.environ.get('XDG_STATE_HOME', Path.home()/'.local/state'))/'nexis'
            state_directory.mkdir(parents=True, exist_ok=True, mode=0o700)
            state = {**placed,'backup':str(backup),'previous_layout':inventory,'installed_at':time.time(),'previous_services':states}
            state_path = state_directory/'panel-install.json'
            state_path.write_text(json.dumps(state, indent=2)+'\n');state_path.chmod(0o600)
            return placed
        except Exception as error:
            try:
                for name in SERVICES:
                    if name == 'nexis-panel.service' and unit_path.exists() or states[name]['enabled'] not in ('', 'not-found'):
                        subprocess.run(['systemctl','--user','stop',name], env=environment(), check=True, timeout=10)
                if states['nexis-panel.service']['enabled'] in ('', 'not-found') and unit_path.exists():
                    subprocess.run(['systemctl','--user','disable','nexis-panel.service'], env=environment(), check=True, timeout=10)
                if saved_executable.exists():
                    # Replace rather than truncate a potentially executing file.
                    restored = executable_path.with_name('nexis.recovery')
                    shutil.copy2(saved_executable, restored)
                    restored.replace(executable_path)
                if previous_unit is None: unit_path.unlink(missing_ok=True)
                else: unit_path.write_bytes(previous_unit)
                subprocess.run(['systemctl','--user','daemon-reload'], env=environment(), check=True, timeout=10)
                restore_services(states)
                if unmanaged:
                    subprocess.Popen(unmanaged, env=environment(), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
            except Exception as recovery_error:
                raise RuntimeError(f'Installation failed: {error}. Runtime recovery also failed: {recovery_error}. Panel backup: {backup}') from error
            raise

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build',type=Path,default=ROOT/'build')
    parser.add_argument('--prefix',type=Path,default=Path.home()/'.local')
    parser.add_argument('--apply',action='store_true')
    args=parser.parse_args()
    try:
        result=apply(args.build.resolve(),args.prefix.expanduser().resolve()) if args.apply else plan(json.loads(evaluate(INVENTORY)))
        print(json.dumps(result,indent=2))
    except (OSError,ValueError,RuntimeError,subprocess.SubprocessError) as error:
        print(str(error));return 1
    return 0

if __name__=='__main__': raise SystemExit(main())
