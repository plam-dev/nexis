#!/usr/bin/env python3
"""Reversible opt-in hook merge. Never prints existing settings or hook bodies."""
import argparse
import copy
import difflib
import fcntl
import json
import os
from pathlib import Path
import shlex
import stat
import sys
import tempfile
import time

EVENTS = ('SessionStart', 'UserPromptSubmit', 'PreToolUse', 'PermissionRequest',
          'PostToolUse', 'PostToolUseFailure', 'SubagentStart', 'SubagentStop',
          'Stop', 'SessionEnd', 'PreCompact', 'Notification')

def owned(entry, provider):
    return (isinstance(entry, dict) and entry.get('type') == 'command'
            and isinstance(entry.get('command'), str)
            and entry['command'].endswith(' # nexis:v1:' + provider))

def validate(document):
    if not isinstance(document, dict) or not isinstance(document.get('hooks', {}), dict):
        raise ValueError('Settings and hooks must be JSON objects; file preserved.')
    for groups in document.get('hooks', {}).values():
        if not isinstance(groups, list):
            raise ValueError('Hook event groups must be arrays; file preserved.')
        for group in groups:
            if not isinstance(group, dict) or not isinstance(group.get('hooks'), list):
                raise ValueError('Invalid hook group; file preserved.')
            if any(not isinstance(entry, dict) for entry in group['hooks']):
                raise ValueError('Invalid hook handler; file preserved.')

def read(path):
    if path.is_symlink():
        raise ValueError('Symlink settings are not edited; use the real settings path.')
    if not path.exists():
        return b'', {}
    info = path.stat()
    if not stat.S_ISREG(info.st_mode) or info.st_uid != os.getuid() or info.st_size > 1024 * 1024:
        raise ValueError('Settings must be a user-owned regular file below 1 MiB.')
    raw = path.read_bytes()
    try:
        doc = json.loads(raw)
    except (ValueError, UnicodeError):
        raise ValueError('Settings are not valid JSON; file preserved.') from None
    validate(doc)
    return raw, doc

def owned_lines(doc, provider):
    return [event + ': ' + entry['command'] + '\n'
            for event, groups in doc.get('hooks', {}).items()
            for group in groups for entry in group['hooks'] if owned(entry, provider)]

def status(path, provider):
    _, doc = read(path)
    events = [event for event, groups in doc.get('hooks', {}).items()
              if any(owned(entry, provider) for group in groups for entry in group['hooks'])]
    return {'provider': provider, 'configured': len(events) == len(EVENTS),
            'events': events, 'delivery': 'Unverified until an event is received',
            'product': 'Qoder desktop app' if provider == 'qoder' else 'Claude Code'}

def update(path, provider, bridge, action, dry_run=False):
    path = Path(path).expanduser().absolute()
    if provider not in ('claude', 'qoder') or action not in ('install', 'uninstall'):
        raise ValueError('Unsupported provider or action.')
    # Dry runs perform no filesystem writes, including locks or directory creation.
    if dry_run:
        raw, original = read(path)
        return prepare(original, provider, bridge, action, preview=True)
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    lockpath = str(path) + '.nexis.lock'
    lockfd = os.open(lockpath, os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
    try:
        if os.fstat(lockfd).st_uid != os.getuid():
            raise ValueError('Settings lock belongs to another user.')
        fcntl.flock(lockfd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        raw, original = read(path)
        document = prepare(original, provider, bridge, action)
        if document == original:
            return False
        encoded = (json.dumps(document, indent=2, ensure_ascii=False) + '\n').encode()
        mode = stat.S_IMODE(path.stat().st_mode) if path.exists() else 0o600
        temporary = None
        try:
            fd, temporary = tempfile.mkstemp(prefix='.nexis-', dir=path.parent)
            with os.fdopen(fd, 'wb') as handle:
                os.fchmod(handle.fileno(), mode)
                handle.write(encoded); handle.flush(); os.fsync(handle.fileno())
            if read(path)[0] != raw:
                raise ValueError('Settings changed during merge; retry. File preserved.')
            if raw:
                backup = str(path) + '.nexis-' + str(time.time_ns()) + '.bak'
                backupfd = os.open(backup, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
                with os.fdopen(backupfd, 'wb') as handle:
                    handle.write(raw); handle.flush(); os.fsync(handle.fileno())
            os.replace(temporary, path)
            temporary = None
            dirfd = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY)
            try: os.fsync(dirfd)
            finally: os.close(dirfd)
        finally:
            if temporary: os.unlink(temporary)
        return True
    finally:
        os.close(lockfd)

def prepare(original, provider, bridge, action, preview=False):
    validate(original)
    doc = copy.deepcopy(original)
    groups_by_event = doc.setdefault('hooks', {})
    for event, groups in list(groups_by_event.items()):
        kept = []
        for group in groups:
            entries = [entry for entry in group['hooks'] if not owned(entry, provider)]
            if entries or not any(owned(entry, provider) for entry in group['hooks']):
                kept.append({**group, 'hooks': entries})
        if kept: groups_by_event[event] = kept
        else: groups_by_event.pop(event, None)
    if action == 'install':
        command = shlex.quote(str(bridge)) + ' --provider ' + provider + ' # nexis:v1:' + provider
        for event in EVENTS:
            groups_by_event.setdefault(event, []).append({'hooks': [{'type': 'command', 'command': command, 'timeout': 1, 'async': True}]})
    if not groups_by_event and 'hooks' not in original:
        doc.pop('hooks')
    if preview:
        # A filtered diff never discloses unrelated settings, secrets or user hook commands.
        return ''.join(difflib.unified_diff(owned_lines(original, provider), owned_lines(doc, provider), fromfile='NEXIS current hooks', tofile='NEXIS planned hooks'))
    return doc

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('install', 'uninstall', 'status'))
    parser.add_argument('--provider', required=True, choices=('claude', 'qoder'))
    parser.add_argument('--settings', type=Path)
    parser.add_argument('--bridge', required=True)
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args()
    path = args.settings or Path.home() / ('.claude' if args.provider == 'claude' else '.qoder') / 'settings.json'
    try:
        if args.action == 'status': print(json.dumps(status(path, args.provider), indent=2))
        else:
            result = update(path, args.provider, args.bridge, args.action, args.dry_run)
            print(result if args.dry_run else ('Updated. Restart the app or start a new task to load hooks.' if result else 'Already up to date.'))
    except (ValueError, OSError) as error:
        print(str(error), file=sys.stderr); return 1
    return 0

if __name__ == '__main__': sys.exit(main())
