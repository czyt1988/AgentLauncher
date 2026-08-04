# AGENTS.md

Guidance for AI coding agents working in this repository.

## What this project is

AgentLauncher is a Qt6/QML + C++ desktop app that launches the web UI of AI
coding agents from a card grid. It is **config-driven**: agent definitions
(commands, web URLs, config directories, colors) live in `agents.json`, not
in C++.

## Build

```bash
cmake -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.7.3/msvc2019_64"
cmake --build build
```

- Qt 6.5+ required (modules: Core, Gui, Qml, Quick, Network).
- CMake 3.16+, C++17.
- Generator: Ninja (preferred) or MSBuild. On Windows with MSVC, run from a
  developer command prompt or call `vcvars64.bat` first.

## Layout

```
src/         C++ backend: AgentConfig, AgentModel, AgentLauncher, main.cpp
qml/         QML UI: main.qml, AgentCard.qml, ConfigPage.qml
config/      default_agents.json (bundled as a Qt resource)
icons/       SVG icons (bundled as Qt resources)
docs/        MkDocs site (English + zh/)
```

## Config schema (agents.json)

Each agent object has: `id`, `name`, `command`, `webUrl`, `configDir`, `icon`,
`color`, `installCommand`, `updateCommand`, `versionCommand`. See
`config/default_agents.json`. New agents are added by editing this file (and
the on-disk copy at `AppConfigLocation/agents.json`). Do **not** hard-code
agent entries in C++.

On load, `AgentConfig::load()` merges the bundled default into the on-disk
config: any field that is empty on disk is filled from the default, and any
agent present in the default but missing on disk is appended. This migration
ensures older configs (created before `installCommand`/`updateCommand`/
`versionCommand` existed) get the new fields automatically. The updated config
is persisted back to disk if anything changed.

## Conventions

- All agent state (commands, URLs) comes from `AgentConfig`. The launcher
  reads/writes via the model; the UI never holds its own copy of agent data.
- Running state is detected by HTTP health check to `webUrl` (any HTTP
  response = running; connection refused/timeout = stopped). Do not add
  process-sniffing logic — keep it HTTP-based for cross-tool consistency.
- Launch uses `QProcess::startDetached` so agents survive the launcher closing.
- Environment variables in `configDir` use `%VAR%` (Windows) form; the launcher
  expands them. `~` is also expanded to the home dir.
- When changing QML, keep the dark theme colors (Catppuccin Mocha palette).

## Don't

- Don't hard-code agent definitions in C++.
- Don't run `git commit`/`git push` unless explicitly asked.

## Stopping agents

Cards show a subtle × in the top-right corner while an agent is running.
Clicking it terminates the process tree **this launcher started this
session** — `launch()` records the PID returned by `startDetached`, and
`stop()` runs `taskkill /F /T /PID …` (Windows) so the whole
`cmd → .cmd → node` tree dies. PIDs are in-memory only, so:

- An agent detected as running via the HTTP health check but **not started
  from this launcher** has no PID; `stop()` shows a message instead of
  killing anything. Stop those via their own command.
- After the launcher restarts, PIDs are lost — running agents detected by
  health check can't be stopped from the card until re-launched here.

Force-kill is intentional for a dev-server launcher; agents that need
graceful shutdown should still be stopped via their own command.

## Launching on Windows

`launch()` resolves the bare program via `QStandardPaths::findExecutable`
(which applies PATHEXT), then runs `.cmd`/`.bat` shims through `cmd /c` —
`CreateProcess` alone won't find npm-style shims like `qwen.cmd`. Launch
failures emit `launchFailed(id, message)`; the UI shows an at-place red flash
on the card plus a popup with the reason.
