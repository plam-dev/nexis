# Development environment — 2026-10-28

- Fedora Linux 43, kernel 7.2.7; ordinary user build, no system packages installed.
- Installed KDE Plasma / KWin 6.7.5, Qt 6.10.3.
- Running compositor exposes zwlr_layer_shell_v1 v5 and xdg_activation_v1 v1; Wayland socket /run/user/1000/wayland-0.
- Build shell does not inherit graphical session variables; launch scripts explicitly recover the current user's runtime bus and Wayland socket.
- Orchis Dark global theme: #282828 windows, #303030 views, #dfdfdf text, #1a73e8 selected accent. Tela-circle-dark icons. Auto mode reads kdeglobals rather than assuming a light desktop.
- PipeWire 1.4.11 and WirePlumber 0.5.14, wpctl available.
- Claude Code 2.1.310.
- **User-selected target: new Qoder desktop app 0.4.4**, RPM `qoder`, executable `/opt/Qoder/qoder`. The separate IDE 1.30.1 and CLI 1.1.62 are not integration targets.
- Installed Qoder app package.json confirms the app identity/version and Agent SDK dependency. Its main bundle exposes Hooks settings, user settings.json in the product's .qoder directory, and SDK hook callbacks. These facts do not prove live hook delivery; diagnostics require an observed event.
- Qt development RPMs downloaded from Fedora repositories and extracted into ignored `.deps/root`; system Qt libraries reused via local symlinks. Reproducible setup: scripts/bootstrap-fedora.sh.
- User-provided @RTK.md reference: no RTK.md exists in the workspace or ancestor home/main directories.

Official references checked: [Claude hooks](https://code.claude.com/docs/en/hooks), [Qoder shared configuration inheritance](https://docs.qoder.com/qoder/data-import), [Qoder product overview](https://docs.qoder.com/qoder/overview), [Qoder hook contracts](https://docs.qoder.com/cli/hooks), [MPRIS](https://specifications.freedesktop.org/mpris/latest/Player_Interface.html). Qoder's CLI reference is used only for the shared hook JSON contract, not as evidence of desktop delivery or feature parity.
