# NEXIS — Technical Product Specification

**Version:** 1.0 · **Target:** KDE Plasma 6.7.5, Linux Wayland · **Handoff:** Codex · **Language:** English (engineering)

## 0. Product mandate
NEXIS is a **general-purpose Dynamic Island for daily desktop productivity**, with **AI Agent Sessions** as a flagship integration—not an agent-only monitor. The product is an unobtrusive, always-available floating activity surface. Users can listen to music, control playback, inspect Claude Code and Qoder IDE work, notice permission requests, see notifications and download progress (where integration is dependable), and inspect essential system indicators.

### Non-goals (v1)
- Replace Plasma notification daemon, media player, taskbar, window manager, or power management.
- Approve or reject agent tool permissions from island.
- Read private prompts or source code for activity monitoring.
- Promise automatic tracking of arbitrary browser downloads; use supported interfaces only.
- Assume Qoder CLI and IDE have identical event support.

### Guiding principles
1. **Quiet by default:** compact pill when idle, contextual expansion only when useful.
2. **Source-grounded:** distinguish confirmed event, inferred activity, and stale state.
3. **No disruption:** hook adapters must never intentionally block Claude/Qoder work.
4. **Native and efficient:** Qt Quick/QML, C++/Qt, D-Bus, minimal wakeups.
5. **Privacy-first:** on-device only, redact raw inputs/outputs, no remote telemetry.
6. **Graceful degradation:** missing capability must not break island.

## 1. Scope and priority

| Module | Phase | Requirements |
|---|---|---|
| Floating compact/expanded shell | M0 | Qt6 QML; draggable offset; Wayland-friendly surface; keyboard/mouse behavior; settings |
| Media player | M1 | MPRIS player discovery, track/artist/artwork, play/pause/next/previous, seek only if supported, multiple player selection |
| Claude Code | M1 | Hook ingestion; session/turn/tool/permission/subagent tracking; notification on turn end |
| Qoder IDE | M1 | Hooks for IDE's supported events; separate normalization path; active sessions |
| Multi-session center | M2 | Session cards, source/project/status, last event, timestamp, stacked priorities |
| Volume / audio | M2 | WirePlumber/PipeWire preferred APIs; display system volume, mute; user-authorized change |
| Connectivity / power | M2 | NetworkManager + UPower; connected state, battery when present |
| Notifications | M3 | Opt-in bridge; preserve Plasma notification ownership, avoid double notifications |
| Downloads / jobs | M3 | KDE job tracking with validated integration; unsupported browser downloads excluded unless extension/adapter available |
| Developer API | M3 | Safe local producer API with allow-listed event schema, optional project build integrations |
| Performance details | M3 | Optional CPU/RAM/temperature; low-frequency sampling; disabled if sensors unavailable |

### Keyboard and pointer UX
- Compact (default): icon + one-line primary activity + minimal count indicator. Empty state shows clock or NEXIS monogram (configurable).
- Hover: subtle emphasis and tooltip; **do not expand on every incidental hover**. Optional dwell expansion after 350ms configurable.
- Click: expand detailed card; click outside / Escape to collapse.
- Middle-click, primary shortcut, and focus behavior configurable, never globally hijack keys by default.
- Expanded view: segmented tabs `Activities`, `Agents`, `Media`, `System` but show contextual primary card first.
- Configurable reduced-motion, avoid transient focus steals and input interception.
- Fullscreen: default auto-hide; user option overlay (must validate KWin and layer-shell behavior). No exclusive screen zone unless explicitly chosen.
- Multi-monitor: default island on active/selected monitor; per-screen saved position; hotplug recovery.

## 2. Design tokens
- Theme: light desktop with premium charcoal capsule; optional full light/dark/auto.
- Capsule: idle 144–180x36–42dp; active ~250–320x42–52dp; expanded ~350–420dp width, adaptive height up to 65% screen height. Values are baseline tunables, not pixel-perfect contract.
- Radius: pill compact, 20–24dp expanded. Accent: ice-blue for generic, warm orange for Claude, cool blue for Qoder, amber attention, red critical, green confirmation. **Do not use color alone for state**.
- Typography: system font, 12–14sp compact, 13–16sp expanded. Contrast WCAG AA where feasible.
- Motion: idle→activity 300–350ms, expand 350–450ms, collapse 200–280ms; spring-inspired easing, content fade+size morph, no unbounded high-frequency effects. Honor reduced motion.

