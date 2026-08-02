# Configuration

AgentLauncher is config-driven. All agent definitions live in a single
`agents.json` file, copied into your user config directory on first run:

| OS | Path |
|---|---|
| Windows | `%LOCALAPPDATA%\AgentLauncher\agents.json` |
| Linux | `~/.config/AgentLauncher/agents.json` |
| macOS | `~/Library/Preferences/AgentLauncher/agents.json` |

A bundled default is shipped inside the app (from `config/default_agents.json`).

## Agent entry

```json
{
    "id": "kimi-code",
    "name": "Kimi Code",
    "command": "kimi web",
    "webUrl": "http://127.0.0.1:58627",
    "configDir": "%USERPROFILE%/.kimi-code",
    "icon": "qrc:/icons/kimi-code.svg",
    "color": "#FF6B35"
}
```

| Field | Purpose |
|---|---|
| `id` | Stable identifier used by the UI to locate an agent |
| `name` | Card title |
| `command` | Shell command run (detached) by the **Start** button |
| `webUrl` | URL health-checked every few seconds and opened in the browser when the card is clicked |
| `configDir` | Directory opened by the **Open** button on the Configure page; supports `%VAR%` expansion |
| `icon` | Icon resource path (e.g. `qrc:/icons/...`) |
| `color` | Highlight color (border + tinted background) shown while the agent is running |

## Running detection

Each `webUrl` is polled via HTTP every 3 seconds. **Any** HTTP response (even a
`401`/`404`) means the server is up → the card is marked **Running**. A
connection refusal or timeout means it is **Stopped**.

This works across all agents because they all expose an HTTP endpoint once
their web server starts.

## Default agents

| Agent | Launch command | Web URL | Config directory |
|---|---|---|---|
| Kimi Code | `kimi web` | `http://127.0.0.1:58627` | `%USERPROFILE%/.kimi-code` |
| OpenCode | `opencode web --port 4096` | `http://127.0.0.1:4096` | `%USERPROFILE%/.config/opencode` |
| Qwen Code | `qwen serve` | `http://127.0.0.1:4170` | `%USERPROFILE%/.qwen` |

!!! note "OpenCode uses a random port by default"
    OpenCode picks a random free port each run, which makes health-checking and
    "open in browser" unreliable. The default config pins it to `4096` (both the
    `--port` flag and the Web URL). Change it on the Configure page if `4096` is
    taken.

## Changing the port

Open the **Configure** page for an agent and edit the startup command, e.g.:

```
kimi web --port 58628
```

Then update the **Web URL** to match (`http://127.0.0.1:58628`) and **Save**.
The two are stored separately so you keep full control — just keep them in sync.

## Adding a new agent

1. Open `agents.json` in your config directory.
2. Append a new object to the `agents` array with all fields filled in.
3. Restart AgentLauncher (or it will pick up changes on next launch).

No recompilation needed.
