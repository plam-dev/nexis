# Privacy and resilience

NEXIS sends no telemetry and opens no network listener. It uses local Unix sockets and desktop D-Bus. All components run as the logged-in user.

The bridge parses at most 64 KiB of vendor JSON. It constructs a new event object from an allowlist; raw prompt, command, tool arguments/results, transcripts, messages, tokens, full paths and environment values are discarded. Known tool names become generic categories. It does not extract target paths. Session, workspace, tool-call and subagent IDs are hashed. Only a sanitized project basename is displayed; project names can still be sensitive, so disable history when appropriate.

No raw event is logged by default. Rejected events are dropped silently. Default history is bounded and stored only in memory, with a user-visible clear/off switch. Restart drops all sessions and history; nothing is invented as live following restart. Settings contain appearance and interaction preferences, never prompts or source code.

Hook settings backups preserve the complete original config and may contain secrets already in that config. They are created with mode 0600 beside the settings file. The dry-run diff displays only NEXIS-owned handler changes. Atomic settings replacement preserves the existing file mode; malformed files and symlinks are refused. A lock serializes NEXIS installers, and a final byte comparison detects many external edits; a third-party writer ignoring locks can still race the final atomic replacement.

Stdin has a 35 ms deadline, local connect 20 ms and write 20 ms. The bridge always returns exit code zero with no stdout, including invalid data and absent collectors. It never emits decision JSON or grants/denies permission. OS scheduling/startup can increase wall-clock duration; timings are measured, not hard real-time guarantees.

Artwork is local-only with file type/size/dimension checks. No remote artwork is downloaded. Existing media providers may independently use the network. WirePlumber actions occur only following direct UI interaction. The KWin observer reads fullscreen state and output names; it never sends window titles, application contents, or task information.

The same-UID trust boundary prevents other users from submitting events. Software already running as this user can publish misleading allowlisted metadata; it is not cryptographic proof that a vendor emitted an event. Qoder products share configuration, so the configured desktop target cannot be distinguished reliably from another Qoder product loading the same user hooks.