## 3. Architecture

```text
Claude Code hooks ─┐
                   ├─> per-source hook adapters ─> Unix domain socket ─> NEXIS daemon
Qoder IDE hooks ───┘                                           │
MPRIS / D-Bus ─────────────────────────────────────────────────┤
UPower / NM / audio APIs ──────────────────────────────────────┤
Optional jobs/notifications bridges ──────────────────────────┤
                                                               ▼
           Validator → Normalizer → Session Registry / Activity Engine
                                    │           │
                               SQLite*      D-Bus state signals
                                    │           │
                                    └───────────┴──> Qt Quick/QML Island
*Persist only minimal event metadata; retention/off switch is configurable.
```

Repository layout:

```text
nexis/
  CMakeLists.txt
  README.md
  docs/ARCHITECTURE.md
  docs/HOOKS.md
  docs/PRIVACY.md
  apps/island/                 # Qt6 QML + app entrypoint
  services/core/              # C++ Qt event manager, DBus service, persistence
  modules/media/              # MPRIS, player discovery
  modules/agents/             # session normalization and registry
  modules/system/             # power, connectivity, audio
  integrations/claude/        # hooks install/merge & bridge entrypoint
  integrations/qoder-ide/     # independent hooks config/install adapter
  tools/nexis-hook-bridge/    # light stdio-to-UDS binary
  tests/unit/
  tests/integration/
  tests/gui/
  packaging/systemd-user/
  packaging/desktop/
  scripts/doctor.sh
```

### Runtime contract
- Unix socket under `$XDG_RUNTIME_DIR/nexis/events.sock`, directory mode 0700, socket mode 0600, same-UID clients only; no TCP listener by default.
- Qt session bus D-Bus service `org.nexis.Desktop` (name tentative), with read-only session snapshots/signals and tightly scoped action methods.
- Queue bounded to protect daemon; nonblocking producer short timeout (target <100ms). Adapter must return normal success to host when collector unavailable, without extraneous stdout; errors logged locally to opt-in diagnostic log or stderr only when safe.
- DB: SQLite WAL if needed, default event metadata retention 7 days, max number of records, user can disable/clear history. Memory-only MVP is allowed.
- Process launch using XDG autostart or `systemd --user`; prevent duplicate instances.

## 4. Agent hook integrations
### Verified supported lifecycle events
Both Claude Code and **Qoder IDE** document these: `SessionStart`, `UserPromptSubmit`, `PreToolUse`, `PermissionRequest`, `PostToolUse`, `PostToolUseFailure`, `SubagentStart`, `SubagentStop`, `Stop`, `SessionEnd`, `PreCompact`, `Notification`. Claude Code supports additional events; **only consume events confirmed in the target installed version**. Qoder IDE's 12 events and `command` / `http` handler support are distinct from Qoder CLI or other Qoder products.

Sources:
- https://code.claude.com/docs/en/hooks
- https://docs.qoder.com/extensions/hooks

### Installation
- Claude Code: merge hook groups into user settings without overwriting user hooks; validate against installed version's official config schema. Preserve existing matching hooks.
- Qoder IDE: merge hook groups into `~/.qoder/settings.json`; Qoder also supports project settings paths described in official docs. Prefer user-level opt-in setup; ensure settings path and IDE reload behavior verified.
- Hook command adapter reads JSON from stdin, validates schema and size, strips prompt / tool input / responses, sends normalized event over local UDS; `exit 0` even if daemon is unavailable. Do not write any decision JSON or human text to stdout. Never register as permission-blocking hook.
- Support `nexis doctor`, `nexis hooks install --provider claude|qoder`, `nexis hooks status`, `nexis hooks uninstall`; dry-run diff and backup before modifying settings, atomic write/rollback, do not erase unrelated hooks.
- If Qoder's currently installed build doesn't fire a documented event, degrade to a known state and report adapter capability status; never fake activity.

### Normalized event JSON (internal design; not vendor-native)

