# Settings Page & Launcher Management — Design

Date: 2026-09-03
Status: Approved (user confirmed approach A and full design)

## Background

AgentLauncher is config-driven: agent definitions live in `agents.json`, seeded
from the bundled `:/config/default_agents.json` on first run. Today the only
way to add an agent or edit more than `command`/`webUrl`/`setupCommand` is to
hand-edit the on-disk config at:

```
%LOCALAPPDATA%\AgentLauncher\agents.json        (Windows)
```

(resolved by `AgentConfig::configFilePath()` via
`QStandardPaths::AppConfigLocation`; `agent_state.json` sits next to it).

Users of released builds cannot find this file and manual JSON editing is
error-prone. This feature adds in-app launcher management.

## Goals

1. A settings button in the bottom-right corner of the main window opens a
   settings page.
2. The settings page lists all launchers, allows editing any of them, adding
   new ones, and deleting them (including built-in ones).
3. The add/edit form exposes every `agents.json` field as UI controls.
4. Required fields are marked with a red `*`; every field label shows a
   tooltip explaining its meaning on hover.

## Non-goals

- Editing the root `title` config field.
- Reordering agents (drag & drop).
- Graceful shutdown of a deleted/edited agent's process.
- Port-collision detection between agents.

## Chosen approach

Approach A (chosen over modal-popup and separate-window alternatives): new
QML pages pushed onto the existing `StackView`, with CRUD logic behind
`Q_INVOKABLE`s on `AgentLauncher`, persisting through `AgentConfig::save()`.

Rationale: consistent with existing navigation (current `ConfigPage`), full
space for a 12-field form, reuses the dark theme controls, and keeps the
"UI never holds its own copy of agent data" convention — all writes go
through the model.

## UI design

### Entry button (main.qml)

- Floating button (~44×44, radius 12) anchored bottom-right of the window
  with 20px margins, declared after the `StackView` so it renders above the
  scrollable home page.
- Style: `#313244` background, hover highlight, gear glyph (new
  `icons/gear.svg`, registered in CMake resources), tooltip "Settings".
- Click → `stack.push(settingsPageComp)`.

### SettingsPage.qml (new)

- Top bar with back button (pops the StackView) + "Settings" title, matching
  the existing `ConfigPage` styling.
- Scrollable column with section headers, structured so more settings
  sections can be added later. First (and only) section: "Launchers".
- Header row: section label + "Add Launcher" button → pushes
  `AgentEditPage` with empty `agentId`.
- Launcher list (Repeater over `agentModel`): each row shows the agent icon,
  name, `command` as secondary text, a running-state dot, and Edit / Delete
  buttons.
  - Edit → pushes `AgentEditPage` with that `agentId`.
  - Delete → confirmation dialog:
    - If the agent is running: extra warning that the process is NOT
      terminated, only removed from the list.
    - If the agent is a built-in default: note that it can be brought back
      via "Restore default launchers".
- "Restore default launchers" text button under the list: clears the
  removal record and re-merges the bundled defaults (does not overwrite
  existing on-disk field values; only re-adds missing agents).

### AgentEditPage.qml (new, replaces ConfigPage.qml)

One component, two modes: `agentId === ""` → add mode; otherwise edit mode
(fields pre-filled from `agentModel.agent(id)`).

Form sections and fields (all optional fields may stay empty):

| Section | Fields | Notes |
|---|---|---|
| Basics | Name `*`, Command `*`, Web URL `*`, Config directory, ID | ID: optional in add mode (auto-generated from Name when left empty, slugified + uniquified with `-2`, `-3`… suffixes); read-only in edit mode (it keys `agent_state.json`) |
| Appearance | Icon, Color, Card color | Icon: free-text field (built-in `qrc:/icons/*.svg`, local path, or `http(s)://` URL) with a live preview and clickable built-in icon thumbnails as quick picks. Colors: `#RRGGBB` text field with live swatch preview; empty Color = auto-assigned palette color; empty Card color = default `#313244` |
| Install & maintenance | Install command, Update command, Version command | Tooltips include the npm-style examples from `default_agents.json` |
| Advanced | First-run setup command, Token file | Setup tooltip documents the one-time-run semantics and `agent_state.json` |

Interaction rules:

- Required-field labels are prefixed with a red `*` (`#f38ba8`).
- Every field label has a hover tooltip (MouseArea + ToolTip, same pattern
  as the Python/Node badges on the home page) explaining the field's
  meaning plus an example.
