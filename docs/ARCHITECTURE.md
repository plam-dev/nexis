# Architecture

`nexis` combines the Qt Quick shell and event collector in one ordinary-user process. This keeps the bounded in-memory MVP small and avoids a redundant daemon/UI synchronization layer. `--headless` runs the same collector without the GUI. No database is created.

```text
Claude Code / new Qoder app
  -> nexis-hook-bridge (bounded stdin, privacy allowlist)
  -> XDG_RUNTIME_DIR/nexis/events.sock
  -> Collector (SO_PEERCRED, bounded connections/buffer/rate)
  -> Registry (validation, namespaces, replay/order checks, TTL)
  -> Backend -> QML capsule / agent cards / history
MPRIS session D-Bus -> Media -> Backend / QML
UPower + NetworkManager system D-Bus -> System -> QML
WirePlumber wpctl -> asynchronous output status/actions
KWin runtime observer -> fullscreen output names -> visibility
Local producer -> same socket -> Jobs (independent strict schema)
```

The runtime directory must belong to the current UID and have mode 0700, must not be a symlink, and is created beneath a user-owned 0700 XDG_RUNTIME_DIR. The socket is 0600; both collector and bridge verify peer UID. A QLockFile prevents duplicate instances before socket cleanup. Up to 16 clients, 16 KiB per normalized event, 100 messages/second, and a 500 ms incomplete-message deadline bound the collector. One JSON object plus newline is accepted per connection.

Agent IDs and workspace strings are SHA-256 hashed before crossing the socket. Claude session keys combine provider and hashed session ID so working-directory changes cannot strand an old permission card; Qoder keys also include the hashed workspace. Missing session IDs use a workspace-scoped provisional hash with `limited` confidence. Ambiguous no-ID sessions can coalesce within one workspace; this is explicitly cautious rather than a claim of independent identity.

Registry limits: 256 sessions, 4096 recent event IDs, 300 history records. Jobs: 32 jobs, 1024 replay IDs, 100 history records. Retention is memory-only; disabling history clears it. SessionEnd is terminal until a new SessionStart or UserPromptSubmit. Stop marks only turn completion. A 15-minute silence marks unfinished sessions/jobs stale. Confirmed permission progression clears attention; Notification/Subagent events alone do not. Each session caps tools, pending permissions and identified subagents at 64 each; overflow marks confidence limited. Stale reconciliation drops in-flight sets without claiming completion. Matching tool completions resolve pending permissions by tool ID, or by a SHA-256 fingerprint of tool name and canonical tool-input JSON when the permission event omits its ID. PreToolUse is not confirmation of a permission decision; unrelated completions cannot clear it.

Source timestamps and IDs reject stale or duplicate delivery; receive indexes order display. Vendor hooks without source timestamps use receive time and cannot establish true original order.

`org.nexis.Desktop`, object `/org/nexis/Desktop`, exposes `Snapshot() -> JSON string`, state-change signals and the tightly scoped PanelAction allowlist for the native panel (media, output volume, history, reduced motion and popup state); it exports no permission or process-launch action. See PANEL.md for the panel architecture. The compositor object exposes a narrowly scoped `Report(JSON output names)` method accepted only from the current KWin owner, plus read-only health/fullscreen properties. No TCP endpoint, global shortcut, or remote telemetry exists.

Media uses async D-Bus discovery, GetAll/Get/PropertiesChanged, NameOwnerChanged and Seeked. Name owners are resolved explicitly before subscribing. Capabilities gate controls; service and track IDs scope seek. Position is refreshed only while expanded. Only local raster artwork below 4 MiB and 4096 pixels per side in permitted local locations is loaded; remote URLs are ignored.

Equal-priority running agent selection remains stable while that session is active, avoiding event-driven rotation. Permission alerts and critical battery state preempt as configured; recent completion lasts five seconds. Expanded state retains the selected agent/media context. Categories can be pinned in Settings. Activity cards remain accessible in the tabbed center.

The KWin helper is loaded through KWin's documented scripting D-Bus interface for the application's lifetime, with no persistent KWin configuration changes. On clean shutdown it unloads; the next startup reclaims its owned script after a crash or signal termination. It monitors fullscreen/minimize/desktop/activity/output changes and reports only fullscreen output names. If KWin restarts it reloads; if unavailable, the diagnostic says so.

Build boundaries: `nexis-core` (agent state, MPRIS), `nexis-desktop` (settings, system, backend, jobs, collector and compositor), two executables, QtTest and Python integration suites. LayerShellQt 6.7+ is optional at compile time; fallback windows cannot promise Wayland positioning.