```json
{
  "schema_version": 1,
  "event_id": "uuid",
  "provider": "claude_code",
  "source_event": "PreToolUse",
  "session_id": "opaque-provider-session-id",
  "subagent_id": null,
  "project_id": "hashed-or-local-project-key",
  "project_label": "portfolio-web",
  "activity_kind": "tool_started",
  "tool_kind": "edit",
  "target_label": "src/Hero.tsx",
  "occurred_at": "2026-10-27T16:00:00Z",
  "received_at": "2026-10-27T16:00:00Z",
  "confidence": "observed",
  "status": "working"
}
```

`session_id` namespace is `(provider, workspace, provider_session_id)`; never merge two provider session IDs blindly. If session ID absent: generate provisional key and surface limited-confidence badge. Sanitize target labels: prefer basename / path relative to trusted project, remove secrets and path traversal; never send raw shell commands, prompt strings, env vars, file contents, full transcripts, or tool outputs. Events capped at 16KB normalized; reject oversize raw event without blocking agent.

### State machine
`unknown → started → running → tool_running / waiting_permission → running → turn_complete → running (next prompt) → session_ended`
Additional states: `tool_failed` (event, not necessarily terminal), `stale`, `unavailable`; `subagent_running` is an overlay and count.

Rules:
- `SessionStart`: open session, state `started`, not automatically running.
- `UserPromptSubmit`: `running`.
- `PreToolUse`: `tool_running` with last **known** tool category; avoid claiming tool still active after next terminal event.
- `PermissionRequest`: `waiting_permission`, higher-priority attention. Clear on subsequent confirmed progression; if no confirmation, keep waiting until liveness check or explicit reconciliation.
- `PostToolUse`: clear matching active tool, return `running` unless turn is complete.
- `PostToolUseFailure`: record failure; remain running unless explicit stop/failure event.
- `Stop`: `turn_complete`, **not** `session_ended`, nor proof all project tasks finished.
- `SessionEnd`: `session_ended`.
- `SubagentStart/Stop`: track by IDs where available, allow independent in-flight status.
- No guaranteed "thinking" hook: label as `Processing` or `Active` unless separately verified; inference must display as estimated.
- Hook delivery can be missed or reordered; enforce event IDs, source timestamps, monotonic receive index, TTL, and periodic process liveness check (where PID available). Don't mark a session completed based solely on timeout; use `stale` or `unknown`.

### User actions
Click session opens details; "Focus app" is optional best-effort KDE window activation only after real user gesture. Mapping terminal/IDE window to session must be reliable; otherwise offer `Open project` instead. Never impersonate a permission approval.

## 5. Media specification (first-class)
Media is not a secondary afterthought. Use the standard `org.mpris.MediaPlayer2` interfaces on session D-Bus:
- Discover `org.mpris.MediaPlayer2.*`, subscribe to `PropertiesChanged`, `NameOwnerChanged`.
- Show `xesam:title`, `xesam:artist`, `mpris:artUrl`, player identity, `PlaybackStatus`, playback position/duration where provided.
- Controls: `PlayPause`, `Next`, `Previous`, seek/position **only** when player flags support it (`CanControl`, `CanSeek`, etc.).
- Multiple players: prefer user-pinned player, else current actively playing player, else last interacted one. Expose selector in expanded view.
- Art: async loaded; reject untrusted or oversized artwork; avoid displaying arbitrary remote art URIs without a privacy policy/caching guard.
- Paused tracks remain accessible but do not override urgent events. Show artwork thumbnail + title in compact; full progress and controls expanded.
- Keyboard volume is **system output volume**, not necessarily MPRIS per-player volume: wire PipeWire/WirePlumber integration separately.
- Do not promise universal Spotify/browser support when no MPRIS service exists; show diagnostics.

Source: https://specifications.freedesktop.org/mpris/latest/Player_Interface.html

## 6. Priority engine and interaction arbitration
Categories:
1. `critical`: low battery emergency, connectivity critical relevant to active job.
2. `attention`: agent permission request, agent failure requiring action.
3. `completion`: agent turn completed, build completed, download completed.
4. `ongoing`: agent work, media playing, download progress.
5. `ambient`: network, date/time, CPU.

