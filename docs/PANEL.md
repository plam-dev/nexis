# NEXIS · Optional Fold panel widget

The current display mode is a standalone Fold island floating over the top panel, managed by `nexis.service`. The embedded widget and its headless `nexis-panel.service` are disabled on this machine. They remain an optional alternative; running the panel installer below switches back to that mode.

Fold keeps the capsule content centered with symmetric padding and derives its width from the current title, subtitle, artwork, cat and pulse. Long titles elide at the width limit. The expanded player follows its content height and scrolls within the screen when notices or activity would exceed it. Music uses a 30 px compact thumbnail (bounded by panel height), a 48 px expanded thumbnail, icon playback controls and a quiet playback-state pulse. Pause, reduced motion, hidden/minimized windows and hidden items stop the pulse; the cat also sleeps when its window is hidden.

Incoming KDE notifications briefly replace the capsule text for five seconds, while preserving the cat's working/listening state. Permission and critical-battery alerts stay higher priority. Click the capsule to see notification details alongside music, or open Activity. Up to three notices are shown at a time; older notices remain accessible in Plasma's own history. Dismiss removes the island copy; Open invokes the advertised default action. Plasma retains ownership and may also show its own banner. DND prevents island previews. No notification content is persisted.

| State | Text | Pet |
|---|---|---|
| Idle | Cat-only by default; optional clock + date | Slow breathing, blinking, occasional ear twitch and tail flick |
| Desktop adjustment | Volume/brightness percentage and a small bar, or a short system status for 3 seconds | Preserves current work/music pose |
| Agent processing | Working | Typing at laptop |
| Music playing | Current track title (Music if unavailable) | Headphones, gentle head bob |
| Music paused | Track title and artwork, static pulse | Resting unless an agent is working |
| Desktop notification | Summary and sending app for 5 seconds | Preserves current work/music pose |
| Music and processing | Current track title (Music if unavailable) | Headphones and typing together |
| Agent response completes | Claude Code / Qoder · Done for 4 seconds | Continues any remaining work/music pose |
| Permission / critical | Clear attention message | Keeps listening; only types if another session is processing |

The cat always exists. Source/project names stay in the popup during ordinary activity. A completed response never claims the entire session or project has ended. Completion expiry uses its own timestamp, so later notifications do not restart the banner.

**Show clock and date when idle** is available in System and Preferences. It defaults off, persists as `idleClock`, and updates the capsule immediately. Enabling it shows the clock/date beside the cat during idle; music, notifications and system feedback still take precedence. The clock timer stops when disabled or when its surface is hidden/minimized.

## Previous embedded layout

Before switching to the floating island:
- Widget: `org.nexis.panel`, instance 230 in top panel 200 at index 3 (removed for floating mode).
- It sits between preexisting spacers 203 and 225; launcher, window title, system tray, panel colorizer and bottom dock remain.
- Runtime: `~/.local/bin/nexis --headless`, managed by `nexis-panel.service`.
- Panel package: `~/.local/share/plasma/plasmoids/org.nexis.panel`.
- Private backup and latest pre-install widget inventory: `~/.local/state/nexis/panel-install.json`.

Live scripting measured the compact widget center at x=855 on a 1707 px display, within rounding of the panel center. The pet adapts to the panel content height; both 26 px and 36 px content geometry were observed during validation.

Click the cat or track title to open its popup. Tabs: Music, Agents, Activity, System. Middle click play/pauses the selected player; explicit controls are capability-gated. Reduced motion is available in System. Qoder targets the new desktop application; live hook events from the new Qoder app and concurrent MPRIS playback were received during validation. Hook setup remains opt-in for other integrations.