- Validation: required fields non-empty; `webUrl` must parse as an
  `http(s)` URL; `color`/`cardColor`, when non-empty, must match
  `#[0-9a-fA-F]{6}`; manually entered ID must not collide with an existing
  agent id. Invalid fields get a red border; Save stays disabled until the
  form is valid.
- Edit mode shows a hint line: changes take effect on next launch (running
  processes are untouched).
- Save → `launcher.addAgent(...)` / `launcher.updateAgentFull(...)` → pop
  back; a `false` return (disk write failure) opens the existing red
  errorPopup.

### Home page wiring change

`AgentCard`'s configure signal currently pushes `ConfigPage.qml`; it will
push the new `AgentEditPage` instead. `ConfigPage.qml` is deleted and both
of its references in `CMakeLists.txt` (resources + `qt6_create_translation`)
are updated.

## C++ design

### AgentConfig

- New member `QStringList m_removedIds`, persisted as a root `"removed"`
  array in `agents.json` (empty/absent = no removals).
- `parse()` reads `"removed"`; `save()` writes it.
- `migrate()` skips any bundled default whose id is in `m_removedIds` —
  this is what makes deleting built-in agents stick across restarts.
- `resolveIcon()`: pass `file://` URLs through unchanged. Fixes an existing
  round-trip bug: a local-path icon is resolved to a `file:///` URL and
  saved back; on next load `QFileInfo` on the URL string fails and the
  icon silently falls back to `default.svg`. The edit form's round-trip
  depends on this fix.
- New static helper `AgentConfig::defaultAgentIds()` returning the id set of
  the bundled `default_agents.json`, used by `removeAgent()` to decide
  whether a removed id must be recorded in `"removed"`.

### AgentModel

- `insertAgent(int row, const Agent &)` and `removeAgentById(const QString &id)`
  with proper `beginInsertRows` / `beginRemoveRows` so the home-page
  Repeater updates automatically.
- Existing roles suffice; no new roles needed (`agent(id)` already returns
  every persisted field including `tokenFile`).

### AgentLauncher (QML surface)

- `Q_INVOKABLE bool addAgent(const QVariantMap &fields)` — validates,
  generates a unique id when absent, appends to the model, saves. Returns
  false on save failure.
- `Q_INVOKABLE bool updateAgentFull(const QString &id, const QVariantMap &fields)`
  — replaces every persisted field, emits `dataChanged` for all roles,
  saves. Replaces the current 4-argument `updateAgent()`, which is removed
  together with its only caller (`ConfigPage.qml`).
- `Q_INVOKABLE bool removeAgent(const QString &id)` — removes from the
  model; if the id belongs to a bundled default, records it in
  `m_removedIds`; never kills processes. Persisted via save.
- `Q_INVOKABLE bool restoreDefaults()` — clears `m_removedIds` and re-runs
  the default merge so deleted built-ins reappear (existing user edits are
  preserved because migration only fills empty fields).

## Data flow

QML form → `AgentLauncher` invokable → mutates `AgentModel` (home page
refreshes via model signals) → `AgentConfig::save()` writes
`agents.json` (+ `"removed"` array). Read path unchanged: on startup
`load()` seeds, migrates (now honoring removals), and assigns palette
colors.

## Error handling

- Save failure (file not writable): invokable returns false; QML shows the
  existing centered red errorPopup with the reason.
- Duplicate manually-entered id: inline form error, Save disabled.
- Deleting a running agent: confirmation dialog explicitly states the
  process keeps running and must be stopped separately (e.g. Force Stop on
  the card before deleting, or via the agent's own command).

## Internationalization

All new user-visible strings use English source wrapped in `qsTr()`.
`lupdate` runs at build time; Chinese translations are filled into
`translations/agentlauncher_zh_CN.ts` as part of implementation. The two
CMake references to `ConfigPage.qml` are updated to the new QML files.

## Verification

No test infrastructure exists in the repo (no CTest), so verification is:

1. Full build with the documented CMake commands.
2. Manual walkthrough: add (all-optional-empty and fully-filled), edit
   (incl. pre-fill correctness), delete built-in + user agent, restore
   defaults, restart app → agents.json and UI state persist correctly.
3. Verify a deleted built-in agent does not reappear after restart.
4. Verify zh_CN translations render (run with Chinese locale).