Behavior:
- User-pinned activity stays visible unless critical or explicitly allowed attention override; music can be pinned while agents run.
- Brief completion banners expire after ~5 seconds (configurable), leaving full history in Activity Center.
- Permission alert persists until event clears/changes; no automatic permission submission.
- Debounce equal-priority events (2 seconds baseline), prevent flicker; simultaneous agents aggregate count and display session icon stack, rotate only on user action or opt-in carousel.
- User expanding the island freezes auto-rotation until collapse.
- Notification bridge never suppresses Plasma notifications by default; avoid duplicate banners for same source event.

## 7. Detailed interfaces
### Compact island
- idle: small, unobtrusive capsule with optional clock.
- music: artwork mini + title + animated sound bars (only when playing).
- agents: source logo + short project name + status (`Working`, `Permission`, `Done`) + concurrent count.
- mixed: leading priority status + trailing small media indicator if room; clicking expands multi-activity panel.
- transient: animated confirmation for agent turn completion or media change.

### Expanded island
- Header: active activity, source, last update, collapse control.
- Context card: permission / currently running tool / now playing.
- Agent tab: all live Claude Code + Qoder IDE sessions, grouped by provider/project, state, last observed event, relative time, subagent count, optional activity timeline.
- Music tab: art, track, artist, position, previous/play-next, player switch.
- Activity history tab: privacy-filtered recent events; clear button.
- System tab: battery/network/output volume; advanced metrics optional.
- Settings shortcut. Explicit keyboard focus navigation and screen-reader accessible labels.

## 8. Security / privacy / resilience
- Hook scripts and daemon run as ordinary user; no sudo, privileged APIs, or remote server.
- Strict event schema allowlist and producer credentials on Unix socket. Rate limiting & maximum message sizes.
- Do not serialize input prompt, tool args, command strings, tokens, full paths, tool results, or files. Session IDs should not leave host.
- Make installer reversible; generate backups before editing Claude/Qoder settings; support merging safely and idempotently.
- Ignore malicious hook JSON, Unicode control characters, huge strings, directory traversal and malformed timestamps.
- Failing core must not break Claude or Qoder, and failure handling cannot block UI main thread.
- Surface connection health: `Connected / Degraded / Not configured` per integration.
- Simulate restart mid-session and verify resumed state shows `unknown/stale`, not invented live state.

## 9. Performance / quality targets (to benchmark, not claimed as achieved)
- UI updates after accepted local event: p95 <250ms on development hardware.
- Idle CPU average <1% on representative desktop, measured over 10 minutes without animations; event driven where possible.
- Idle RAM goal <=120MB combined shell+daemon (adjust after baseline, record actual footprint).
- 60 FPS target for morphing on capable GPU; no excessive QML reflows.
- Hook bridge p95 execution <50ms when local daemon healthy; if unavailable short timeout <100ms.
- No >2 sec oscillation due to equal priority events; no duplicate session cards.
- No app focus theft; no lost input in fullscreen apps; no reserved dead zone at top of display by default.

## 10. Implementation plan / Codex order of execution
**Milestone 0: Discovery and bootstrap**
1. Detect distro, Qt and KDE versions, session type (`echo $XDG_SESSION_TYPE`), compositor protocols, available build dependencies, audio stack, vendor binaries and hooks config paths.
2. Confirm actual Qoder IDE build's 12 hook events, config merge format; inspect Claude Code hook schema in currently installed version. Preserve user config.
3. Create CMake project and unit-test harness; record unsupported APIs as risks instead of improvising.

**Milestone 1: Desktop + media MVP**
1. Implement standalone window + Qt Quick QML compact/expanded states; evaluate layer-shell behavior under KWin Wayland.
2. Connect MPRIS player list/status/metadata, play/pause/next/previous and multi-player selection.
3. Provide settings for position, theme, auto-expand, reduced motion, fullscreen hide. Test with at least two MPRIS providers if available.

**Milestone 2: Agent MVP**
1. Build Unix socket ingest / validator / normalizer + session state machine.
2. Implement idempotent Claude and Qoder IDE hook installers and bridge executable.
3. Support all 12 common hook events; test representative payload fixtures from each provider and degrade when field absent.
4. Add session cards, attention override, activity history, counter and last event, true turn completion semantics.

