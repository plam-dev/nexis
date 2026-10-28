# Agent hooks

The user-selected providers are Claude Code and **the new Qoder app**, not Qoder IDE or standalone Qoder CLI. Stable CLI flags are `--provider claude` / `--provider qoder`; normalized identities are `claude_code` / `qoder_app`.

Claude settings: `~/.claude/settings.json`. Qoder shared user settings: `~/.qoder/settings.json`. Both adapters register only the twelve lifecycle events in the supplied spec: SessionStart, UserPromptSubmit, PreToolUse, PermissionRequest, PostToolUse, PostToolUseFailure, SubagentStart, SubagentStop, Stop, SessionEnd, PreCompact, Notification.

Each handler runs `nexis-hook-bridge --provider PROVIDER` asynchronously with a one-second host timeout. Its own I/O budget is shorter. Hooks contain no blocking decisions. NEXIS never writes stdout, submits a permission, executes an agent prompt, or reads a transcript to infer work.

The installed new Qoder app 0.4.4 confirms a shared `.qoder` config directory, Hooks settings, Agent SDK use and hook input contracts in its application bundle. [Qoder's data import documentation](https://docs.qoder.com/qoder/data-import) describes shared hook inheritance from IDE installations. [The shared hook contract reference](https://docs.qoder.com/cli/hooks) documents JSON groups and fields; it does not prove every event fires in the new desktop app. The diagnostic remains Not configured / Configured awaiting event until this collector actually receives events. There is no IDE/CLI capability claim.

[Claude's official reference](https://code.claude.com/docs/en/hooks) documents the matching hook structure and lifecycle fields. NEXIS intentionally consumes a common subset rather than registering every newer hook name.

Install/status/uninstall and dry-run usage are in the README. Merges are idempotent. Handler ownership is an exact trailing marker ` # nexis:v1:PROVIDER`; unrelated groups and entries survive uninstall. A removed executable or relocated build requires reinstalling hooks from the new installation path. Prefer installing NEXIS into `~/.local` before installing hooks permanently.

Representative hook input:

```json
{"hook_event_name":"PreToolUse","session_id":"opaque-session","cwd":"/work/project","tool_name":"Edit","tool_use_id":"opaque-tool"}
```

The bridge produces schema_version, UUID event_id, provider, source_event, hashed session/project identifiers, sanitized project_label, confidence, derived status, generic tool_kind, occurred_at, and optional hashed tool/subagent IDs and tool_fingerprint. Claude PermissionRequest omits tool_use_id, so the bridge hashes tool name and canonical tool_input JSON to correlate the later PostToolUse/PostToolUseFailure without forwarding arguments. PreToolUse alone does not resolve permission attention. Unknown fields never survive normalization. Payloads are capped at 16 KiB after normalization. Missing session IDs show limited confidence. Timeless vendor payloads receive a timestamp locally.

For acceptance with your apps: install hooks, start NEXIS, start one harmless task in the new Qoder app, and inspect System for Connected. Submit another turn to verify Stop reopens as running. Run a second session and verify separate cards. Permission controls remain in the originating application. This interactive vendor check has not been automated using paid agent work.
