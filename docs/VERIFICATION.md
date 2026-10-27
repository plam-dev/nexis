# Verification — 2026-10-28

## Current notch acceptance

- Full CMake build and all 11 CTest suites pass. GUI regressions reproduce premature artist truncation before the fix, then verify the longest line fits within an explicitly available width and only truncates when that width is exhausted. Other checks cover centered groups, artwork fallback, playback forwarding, hidden/reduced-motion pulse behavior, light appearance, job progress, scroll bounds and notch alpha/input geometry.
- Width uses the rendered title/subtitle fonts, the longer measured line, and the actual fixed-width artwork/cat/pulse/padding. The old 440 px minimum and independent text-width cap are removed. The host supplies a maximum of 60% of the current screen width.
- Every expansion opens Music even when an agent permission request exists. The navigation contains only Music, Agents and System; activity-log UI is absent and history defaults off. Live jobs remain in Agents.
- Native Wayland trace from the installed notch confirms overlay layer 3, exclusive zone -1 and margins `(0,0,0,0)`. The compositor configured the attached notch at `(440,72)` in the initial geometry check; subsequent width is content-driven. Native screenshot alpha confirms transparent shoulders/corners and an opaque notch body. The surface/buffer size assertion passed.
- Floating `nexis.service` replaces the embedded headless panel mode. Only the owned NEXIS widget was removed; the remaining panel widgets were preserved. Live snapshots report media artwork and notification bridge Connected.
- A labeled local desktop notification reached the island, retained music context, expired after five seconds and returned presentation to music. KDE remains notification owner and may also show its own popup.
- A fresh review checked geometry and notification trust/recovery. Important findings were corrected; QtQuick.Shapes is included in the documented Ubuntu runtime requirements. Private-bus tests cover delayed daemon availability, DND recovery, bounded text, forged callbacks, restart, expiry and demo isolation.

Development host: Fedora 43, KDE Plasma/KWin 6.7.5 Wayland, Qt 6.10.3. User target: **new Qoder desktop app 0.4.4**. See ENVIRONMENT.md.

## Milestone evidence

- M0: workspace was empty except the specification; versions, runtime, theme and layer-shell support recorded. CMake config/build uses local development RPM extraction without privileged installation.
- M1: native QML capsule renders in software/offscreen and real KWin Wayland. Actual screenshots were inspected. MPRIS integration tests discover two isolated players, select/control only one, deny unsupported controls, seek with track ID, update metadata/status and recover on service removal/restart.
- M2: core state and privacy tests cover Claude Code and Qoder's twelve common hooks; separate namespaces/workspaces, Stop versus SessionEnd, permission persistence, subagents, stale/order/replay checks. Transport uses real private Unix sockets, checks 0600 permissions, collector absence, bounded/stalled input, redaction, malformed events and duplicate instance protection. Python tests verify preservation, backup, idempotence, dry-run and uninstall behavior.
- M3: system adapters compile and report capability state; runtime-only KWin script reports `Connected` through its read-only D-Bus health property on the real compositor. Jobs reject out-of-range progress, unexpected fields, replay and unsafe labels. GUI tests click to expand, press Escape, switch themes and inspect all tabs/settings with zero QML warnings. Staged CMake installation succeeds.

## Commands

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="$PWD/.deps/root/usr" -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/nexis doctor
cmake --install build --prefix "$PWD/build/stage"
```

Seven CTest suites: core, hooks, gui-smoke, media-bus, transport, desktop, gui-interaction. Final full run after review fixes: **7/7 suites pass, 0 failures, 4.32 seconds**. Source formatting and staged installation also pass.

The independent review issues were reproduced as failing tests before fixes: hostile media markup caused a real loopback HTTP connection; interleaved tool A/B events incorrectly cleared permission; 100 subagents exceeded a per-session bound; an orphan fake KWin script prevented reload; hover changed focus; and expanded idle context switched on ordinary work. All regression tests pass. Additional theme RGB-list and history-preservation regressions pass.

## Acceptance matrix

| Criteria | Evidence / remaining check |
|---|---|
| AC-01 | Native Wayland launch and layer-shell startup pass; manual panel interaction not exhaustively audited. |
| AC-02–04 | Two-provider isolated session D-Bus tests pass for discovery, metadata, control gating, selection, seek, restart. Real music app check remains. |
| AC-05–08 | Both provider fixtures/state tests and real socket tests pass. Actual authenticated app task checks remain. AC-06 now targets the new Qoder app per user correction. |
| AC-09–10 | Permission persistence, no approval action, conservative missing fields, privacy validation tests pass. |
| AC-11–13 | Fail-open bridge, stalled clients, provider reconnect, duplicate instances and safe settings merge tests pass. |
| AC-14–16 | Allowlist/redaction, priority and reduced-motion GUI checks pass; pin policy is implemented. |
| AC-17 | Hotplug fallback and per-display preferences implemented; physical disconnect/reconnect not run. |
| AC-18 | Real KWin observer handshake and per-screen state tests pass; fullscreen/focus acceptance during a real app workload remains manual. |
| AC-19 | CPU/RSS and bridge/collector timing reports are generated by benchmark scripts; animation FPS and rendered-update latency are not claimed. |
| AC-20 | Keyboard expansion/collapse and accessible labels implemented/tested where possible; a screen-reader audit remains manual. |

No hooks, existing vendor settings, autostart, system packages or compositor persistent preferences were changed. Synthetic agent data is restricted to explicit demo mode or isolated tests. Notification/download bridges and advanced sensors remain unavailable rather than simulated.

## Measured performance

[Native idle baseline](benchmark-idle.json): 600 seconds, 0.06% of one CPU core, 177.25 MiB mean RSS, 198.61 MiB peak RSS. CPU meets the stated idle goal; **memory exceeds the aspirational 120 MB goal**. It was recorded on the integrated build before final review corrections and the layer-shell initialization fix, with no synthetic activity. GPU/KWin helper allocations are outside the process RSS metric.

[Bridge/collector measurement](benchmark-bridge.json) records 50 runs per case, including process startup. Rendered-frame latency and animation FPS remain unmeasured; event-to-snapshot timing is not represented as event-to-frame latency.

## Native surface follow-up

A real Wayland screenshot caught an early-surface initialization error that offscreen tests could not detect. The QML window now waits for native placement/layer-shell setup before becoming visible, and explicit desired size follows its morph. A new `--assert-surface` check reproduced a 400×570 logical versus 450×66 physical buffer mismatch before the fix. The software-rendered native Wayland test now produces the correct 600×855 buffer at 1.5× scaling and exits zero. Protocol tracing confirms `zwlr_layer_shell_v1.get_layer_surface`, not xdg_toplevel.

The user's screen is locked (`org.freedesktop.ScreenSaver.GetActive = true`), which suppresses GPU frame callbacks. Hardware-rendered expanded animation and interactive desktop focus behavior cannot be accepted under this condition; no attempt was made to unlock the session. The saved preview uses the real native Wayland app with `QT_QUICK_BACKEND=software` and explicit demo data.

Final staged native software run: `QT_QUICK_BACKEND=software build/stage/bin/nexis --demo --smoke-test --assert-surface` passes with exit zero; all four tabs/settings and expansion/collapse are exercised. Expanded native software screenshot plus surface assertion also exits zero. A separate [10-second software probe](benchmark-software-probe.json) measured 83.14 MiB RSS and 0.0% CPU, which is promising but is not a replacement for the ten-minute acceptance window.
