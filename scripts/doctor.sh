#!/usr/bin/env bash
set -u
printf 'NEXIS diagnostics\n'
if test -r /etc/os-release; then . /etc/os-release; printf 'OS: %s\n' "$PRETTY_NAME"; fi
printf 'Session: %s | desktop: %s\n' "${XDG_SESSION_TYPE:-not inherited by this shell}" "${XDG_CURRENT_DESKTOP:-not inherited by this shell}"
printf 'Runtime: %s\n' "${XDG_RUNTIME_DIR:-unavailable}"
for name in cmake g++ qtpaths6 wpctl wayland-info; do
    if command -v "$name" >/dev/null 2>&1; then printf '%s: available\n' "$name"; else printf '%s: unavailable\n' "$name"; fi
done
if command -v qtpaths6 >/dev/null; then printf 'Qt: '; qtpaths6 --qt-version; fi
if command -v rpm >/dev/null; then rpm -q --qf '%{NAME}: %{VERSION}\n' plasma-workspace kwin qoder 2>/dev/null; fi
if command -v claude >/dev/null; then claude --version; fi
printf 'Qoder target: new desktop app (/opt/Qoder/qoder), NOT IDE or standalone CLI\n'
if test -x /opt/Qoder/qoder; then printf 'Qoder desktop: installed\n'; else printf 'Qoder desktop: not found at Fedora default path\n'; fi
if test -S "${XDG_RUNTIME_DIR:-/nonexistent}/bus"; then printf 'Session D-Bus: available\n'; else printf 'Session D-Bus: unavailable\n'; fi
if test -S "${XDG_RUNTIME_DIR:-/nonexistent}/nexis/events.sock"; then printf 'Collector socket: present (use --snapshot to check response)\n'; else printf 'Collector socket: not running\n'; fi
if command -v wayland-info >/dev/null && test -S "${XDG_RUNTIME_DIR:-/nonexistent}/${WAYLAND_DISPLAY:-wayland-0}"; then
    if WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-0}" wayland-info 2>/dev/null | grep -q 'zwlr_layer_shell_v1'; then printf 'Wayland layer-shell: available\n'; else printf 'Wayland layer-shell: unavailable\n'; fi
fi
printf 'Notifications/download bridge: unavailable; Plasma retains ownership\n'
printf 'Live hooks: unverified until observed; inspect System in NEXIS\n'
printf 'History: memory-only (bounded); clears on restart\n'
printf 'No settings were changed by this command.\n'
