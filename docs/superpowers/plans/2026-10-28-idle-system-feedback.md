# Idle companion and system feedback implementation plan

> **For agentic workers:** Use superpowers:executing-plans to implement this plan task by task. Steps use checkbox syntax for tracking.

**Goal:** Replace the idle clock/date with a quietly animated cat, and briefly show relevant desktop changes in the island.

**Architecture:** Keep the shared QML cat/capsule and backend presentation. Listen to Plasma's existing OSD signals on the session bus, exposing a single expiring feedback banner; retain work/listening flags underneath it. Reuse that banner for successful volume changes from the island's own controls.

**Tech Stack:** Qt6/C++, QML Canvas/Layouts, session D-Bus, CTest.

**Spec:** The user selected “Kucing + suasana kecil + info saat relevan”, explicitly including volume and brightness changes. Implementation choices: no idle text; existing breathing/blinking plus occasional ear/tail movements; a three-second banner refreshed by each adjustment; progress bar for numeric values. Native KDE OSD remains enabled. Critical battery/permission alerts outrank feedback; feedback temporarily precedes ordinary notifications/music. Reduced motion and hidden windows stop pet motion.

## Global constraints

- Preserve existing work/music animation, notification history and focus behavior.
- No new dependencies, background shell polling or changes to KDE shortcuts/settings.
- Bound display text and progress values. Only consume the owner of org.kde.plasmashell.
- No fake startup banner; expire transient state and return to the current activity.

## Review focus

- Repeated adjustments must restart expiry; stale timers must not dismiss newer events.
- Reject forged signals; recover when Plasma restarts.
- Feedback must not hide critical/permission alerts or alter pet work/listening state.
- Idle must fit/center without an invisible text column or stale progress bar.
- Island volume controls must report confirmed changes, not failed writes.

### Task 1: Desktop feedback ingestion and priority

**Files:** Add modules/system/Osd.{h,cpp}, tests/integration/test_osd.cpp; modify CMakeLists.txt, modules/system/System.{h,cpp}, services/core/Backend.{h,cpp}.

- [x] Add private-bus integration tests for numeric/text events through Backend::Snapshot, priority, expiry refresh, forged senders, owner restart, disabled demo mode and invalid bounds.
- [x] Run the tests against the existing backend and observe missing feedback.
- [x] Implement the bounded three-second Osd banner, native signal subscriptions and successful local-volume feedback. Merge feedback after normal notifications, retaining pet state and critical/permission priority.
- [x] Build and run the integration tests.

### Task 2: Quiet idle and compact feedback

**Files:** Modify ui/CompactActivity.qml, ui/TuxedoCat.qml, tests/gui/test_gui.cpp and docs/PANEL.md.

- [x] Update idle GUI expectations to require no clock/date and add feedback progress/return-to-idle rendering checks; observe the old behavior failing.
- [x] Hide the idle text column, remove its clock timer, show a bounded numeric progress bar for feedback, and add bounded ear/tail variation using the existing idle timer.
- [x] Run the GUI checks and all CTest suites; inspect rendered idle/feedback images and git diff.
- [x] Update the user-local binary, restart the existing standalone service and verify live D-Bus feedback and expiry without changing hardware values.

## Execution notes

- Task 1: desktop feedback tests passed (8/8 cases, private bus); native events, expiry refresh, owner replacement and successful/failed local volume writes verified.
- Task 2: GUI checks passed, including text-free idle sizing, rendered breathing/ear/tail differences and static reduced motion. Shared QML idle/volume/brightness/status preview rendered and visually inspected.
- Full-suite testing exposed duplicate notification registration when startup discovery and a name-owner event observe the same daemon. A focused regression failed before adding an idempotent owner guard, then passed.
- Fresh independent review found no important issues. No production settings, shortcuts or hardware levels were changed.
- Final verification: build succeeded; CTest 12/12 suites passed. The user-local standalone service is running the new binary. Live Plasma volume (63%), brightness (40%) and mute OSD methods reached the island, then expired back to the current activity after three seconds; these display-only methods do not alter hardware settings.

## Follow-up: optional idle clock/date

The user requested an on/off option for clock/date. The existing `idleClock` setting is now wired to both compact surfaces, with switches in System and Preferences. It defaults off, preserves saved choices and uses the existing typed settings/snapshot path. The clock timer runs only for visible idle clock content; feedback/music/alerts retain priority. Tests cover on/off rendering, persisted reload, boolean action validation and panel forwarding. Fresh build and CTest: 12/12 suites pass. Both variants were rendered and inspected, and the installed live service accepted on/off changes; the original preference was restored.
