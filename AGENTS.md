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
`color`. See `config/default_agents.json`. New agents are added by editing
this file (and the on-disk copy at `AppConfigLocation/agents.json`). Do **not**
hard-code agent entries in C++.

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
- Don't add a "stop agent" feature (each agent manages its own lifecycle).
- Don't run `git commit`/`git push` unless explicitly asked.
