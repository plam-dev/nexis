# Fold Island Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans for the UI. The independent notification bridge is delegated under superpowers:dispatching-parallel-agents. Steps use checkbox syntax for tracking.

**Goal:** Ship the approved Fold design as a floating island over the top panel, with centered content-sized capsules, real artwork, playback pulse, compact expanded controls and desktop notifications.

**Architecture:** Reuse the Qt6 backend and panel client. Share the capsule and Fold surface between the panel popup and standalone window. Keep Plasma as notification owner and receive desktop events through its watcher protocol.

**Tech Stack:** C++17, Qt6 Quick/Controls/DBus, KDE Plasma.

**Spec:** User-approved Fold prototype at `.superpowers/brainstorm/837428-1793125809/content/fold-centered-artwork-v2.html`, and the requests in this chat.

## Global Constraints

- Preserve the persistent Tuxedo cat, concurrent agent work, media capabilities, accessibility, reduced motion, and existing controls.
- No new dependencies; text stays plain text and artwork stays local and validated.
- Capsule content is centered as a group, with symmetric padding and intrinsic width bounded to available space.
- Notification previews are transient, memory-only, and never answer agent permission requests.
- Work in the current clean checkout on `codex/fold-island`; user has authorized implementation and local installation.

## Review Focus

- Extremely long titles must elide without overflowing or moving the content group off center.
- Missing/broken artwork must fall back to a music icon without changing text interpretation.
- Pause and reduced motion must stop the pulse; hidden surfaces must not animate.
- Daemon restart, notifications during music and permission alerts must preserve the highest-priority state.
- Panel sizing must follow content without minimum-size dead space, and popup height must follow its active section.

### Task 1: Shared capsule and Fold surface

**Files:** `ui/CompactActivity.qml`, new `ui/PlaybackPulse.qml`, new `ui/TrackArtwork.qml`, new `ui/FoldSurface.qml`, `apps/plasmoid/contents/ui/main.qml`, `apps/island/Main.qml`, `apps/island/resources.qrc`, `CMakeLists.txt`, `tests/gui/test_gui.cpp`.

- [x] Add failing GUI checks for intrinsic short/long/idle widths, centered group in excess width, artwork error fallback, pulse stop conditions, and Fold content height.
- [x] Run `cmake --build build -j4` and `ctest --test-dir build -R gui-interaction --output-on-failure`; observe failures before implementing.
- [x] Build shared artwork/pulse components and a content-sized capsule. Build a compact Fold player with icon controls, notice details, accessible secondary navigation for existing agent/activity/system functions.
- [x] Replace panel popup and standalone content with the shared Fold surface; keep top-level native positioning and focus behavior.
- [x] Re-run GUI tests and fix all QML warnings.

### Task 2: KDE notification bridge

**Files:** `modules/notifications/Notifications.h`, `modules/notifications/Notifications.cpp`, `tests/integration/test_notifications.cpp`, `services/core/Backend.h`, `services/core/Backend.cpp`, `apps/panel-client/DesktopClient.cpp`, `CMakeLists.txt`.

- [x] Use the existing KDE watcher protocol; exercise it against a private-bus fake daemon before implementation.
- [x] Bound and sanitize incoming text, authenticate daemon callbacks, respect inhibition, expire banners and recover from daemon restart. Demo never observes live notifications.
- [x] Export notification list and presentation override, with explicit dismiss/open actions and unchanged permission priority.
- [x] Verify private-bus tests; integrate the shared UI with the exported snapshot.

### Task 3: Verify and install

**Files:** `docs/PANEL.md`, `docs/KNOWN_LIMITATIONS.md`, `docs/VERIFICATION.md`, `README.md`.

- [x] Build and run all CTest suites; render the native shared components and inspect screenshots.
- [x] Have a fresh reviewer inspect the change and correct important findings.
- [x] Install through the existing installer, reload the local panel safely, and confirm the service and plugin are healthy.
- [x] Exercise one labeled desktop test notification and confirm it reaches the island through the real daemon.
- [x] Document behavior, test evidence and any remaining limitations.

## Execution record

- Baseline: clean checkout at `09af4f1`; existing 10-suite baseline running.
- Ruling: execute the already-approved design without another approval round; preserve the current checkout and use a feature branch.
- Shared capsule/Fold surface completed and verified: artwork, group centering, paused playback context, hidden-window pulse gating, preserved local jobs, theme brightness and overall scroll bound.
- Fresh code review found hidden-window animation, overflow, theme and navigation regressions; the relevant fixes and GUI regressions are in place. Full suite passed 11/11 after integration.
- Live check: installed to existing panel 200/widget 230 without replacing other widgets. Initial notification bridge connected, media artwork present, reduced motion false.
- Live KDE style check exposed a custom ComboBox content item incompatible with KDE's cursor helper; restored its native content item.
- A subsequent Plasma restart exposed delayed watcher availability; registration retry is being implemented and tested before final live acceptance.
- User steering after viewing the installed widget: use a standalone floating island above the top panel, not an embedded panel widget; allow wider titles. Disable the headless panel service, remove only the owned NEXIS widget, enable the GUI service, retain the rest of the panel. Closed capsule height becomes 52 px, text can grow to 380 px within a 560 px overall limit, and expanded width becomes 420 px. The layer has zero exclusive zone and stays centered while changing size.

- Final user steering implemented: notch geometry attaches to the output edge with exclusive zone -1; collapsed width follows the longest rendered line with no artificial minimum and a maximum of 60% of the selected screen. Music always opens first; Activity logs and the history switch are removed, history defaults off, and live job progress remains under Agents.
- Final verification: CMake build and all 11 CTest suites passed (20.36 s); installed binary matches the build, GUI service is active with no restarts, notification bridge Connected, history false/empty, and no new QML load/type/binding errors.