**Milestone 3: Productivity expansion**
1. Wire audio output level, power, network, optional notifications and KDE job progress behind capability toggles.
2. Add focus-app best effort, session filtering, pinned activity, multi-monitor preferences.
3. Tune animation/performance, accessibility, distribution packaging.

## 11. Acceptance criteria / test matrix
| ID | Test | Pass condition |
|---|---|---|
| AC-01 | Start NEXIS on KDE 6.7.5 Wayland | Capsule appears top-center, can expand/collapse, no panel resizing unexpectedly |
| AC-02 | Music player start/stop | Metadata + state changes reflect MPRIS without restart |
| AC-03 | Music controls | Play/pause/next/previous call correct selected MPRIS service; unsupported actions disabled |
| AC-04 | Multi-player | Selector changes controlled player without cross-control |
| AC-05 | Claude hook events | Start, prompt, tool, permission, stop, end produce correct transitions |
| AC-06 | Qoder IDE hook events | Same transitions verified with actual Qoder IDE, not CLI assumption |
| AC-07 | Parallel sessions | Two Claude sessions + one Qoder IDE show independently, no merge or duplicate |
| AC-08 | Turn complete vs ended | `Stop` shows turn complete; subsequent prompt reopens running, session remains until end |
| AC-09 | Permission | Attention persists; clicking never grants permission by itself |
| AC-10 | Missing data | Unknown IDs/status rendered cautiously; never fabricate tool/task labels |
| AC-11 | Daemon unavailable | Claude/Qoder continue normally; adapter exits success quickly; no accidental block |
| AC-12 | Reconnect | D-Bus/MPRIS service restart and daemon restart self-heal; stale state labeled |
| AC-13 | Settings safety | Hook install/uninstall preserves preexisting hooks, works twice without duplicates |
| AC-14 | Privacy | No raw prompts, tokens, tool outputs or absolute sensitive paths in default logs/storage |
| AC-15 | Priority | Permission beats ambient/music except explicit policy; pinned media remains retrievable |
| AC-16 | Reduced motion | Animations suppressed; controls remain usable |
| AC-17 | Multi-monitor | Disconnected display repositions island to available screen |
| AC-18 | Fullscreen | Default island hides or behaves as configured; does not steal focus |
| AC-19 | Performance | Benchmark output records actual idle CPU, memory, p95 update latency and animation FPS |
| AC-20 | Accessibility | Focusable controls, readable status labels, keyboard collapse; no color-only state |

## 12. Deliverables
- Source code and test suite, clean reproducible build.
- README with Ubuntu 24.04/KDE dependency instructions determined from actual package availability.
- `nexis doctor` output and integration diagnostic screen.
- Safe `hooks install/status/uninstall` and backups.
- Desktop file and user service, configurable autostart.
- `docs/KNOWN_LIMITATIONS.md` noting unsupported browser downloads, notifications, and window activation if needed.
- Screenshots/demo video optional after functional build (not a dependency).

## 13. Codex implementation instruction
> Build NEXIS iteratively from this specification. Prioritize a real, testable KDE Plasma 6.7.5 Wayland Qt6 floating island with working MPRIS controls, followed immediately by both Claude Code and Qoder IDE hook-driven session monitoring. Use official source documentation and the actual installed app versions. Do not invent events or APIs. Start with environment inventory and an implementation checklist. Keep hooks safe, minimal, non-blocking, reversible and privacy-filtered. Implement automated tests for state transitions, multiple concurrent sessions and adapter failure handling. At each milestone, run the build/tests and report pass/fail evidence before moving ahead. Do not silently substitute fake data in production; provide a clearly labeled demo mode for UI preview.

## References
- Claude Code Hooks Reference: https://code.claude.com/docs/en/hooks
- Qoder IDE Hooks: https://docs.qoder.com/extensions/hooks
- MPRIS Player interface: https://specifications.freedesktop.org/mpris/latest/Player_Interface.html
- KDE LayerShellQt: https://invent.kde.org/plasma/layer-shell-qt
