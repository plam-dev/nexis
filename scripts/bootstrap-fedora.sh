#!/usr/bin/env bash
# Extract development packages locally; never modifies the RPM database or system files.
set -euo pipefail
cd "$(dirname "$0")/.."
qt_version=$(rpm -q --qf '%{VERSION}-%{RELEASE}' qt6-qtbase)
layer_version=$(rpm -q --qf '%{VERSION}-%{RELEASE}' layer-shell-qt)
mkdir -p .deps/rpms .deps/root
dnf --repo=fedora --repo=updates download --arch=x86_64 --destdir .deps/rpms \
    "qt6-qtbase-devel-$qt_version" "qt6-qtdeclarative-devel-$qt_version" "layer-shell-qt-devel-$layer_version"
for package in .deps/rpms/*x86_64.rpm; do
    rpm2cpio "$package" | (cd .deps/root && cpio -idm --quiet)
done
python3 - <<'PY'
from pathlib import Path
root = Path('.deps/root')
for p in Path('/usr/lib64').glob('lib*.so*'):
    if p.name.startswith(('libQt6', 'libLayerShell')):
        dest = root / p.relative_to('/')
        if not dest.exists() and not dest.is_symlink(): dest.symlink_to(p)
for directory in ('/usr/lib64/qt6/bin', '/usr/lib64/qt6/libexec', '/usr/share/qt6'):
    for p in Path(directory).rglob('*'):
        dest = root / p.relative_to('/')
        if p.is_file() and not dest.exists() and not dest.is_symlink():
            dest.parent.mkdir(parents=True, exist_ok=True); dest.symlink_to(p)
for name in ('qml', 'plugins'):
    dest = root / 'usr/lib64/qt6' / name
    if not dest.exists(): dest.symlink_to(Path('/usr/lib64/qt6') / name)
PY
cmake -S . -B build -DCMAKE_PREFIX_PATH="$PWD/.deps/root/usr" -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j4