## Rebuild / install

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="$PWD/.deps/root/usr" -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j4
ctest --test-dir build --output-on-failure
python3 scripts/install-panel.py           # read-only placement plan
python3 scripts/install-panel.py --apply   # back up, install, start collector, add/reuse widget
```

`--apply` migrates an owned standalone NEXIS runtime to the lightweight headless service. It disables the old standalone autostart to avoid competing collectors at login. A failed install restores the previous runtime binary, service unit and active/enabled service state. It never stops Claude, Qoder, music players or Plasma. Installation reuses an existing widget and preserves every previous widget ID. KDE settings are changed through the documented live Plasma scripting API, not by overwriting the running shell's config file.

To stop the collector: `systemctl --user disable --now nexis-panel.service`. The pet stays in the panel; music/agent updates show disconnected. To remove the widget, use KDE's panel edit mode and remove **NEXIS · Tuxedo**. The backup is retained; restore only while Plasma is stopped, or use KDE's panel editor to preserve newer unrelated layout changes.

## Implementation and checks

`ui/TuxedoCat.qml` is original local pixel artwork drawn on a 40×28 grid. It has independent work/listen flags, bounded 8-frame motion, a 3.2-second idle breathing cycle with a low-frequency blink and occasional ear/tail movement, nearest-neighbor scaling and static reduced-motion poses. Hidden/minimized windows pause the animation. No downloaded asset, image-generation service, microphone or beat analysis is used.

Desktop feedback listens to the current `org.kde.plasmashell` owner's `org.kde.osdService` progress/text signals. Volume, screen/keyboard brightness and microphone levels show a percentage and bar; mute, Wi-Fi, Bluetooth and other native status events show short plain text. Each event restarts a three-second timer. Permission and critical-battery alerts retain priority; feedback otherwise temporarily takes the place of notifications/music, then restores the latest activity. Successful volume/mute changes made inside the island also use this banner. There is no startup banner or extra polling. Native Plasma OSD remains enabled and may appear too; disabling it also stops Plasma from emitting these signals.

`CompactActivity.qml` is shared with the standalone preview. The widget inherits Kirigami/Plasma colors and the panel background, rather than drawing a second floating window. The D-Bus client is packaged as a private Qt plugin, coalesces signals, limits snapshots to 1 MiB and recovers when the collector restarts. There is no once-per-second process launch or shell command polling.

The existing snapshot now contains presentation, media, player list, system, options and integration health. `PanelAction` exposes a small allowlist for explicit media/volume/history/reduced-motion/popup gestures, never permission decisions, shell commands, arbitrary settings or remote requests. Track text remains plain text. Media exports only bounded display fields and capability flags, with at most 32 discovered players; arbitrary MPRIS properties and metadata are excluded. Numeric controls reject strings and booleans.

Automated checks cover the state matrix and exact 4-second boundary, working/listening independence, another session continuing, private-bus initial/connect/disconnect/reconnect, signal coalescing, denied unknown actions, safe panel insertion, idempotence and preservation. GUI checks exercise all pet poses, text-free idle, breathing/ear/tail changes, feedback progress and static reduced motion. Desktop feedback tests cover expiry refresh, forged/stale senders, Plasma restart, bounded values, disabled demo mode and successful/failed local volume writes. Installer checks simulate install, service-start and placement failures and verify runtime recovery. The native Plasma host loads without QML errors; direct interaction with the live popup was not automated. Plasma may log a non-fatal identifier warning for the private relative QML module; it loads successfully.

Reference: [KDE Plasma scripting API](https://develop.kde.org/docs/plasma/scripting/api/), [Plasma widget API](https://develop.kde.org/docs/plasma/widget/).

## Follow-up: animation and track title

Persisted INI settings reload as strings in QSettings. Options are now converted back to their declared bool/int/string types before entering QML or JSON, so the string `false` can no longer enable reduced motion by accident. The music presentation now displays the current track title (bounded to 200 characters, visually elided to the available panel width), retaining Music as the empty-title fallback. Concurrent work still keeps the typing pose; permission/completion precedence stays the same.

Regression checks reproduce persisted string settings through the real backend JSON and shared QML binding. The GUI check compares rendered frames, rather than only checking a timer counter, while the previous reduced-motion test still ensures the pet can intentionally stay static.

Post-install verification: nexis-panel.service is active; the live snapshot exports reducedMotion as JSON boolean false and the current music title matches MPRIS. Four native Spectacle captures compared only the cat region in memory: changed pixels were observed in two of three comparisons, confirming animation in the actual top panel. Temporary full-screen captures were deleted. Fresh CTest: 10/10 suites pass, 5.63 seconds.
