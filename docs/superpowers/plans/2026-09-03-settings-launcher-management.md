# Settings Page & Launcher Management Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** In-app launcher management — a bottom-right settings button opening a settings page where launchers can be viewed, edited, added, and deleted (including built-ins, with restore), replacing manual `agents.json` editing.

**Architecture:** New QML pages (`SettingsPage.qml`, `AgentEditPage.qml`) pushed onto the existing `StackView`; CRUD behind `Q_INVOKABLE`s on `AgentLauncher` that mutate `AgentModel` (UI refreshes via model signals) and persist through `AgentConfig::save()`. Deleted built-in agent ids are persisted as a root `"removed"` array so `migrate()` does not resurrect them.

**Tech Stack:** Qt 6.5+ (QML/Quick Controls 2, Basic style), C++17, CMake + Ninja/MSVC, QtTest for the new C++ unit tests.

**Spec:** `docs/superpowers/specs/2026-09-03-settings-launcher-management-design.md`

## Global Constraints

- Qt 6.5+ required; build with `cmake -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.7.3/msvc2019_64"` then `cmake --build build` (from a VS developer prompt / after `vcvars64.bat` — see AGENTS.md).
- All user-visible strings: English source wrapped in `qsTr()`. Chinese translations go ONLY into `translations/agentlauncher_zh_CN.ts`.
- Dark theme colors only (Catppuccin Mocha: bg `#1e1e2e`, surface `#313244`, border `#45475a`, text `#cdd6f4`, muted `#a6adc8`/`#7f849c`, accent blue `#89b4fa`, red `#f38ba8`, green `#a6e3a1`, yellow `#f9e2af`).
- No hard-coded agent definitions in C++.
- **Per AGENTS.md: do NOT run `git commit` or `git push` unless the user explicitly asks.** Skip per-task commit steps; after the final task, ask the user once whether to commit.
- Comments/identifiers/log messages in English.
- QML model role for the agent id is **`agentId`** (not `id`) — see `AgentModel::roleNames()`.
- QML gotchas honored in this plan: never anchor children of Layout/positioner items (use `Layout.*` attached properties or child-parent anchoring on plain Items); never name a QML property `data` or `required` (collides with built-ins/keywords); MouseArea+ToolTip tooltip pattern follows the home-page badges in `main.qml`.

## File Structure

```
src/AgentConfig.h/.cpp    MODIFY  removed-ids persistence, loadDefaults/defaultAgentIds/
                            appendMissingDefaults/slugFromName/paletteColorAt,
                            public static resolveIcon + file:// passthrough;
                            delete unused AgentConfig::updateAgent
src/AgentModel.h/.cpp     MODIFY  insertAgent / removeAgentById
src/AgentLauncher.h/.cpp  MODIFY  addAgent / updateAgentFull / removeAgent / restoreDefaults /
                            setRemovedIds / isDefaultAgent / configFilePath;
                            old 4-arg updateAgent removed in Task 6 with ConfigPage.qml
src/main.cpp              MODIFY  pass removedIds into the launcher
qml/AgentEditPage.qml     CREATE  full-field add/edit form (required marks, tooltips, validation)
qml/SettingsPage.qml      CREATE  launcher list + add/edit/delete + restore defaults
qml/main.qml              MODIFY  floating settings button, card → AgentEditPage, drop ConfigPage
qml/ConfigPage.qml        DELETE  (replaced by AgentEditPage)
icons/gear.svg            CREATE  settings button glyph
tests/tst_core.cpp        CREATE  QtTest unit tests (AgentConfig, AgentModel, AgentLauncher)
CMakeLists.txt            MODIFY  new QML/icon resources, translation list, test target
translations/agentlauncher_zh_CN.ts MODIFY  fill Chinese for all new strings
AGENTS.md                 MODIFY  document the "removed" root key and the settings UI
```

---

### Task 1: AgentConfig — removal tracking, helpers, icon round-trip fix (+ test scaffolding)

**Files:**
- Create: `tests/tst_core.cpp`
- Modify: `src/AgentConfig.h`, `src/AgentConfig.cpp`, `CMakeLists.txt`

**Interfaces:**
- Consumes: existing `AgentConfig` (`load/save/parse/migrate/resolveIcon`), `Agent` struct, bundled `:/config/default_agents.json`.
- Produces (used by Tasks 2–7):
  - `QStringList AgentConfig::removedIds() const` / `void setRemovedIds(const QStringList &)`
  - `static QList<Agent> AgentConfig::loadDefaults(QString *outTitle = nullptr)`
  - `static QStringList AgentConfig::defaultAgentIds()`
  - `static bool AgentConfig::appendMissingDefaults(QList<Agent> &agents)`
  - `static QString AgentConfig::slugFromName(const QString &name)`
  - `static QString AgentConfig::paletteColorAt(int index)`
  - `static QString AgentConfig::resolveIcon(const QString &raw)` (moved from private to public)
  - New root JSON key `"removed"` (array of ids) in `agents.json`.

- [ ] **Step 1: Write the failing tests**

Create `tests/tst_core.cpp`:

```cpp
#include <QtTest>
#include <QStandardPaths>
#include <QTemporaryFile>

#include "AgentConfig.h"

// Unit tests for the config/model/launcher core. All config reads and
// writes are isolated from the user's real config via
// QStandardPaths test mode.
class TestCore : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QStandardPaths::setTestModeEnabled(true);
        QFile::remove(AgentConfig::configFilePath());
    }

    void testSlugFromName()
    {
        QCOMPARE(AgentConfig::slugFromName(QStringLiteral("Kimi Code")),
                 QStringLiteral("kimi-code"));
        QCOMPARE(AgentConfig::slugFromName(QStringLiteral("My_Agent! 2")),
                 QStringLiteral("my-agent-2"));
        QCOMPARE(AgentConfig::slugFromName(QStringLiteral("  --Trim--  ")),
                 QStringLiteral("trim"));
        QCOMPARE(AgentConfig::slugFromName(QStringLiteral("???")),
                 QStringLiteral("agent"));
    }

    void testResolveIconPassthrough()
    {
        // Regression: file:// URLs must pass through unchanged, otherwise a
        // resolved local-file icon degrades to default.svg after save+reload.
        QCOMPARE(AgentConfig::resolveIcon(QStringLiteral("file:///C:/icons/a.svg")),
                 QStringLiteral("file:///C:/icons/a.svg"));
        QCOMPARE(AgentConfig::resolveIcon(QStringLiteral("")),
                 QStringLiteral("qrc:/icons/default.svg"));
        QCOMPARE(AgentConfig::resolveIcon(QStringLiteral("qrc:/icons/bot.svg")),
                 QStringLiteral("qrc:/icons/bot.svg"));

        QTemporaryFile tmp;
        QVERIFY(tmp.open());
        QVERIFY(AgentConfig::resolveIcon(tmp.fileName())
                    .startsWith(QStringLiteral("file:///")));
    }

    void testRemovedIdsRoundTrip()
    {
        {
            AgentConfig cfg;
            cfg.load(); // seed the test-mode file from the bundled defaults
            cfg.setRemovedIds({"agent-one", "agent-two"});
            QVERIFY(cfg.save());
        }
        AgentConfig reloaded;
        reloaded.load();
        QCOMPARE(reloaded.removedIds(),
                 QStringList({"agent-one", "agent-two"}));
    }

    void testMigrateSkipsRemovedDefaults()
    {
        {
            AgentConfig cfg;
            cfg.load(); // seed defaults into the test-mode location

            // Simulate a Settings-page deletion: drop the agent from the
            // list AND record its id, then persist (same sequence
            // AgentLauncher::removeAgent() produces).
            QList<Agent> remaining;
            for (const Agent &a : cfg.agents()) {
                if (a.id != QStringLiteral("kimi-code"))
                    remaining.append(a);
            }
            cfg.setAgents(remaining);
            cfg.setRemovedIds({QStringLiteral("kimi-code")});
            QVERIFY(cfg.save());
        }
        AgentConfig reloaded;
        reloaded.load();
        QVERIFY(reloaded.removedIds().contains(QStringLiteral("kimi-code")));
        for (const Agent &a : reloaded.agents())
            QVERIFY2(a.id != "kimi-code", "removed default must not resurrect");
    }

    void testDefaultAgentIds()
    {
        const QStringList ids = AgentConfig::defaultAgentIds();
        QVERIFY(ids.contains(QStringLiteral("kimi-code")));
        QVERIFY(ids.contains(QStringLiteral("opencode")));
    }
};

QTEST_MAIN(TestCore)
#include "tst_core.moc"
```

- [ ] **Step 2: Add the test target to CMakeLists.txt**

Append at the end of `CMakeLists.txt`:

```cmake
# --- Unit tests ---------------------------------------------------------------
# Isolated from the real user config via QStandardPaths test mode (see
# tests/tst_core.cpp). Disable with -DBUILD_TESTING=OFF (e.g. if the Qt
# installation lacks the Test component).
option(BUILD_TESTING "Build the unit test target" ON)

if (BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
    enable_testing()
    qt_add_executable(AgentLauncherTests
        tests/tst_core.cpp
        src/AgentConfig.cpp
        src/AgentModel.cpp
        src/AgentLauncher.cpp
    )
    qt_add_resources(AgentLauncherTests "test_resources"
        PREFIX "/"
        FILES
            config/default_agents.json
    )
    target_link_libraries(AgentLauncherTests PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::Test
    )
    add_test(NAME AgentLauncherTests COMMAND AgentLauncherTests)
endif()
```

- [ ] **Step 3: Run the tests to verify they fail**

```bash
cmake -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.7.3/msvc2019_64"
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: **compile error** — `removedIds`, `slugFromName`, `loadDefaults`, `defaultAgentIds`, `resolveIcon` not declared on `AgentConfig`. That is the failing state.

- [ ] **Step 4: Implement the AgentConfig changes**

`src/AgentConfig.h` — replace the class definition with:

```cpp
class AgentConfig
{
public:
    void load();
    bool save();

    QList<Agent> agents() const { return m_agents; }
    void setAgents(const QList<Agent> &agents) { m_agents = agents; }

    QString title() const { return m_title; }
    void setTitle(const QString &title) { m_title = title; }

    // Ids of built-in agents the user deleted (Settings page). Persisted as
    // the root "removed" array so migrate() does not resurrect them.
    QStringList removedIds() const { return m_removedIds; }
    void setRemovedIds(const QStringList &ids) { m_removedIds = ids; }

    // The bundled default agent definitions from :/config/default_agents.json.
    // outTitle, when not null, receives the default root title.
    static QList<Agent> loadDefaults(QString *outTitle = nullptr);

    // Ids of the bundled default agents.
    static QStringList defaultAgentIds();

    // Append bundled defaults whose id is not yet in the list. Returns true
    // when the list changed. Used by "Restore default launchers".
    static bool appendMissingDefaults(QList<Agent> &agents);

    // Turn a display name into a config id: "Kimi Code" -> "kimi-code".
    static QString slugFromName(const QString &name);

    // Catppuccin Mocha palette color by position (auto color assignment).
    static QString paletteColorAt(int index);

    // Resolve an icon string to a displayable image URL: qrc:/, http(s)://,
    // file:// pass through; an existing local file becomes a file:/// URL;
    // anything else (including empty) falls back to qrc:/icons/default.svg.
    static QString resolveIcon(const QString &raw);

    static QString configFilePath();

private:
    QList<Agent> m_agents;
    QString m_title;
    QStringList m_removedIds;

    QList<Agent> parse(const QByteArray &data, QString &outTitle) const;

    // Fill in empty fields from the bundled default config and add missing
    // agents (skipping removed ids). Called after load() so on-disk configs
    // created from older defaults get the new fields populated automatically.
    void migrate(const QList<Agent> &defaults, const QString &defaultTitle);

    // Assign a color from the built-in palette to every agent whose `color`
    // is still empty after migration. Colors are assigned by cycling through
    // the palette based on the agent's position in the list. Returns true if
    // any color was assigned (so the caller can persist).
    bool assignPaletteColors();

    // Expand %VAR% environment variables and ~ in a path.
    static QString expandEnv(const QString &path);
};
```

Also add `#include <QStringList>` next to the existing `#include <QString>` in `src/AgentConfig.h`, and **delete** the now-obsolete `void AgentConfig::updateAgent(const QString &id, const QString &command, const QString &webUrl)` function from `src/AgentConfig.cpp` (its declaration is dropped by the header rewrite above; the existing `AgentLauncher::updateAgent` invokable does not call it — it mutates the model directly — so removing it now is safe).

`src/AgentConfig.cpp` changes:

(a) Replace the bundled-default loading block in `load()` (lines 33–40) with:

```cpp
    // Load the bundled default config for migration fallback.
    QString defaultTitle;
    const QList<Agent> defaults = loadDefaults(&defaultTitle);
```

(b) In `parse()`, right after `outTitle = root.value(QStringLiteral("title")).toString();`, add:

```cpp
    m_removedIds.clear();
    const QJsonArray removed = root.value(QStringLiteral("removed")).toArray();
    for (const QJsonValue &v : removed)
        m_removedIds.append(v.toString());
```

(c) In `save()`, replace the root-object block with:

```cpp
    QJsonObject root;
    root[QStringLiteral("title")] = m_title;
    root[QStringLiteral("agents")] = arr;
    if (!m_removedIds.isEmpty()) {
        QJsonArray removed;
        for (const QString &id : m_removedIds)
            removed.append(id);
        root[QStringLiteral("removed")] = removed;
    }
```

(d) In `migrate()`, add the skip at the top of the defaults loop:

```cpp
    for (const Agent &def : defaults) {
        // User deleted this built-in agent — do not resurrect it.
        if (m_removedIds.contains(def.id))
            continue;
        auto it = std::find_if(...
```

(e) In `resolveIcon()`, add `file://` to the pass-through list:

```cpp
    // Built-in resources, remote URLs and file URLs are used as-is.
    if (raw.startsWith(QStringLiteral("qrc:/"))
        || raw.startsWith(QStringLiteral("http://"))
        || raw.startsWith(QStringLiteral("https://"))
        || raw.startsWith(QStringLiteral("file://")))
        return raw;
```

(f) Refactor the palette in `assignPaletteColors()` into `paletteColorAt()` and simplify:

```cpp
bool AgentConfig::assignPaletteColors()
{
    bool changed = false;
    for (int i = 0; i < m_agents.size(); ++i) {
        if (m_agents[i].color.isEmpty()) {
            m_agents[i].color = paletteColorAt(i);
            changed = true;
        }
    }
    return changed;
}

QString AgentConfig::paletteColorAt(int index)
{
    // Catppuccin Mocha palette — vibrant colors that read well on the dark
    // card background (#313244).
    static const QStringList palette = {
        QStringLiteral("#f38ba8"), // Red
        QStringLiteral("#fab387"), // Peach
        QStringLiteral("#f9e2af"), // Yellow
        QStringLiteral("#a6e3a1"), // Green
        QStringLiteral("#94e2d5"), // Teal
        QStringLiteral("#89b4fa"), // Blue
        QStringLiteral("#cba6f7"), // Mauve
        QStringLiteral("#f5c2e7"), // Pink
    };
    return palette.at(((index % palette.size()) + palette.size()) % palette.size());
}
```

(Delete the old static `palette` list inside `assignPaletteColors()`.)

(g) Add the new static functions at the end of the file:

```cpp
QList<Agent> AgentConfig::loadDefaults(QString *outTitle)
{
    QFile def(QStringLiteral(":/config/default_agents.json"));
    if (!def.open(QIODevice::ReadOnly)) {
        if (outTitle)
            outTitle->clear();
        return {};
    }
    QString title;
    const QList<Agent> agents = AgentConfig().parse(def.readAll(), title);
    if (outTitle)
        *outTitle = title;
    return agents;
}

QStringList AgentConfig::defaultAgentIds()
{
    QStringList ids;
    const QList<Agent> defaults = loadDefaults();
    for (const Agent &a : defaults)
        ids.append(a.id);
    return ids;
}

bool AgentConfig::appendMissingDefaults(QList<Agent> &agents)
{
    bool changed = false;
    const QList<Agent> defaults = loadDefaults();
    for (const Agent &def : defaults) {
        const bool exists = std::any_of(agents.cbegin(), agents.cend(),
            [&](const Agent &a) { return a.id == def.id; });
        if (!exists) {
            agents.append(def);
            changed = true;
        }
    }
    return changed;
}

QString AgentConfig::slugFromName(const QString &name)
{
    QString s = name.toLower().trimmed();
    s.remove(QRegularExpression(QStringLiteral("[^a-z0-9\\s_-]")));
    s.replace(QRegularExpression(QStringLiteral("[\\s_]+")), QStringLiteral("-"));
    s.replace(QRegularExpression(QStringLiteral("-+")), QStringLiteral("-"));
    while (s.startsWith(QLatin1Char('-')))
        s.remove(0, 1);
    while (s.endsWith(QLatin1Char('-')))
        s.chop(1);
    if (s.isEmpty())
        s = QStringLiteral("agent");
    return s;
}
```

- [ ] **Step 5: Run the tests to verify they pass**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: `100% tests passed, 0 tests failed` (5 test slots in `TestCore`).

---

### Task 2: AgentModel — insert/remove rows

**Files:**
- Modify: `src/AgentModel.h`, `src/AgentModel.cpp`, `tests/tst_core.cpp`

**Interfaces:**
- Consumes: `Agent` struct, existing `QAbstractListModel` base.
- Produces (used by Task 3):
  - `void AgentModel::insertAgent(int row, const Agent &agent)` (clamps out-of-range rows to append)
  - `bool AgentModel::removeAgentById(const QString &id)` (false when id not found)

- [ ] **Step 1: Write the failing tests**

In `tests/tst_core.cpp`, add `#include "AgentModel.h"` to the includes and this slot to `TestCore` (after `testDefaultAgentIds`):

```cpp
    void testModelInsertRemove()
    {
        AgentModel model;
        Agent a;
        a.id = "x1";
        a.name = "X";
        model.setAgents({a});
        QCOMPARE(model.rowCount(), 1);

        Agent b;
        b.id = "x2";
        b.name = "Y";
        model.insertAgent(0, b);
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.index(0, 0).data(AgentModel::IdRole).toString(),
                 QStringLiteral("x2"));
        QCOMPARE(model.index(1, 0).data(AgentModel::IdRole).toString(),
                 QStringLiteral("x1"));

        QVERIFY(model.removeAgentById("x1"));
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.index(0, 0).data(AgentModel::IdRole).toString(),
                 QStringLiteral("x2"));
        QVERIFY(!model.removeAgentById("missing"));
        QCOMPARE(model.rowCount(), 1);
    }
```

- [ ] **Step 2: Run the tests to verify they fail**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: compile error — `insertAgent` / `removeAgentById` not declared.

- [ ] **Step 3: Implement**

In `src/AgentModel.h`, after `void setAgents(...)` add:

```cpp
    // Insert an agent at the given row (out-of-range rows append). Emits the
    // usual rowsInserted signals so bound views refresh.
    void insertAgent(int row, const Agent &agent);

    // Remove the agent with the given id. Returns false when not found.
    bool removeAgentById(const QString &id);
```

In `src/AgentModel.cpp`, after `setAgents()` add:

```cpp
void AgentModel::insertAgent(int row, const Agent &agent)
{
    if (row < 0 || row > m_agents.size())
        row = m_agents.size();
    beginInsertRows(QModelIndex(), row, row);
    m_agents.insert(row, agent);
    endInsertRows();
}

bool AgentModel::removeAgentById(const QString &id)
{
    const int row = indexOf(id);
    if (row < 0)
        return false;
    beginRemoveRows(QModelIndex(), row, row);
    m_agents.removeAt(row);
    endRemoveRows();
    return true;
}
```

- [ ] **Step 4: Run the tests to verify they pass**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: 6 test slots pass.

---

### Task 3: AgentLauncher — CRUD invokables + main.cpp wiring

**Files:**
- Modify: `src/AgentLauncher.h`, `src/AgentLauncher.cpp`, `src/main.cpp`, `tests/tst_core.cpp`

**Interfaces:**
- Consumes: Task 1 (`AgentConfig::removedIds/setRemovedIds/defaultAgentIds/appendMissingDefaults/slugFromName/paletteColorAt/resolveIcon`) and Task 2 (`insertAgent/removeAgentById`).
- Produces (used by Tasks 4–6 QML):
  - `Q_INVOKABLE bool AgentLauncher::addAgent(const QVariantMap &fields)` — keys: `id` (optional), `name`, `command`, `webUrl`, `configDir`, `icon`, `color`, `cardColor`, `installCommand`, `updateCommand`, `versionCommand`, `setupCommand`, `tokenFile`. Returns false on duplicate id or write failure.
  - `Q_INVOKABLE bool AgentLauncher::updateAgentFull(const QString &id, const QVariantMap &fields)` — same keys, `id` ignored (immutable); preserves runtime state flags.
  - `Q_INVOKABLE bool AgentLauncher::removeAgent(const QString &id)` — never kills processes.
  - `Q_INVOKABLE bool AgentLauncher::restoreDefaults()`
  - `Q_INVOKABLE bool AgentLauncher::isDefaultAgent(const QString &id) const`
  - `Q_INVOKABLE QString AgentLauncher::configFilePath() const`
  - `void AgentLauncher::setRemovedIds(const QStringList &ids)`
  - REMOVED (in Task 6, together with ConfigPage.qml, its only caller): the old `Q_INVOKABLE bool AgentLauncher::updateAgent(const QString &id, const QString &command, const QString &webUrl, const QString &setupCommand)`. `AgentConfig::updateAgent` is already deleted in Task 1; keep the launcher invokable until Task 6 so the old ConfigPage keeps working in between.

- [ ] **Step 1: Write the failing tests**

In `tests/tst_core.cpp`, add `#include "AgentLauncher.h"` and this slot:

```cpp
    void testLauncherCrud()
    {
        AgentConfig cfg;
        cfg.load(); // seed test-mode config with the bundled defaults
        AgentModel model;
        model.setAgents(cfg.agents());
        AgentLauncher launcher(&model);
        launcher.setRemovedIds(cfg.removedIds());
        const int baseCount = model.rowCount();
        QVERIFY(baseCount > 0);

        // --- addAgent with auto id --------------------------------------
        QVariantMap fields;
        fields.insert(QStringLiteral("name"), QStringLiteral("My Agent"));
        fields.insert(QStringLiteral("command"), QStringLiteral("myagent web"));
        fields.insert(QStringLiteral("webUrl"), QStringLiteral("http://127.0.0.1:9999"));
        QVERIFY(launcher.addAgent(fields));
        QCOMPARE(model.rowCount(), baseCount + 1);
        QCOMPARE(model.agent(QStringLiteral("my-agent"))
                     .value(QStringLiteral("name")).toString(),
                 QStringLiteral("My Agent"));
        // Auto-assigned palette color so the card renders correctly.
        QVERIFY(!model.agent(QStringLiteral("my-agent"))
                     .value(QStringLiteral("color")).toString().isEmpty());

        // Persisted?
        AgentConfig persisted;
        persisted.load();
        bool found = false;
        for (const Agent &a : persisted.agents())
            found = found || a.id == "my-agent";
        QVERIFY(found);

        // --- addAgent duplicate name -> -2 suffix ------------------------
        QVERIFY(launcher.addAgent(fields));
        QCOMPARE(model.rowCount(), baseCount + 2);
        QVERIFY(model.indexOf(QStringLiteral("my-agent-2")) >= 0);

        // --- addAgent explicit duplicate id -> false ----------------------
        QVariantMap dup;
        dup.insert(QStringLiteral("id"), QStringLiteral("my-agent"));
        dup.insert(QStringLiteral("name"), QStringLiteral("X"));
        dup.insert(QStringLiteral("command"), QStringLiteral("x"));
        dup.insert(QStringLiteral("webUrl"), QStringLiteral("http://127.0.0.1:1"));
        QVERIFY(!launcher.addAgent(dup));

        // --- updateAgentFull ---------------------------------------------
        fields.insert(QStringLiteral("name"), QStringLiteral("Renamed"));
        QVERIFY(launcher.updateAgentFull(QStringLiteral("my-agent"), fields));
        QCOMPARE(model.agent(QStringLiteral("my-agent"))
                     .value(QStringLiteral("name")).toString(),
                 QStringLiteral("Renamed"));

        // --- removeAgent of a built-in -> recorded in "removed" ----------
        QVERIFY(launcher.removeAgent(QStringLiteral("kimi-code")));
        QVERIFY(model.indexOf(QStringLiteral("kimi-code")) < 0);
        AgentConfig after;
        after.load();
        QVERIFY(after.removedIds().contains(QStringLiteral("kimi-code")));

        // --- restoreDefaults brings it back -------------------------------
        QVERIFY(launcher.restoreDefaults());
        QVERIFY(model.indexOf(QStringLiteral("kimi-code")) >= 0);
        AgentConfig after2;
        after2.load();
        QVERIFY(!after2.removedIds().contains(QStringLiteral("kimi-code")));

        // --- isDefaultAgent / configFilePath ------------------------------
        QVERIFY(launcher.isDefaultAgent(QStringLiteral("opencode")));
        QVERIFY(!launcher.isDefaultAgent(QStringLiteral("my-agent")));
        QVERIFY(launcher.configFilePath().endsWith(QStringLiteral("agents.json")));
    }
```

- [ ] **Step 2: Run the tests to verify they fail**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: compile error — new invokables not declared.

- [ ] **Step 3: Implement AgentLauncher**

`src/AgentLauncher.h`:
- Add `#include <QStringList>` and `#include <QVariantMap>` next to the existing includes.
- Add the following declarations (keep the old `updateAgent` invokable for now — it is removed in Task 6 together with its caller):

```cpp
    // Launcher management (Settings page). All three persist to agents.json
    // and return false when the file could not be written (or, for addAgent,
    // when the requested id already exists).
    Q_INVOKABLE bool addAgent(const QVariantMap &fields);
    Q_INVOKABLE bool updateAgentFull(const QString &id, const QVariantMap &fields);
    Q_INVOKABLE bool removeAgent(const QString &id);
    Q_INVOKABLE bool restoreDefaults();

    // True when the id belongs to the bundled default_agents.json.
    Q_INVOKABLE bool isDefaultAgent(const QString &id) const;

    // Path of the on-disk agents.json (shown in error messages).
    Q_INVOKABLE QString configFilePath() const;

    // Deleted built-in ids to persist across restarts; loaded once at
    // startup by main.cpp.
    void setRemovedIds(const QStringList &ids) { m_removedIds = ids; }
```

- In the `private:` section add:

```cpp
    QStringList m_removedIds;

    // Write the model's agents + removal records to agents.json.
    bool saveConfig();
```

`src/AgentLauncher.cpp`:
- Keep the old `updateAgent(...)` invokable for now (its only caller, ConfigPage.qml, is deleted in Task 6).
- Add the new anonymous-namespace helper and functions (the anonymous `namespace { ... }` can be a second block directly above `addAgent`, or merged into the existing one at the top of the file):

```cpp
namespace {

// Build an Agent from the field map the QML edit form submits. The id comes
// from the caller (generated or immutable), the icon is resolved so the
// model always holds a displayable URL.
Agent agentFromFields(const QVariantMap &f, const QString &id)
{
    Agent a;
    a.id = id;
    a.name = f.value(QStringLiteral("name")).toString().trimmed();
    a.command = f.value(QStringLiteral("command")).toString().trimmed();
    a.webUrl = f.value(QStringLiteral("webUrl")).toString().trimmed();
    a.configDir = f.value(QStringLiteral("configDir")).toString().trimmed();
    a.icon = AgentConfig::resolveIcon(f.value(QStringLiteral("icon")).toString().trimmed());
    a.color = f.value(QStringLiteral("color")).toString().trimmed();
    a.cardColor = f.value(QStringLiteral("cardColor")).toString().trimmed();
    a.installCommand = f.value(QStringLiteral("installCommand")).toString().trimmed();
    a.updateCommand = f.value(QStringLiteral("updateCommand")).toString().trimmed();
    a.versionCommand = f.value(QStringLiteral("versionCommand")).toString().trimmed();
    a.setupCommand = f.value(QStringLiteral("setupCommand")).toString().trimmed();
    a.tokenFile = f.value(QStringLiteral("tokenFile")).toString().trimmed();
    return a;
}

} // namespace

bool AgentLauncher::addAgent(const QVariantMap &fields)
{
    QString id = fields.value(QStringLiteral("id")).toString().trimmed();
    if (id.isEmpty()) {
        // Generate from the display name, uniquified with -2, -3, ... suffixes.
        const QString base = AgentConfig::slugFromName(
            fields.value(QStringLiteral("name")).toString());
        id = base;
        int n = 2;
        while (m_model->indexOf(id) >= 0)
            id = base + QLatin1Char('-') + QString::number(n++);
    } else if (m_model->indexOf(id) >= 0) {
        return false; // duplicate id (the form prevents this; defensive)
    }

    Agent a = agentFromFields(fields, id);
    // Empty color would render a broken card until the next restart
    // (load() assigns palette colors); assign one now.
    if (a.color.isEmpty())
        a.color = AgentConfig::paletteColorAt(m_model->agents().size());

    m_model->insertAgent(m_model->agents().size(), a);
    return saveConfig();
}

bool AgentLauncher::updateAgentFull(const QString &id, const QVariantMap &fields)
{
    const int row = m_model->indexOf(id);
    if (row < 0)
        return false;

    Agent a = agentFromFields(fields, id); // id is immutable
    if (a.color.isEmpty())
        a.color = AgentConfig::paletteColorAt(row);

    // Preserve runtime state flags; only the persisted fields change.
    const Agent &old = m_model->agents().at(row);
    a.running = old.running;
    a.launching = old.launching;
    a.installed = old.installed;
    a.version = old.version;
    a.installing = old.installing;
    a.setupDone = old.setupDone;
    a.setupping = old.setupping;
    a.checkingVersion = old.checkingVersion;
    a.consoleOutput = old.consoleOutput;

    m_model->agents()[row] = a;
    const QModelIndex idx = m_model->index(row, 0);
    emit m_model->dataChanged(idx, idx); // no roles = all roles

    return saveConfig();
}

bool AgentLauncher::removeAgent(const QString &id)
{
    if (!m_model->removeAgentById(id))
        return false;

    // Record deleted built-ins so migrate() does not resurrect them.
    if (AgentConfig::defaultAgentIds().contains(id) && !m_removedIds.contains(id))
        m_removedIds.append(id);

    // The process itself keeps running on purpose (documented in the UI).
    m_pids.remove(id);
    m_launchEpoch.remove(id);
    m_versionEpoch.remove(id);
    return saveConfig();
}

bool AgentLauncher::restoreDefaults()
{
    m_removedIds.clear();
    QList<Agent> agents = m_model->agents();
    AgentConfig::appendMissingDefaults(agents);
    m_model->setAgents(agents); // beginResetModel/endResetModel inside
    return saveConfig();
}

bool AgentLauncher::isDefaultAgent(const QString &id) const
{
    return AgentConfig::defaultAgentIds().contains(id);
}

QString AgentLauncher::configFilePath() const
{
    return AgentConfig::configFilePath();
}

bool AgentLauncher::saveConfig()
{
    AgentConfig cfg;
    cfg.setAgents(m_model->agents());
    cfg.setRemovedIds(m_removedIds);
    return cfg.save();
}
```

- [ ] **Step 4: Wire removedIds in main.cpp**

In `src/main.cpp`, after `AgentLauncher launcher(&model);` (before `launcher.start();`) add:

```cpp
    // Deletion records for built-in agents, persisted in agents.json.
    launcher.setRemovedIds(config.removedIds());
```

- [ ] **Step 5: Run the tests to verify they pass**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: 7 test slots pass, including `testLauncherCrud`.

- [ ] **Step 6: App still builds and runs**

```bash
cmake --build build
./build/AgentLauncher.exe &
```

Expected: app starts, cards render as before (the old ConfigPage still works until Task 6).

---

### Task 4: AgentEditPage.qml — the full-field form

**Files:**
- Create: `qml/AgentEditPage.qml`
- Modify: `CMakeLists.txt` (resources + translation lists)

**Interfaces:**
- Consumes: `agentModel` (context property, role `agentId`, invokables `agent(id)` / `indexOf(id)`), `launcher` (context property, `addAgent`/`updateAgentFull`/`configFilePath` from Task 3).
- Produces: `qml/AgentEditPage.qml` — a `Page` with `property string agentId` (empty = add mode), self-popping via `page.StackView.view.pop()`.

Design notes: field labels use a `FormLabel` inline component (label text + red `*` + `ℹ` icon, each Label carrying a MouseArea+ToolTip like the home-page badges); the input is a **sibling** below the label, not a nested child (avoids `default property alias` semantics inside inline components). Icon quick-pick uses clickable built-in icon thumbnails (no ComboBox — keeps the dark theme without restyling popups).

- [ ] **Step 1: Create `qml/AgentEditPage.qml`**

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Full add/edit form for one launcher. Empty agentId = add mode.
// NOTE: do not name the agent map property `data` — it collides with
// QQuickItem's built-in `data` group and silently breaks field bindings.
Page {
    id: page

    property string agentId: ""
    readonly property bool isAdd: agentId.length === 0
    property var agentData: agentId.length > 0 ? agentModel.agent(agentId) : ({})

    background: Rectangle { color: "#1e1e2e" }

    // --- Validation --------------------------------------------------------
    readonly property bool nameValid: nameField.text.trim().length > 0
    readonly property bool commandValid: commandField.text.trim().length > 0
    readonly property bool webUrlValid: /^https?:\/\/\S+$/.test(webUrlField.text.trim())
    readonly property bool colorValid: colorField.text.trim().length === 0
                                      || /^#[0-9a-fA-F]{6}$/.test(colorField.text.trim())
    readonly property bool cardColorValid: cardColorField.text.trim().length === 0
                                           || /^#[0-9a-fA-F]{6}$/.test(cardColorField.text.trim())
    readonly property bool idValid: {
        if (!isAdd)
            return true
        const t = idField.text.trim()
        if (t.length === 0)
            return true
        return /^[A-Za-z0-9_-]+$/.test(t) && agentModel.indexOf(t) < 0
    }
    readonly property bool formValid: nameValid && commandValid && webUrlValid
                                      && colorValid && cardColorValid && idValid

    function save() {
        const fields = {
            "name": nameField.text.trim(),
            "command": commandField.text.trim(),
            "webUrl": webUrlField.text.trim(),
            "configDir": configDirField.text.trim(),
            "icon": iconField.text.trim(),
            "color": colorField.text.trim(),
            "cardColor": cardColorField.text.trim(),
            "installCommand": installField.text.trim(),
            "updateCommand": updateField.text.trim(),
            "versionCommand": versionField.text.trim(),
            "setupCommand": setupField.text.trim(),
            "tokenFile": tokenFileField.text.trim()
        }
        let ok = false
        if (isAdd) {
            fields["id"] = idField.text.trim()
            ok = launcher.addAgent(fields)
        } else {
            ok = launcher.updateAgentFull(page.agentId, fields)
        }
        if (ok)
            page.StackView.view.pop()
        else
            saveErrorPopup.open()
    }

    // Label row: text + red required marker + info icon, each Label is a
    // tooltip hover source (MouseArea child, like the home-page badges).
    component FormLabel: RowLayout {
        id: labelRow
        Layout.fillWidth: true
        property string labelText: ""
        property bool isRequired: false
        property string tip: ""
        spacing: 2

        Label {
            text: labelRow.labelText
            color: "#a6adc8"
            font.pixelSize: 13

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                ToolTip.text: labelRow.tip
                ToolTip.visible: containsMouse && labelRow.tip.length > 0
                ToolTip.delay: 300
            }
        }
        Label {
            text: "*"
            color: "#f38ba8"
            font.pixelSize: 13
            visible: labelRow.isRequired
        }
        Label {
            text: "\u2139"
            color: "#6c7086"
            font.pixelSize: 12
            visible: labelRow.tip.length > 0

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                ToolTip.text: labelRow.tip
                ToolTip.visible: containsMouse
                ToolTip.delay: 300
            }
        }
        Item { Layout.fillWidth: true }
    }

    // Dark-themed TextField with an invalid (red border) state.
    component FormTextField: TextField {
        id: input
        property bool invalid: false
        Layout.fillWidth: true
        color: "#cdd6f4"
        background: Rectangle {
            radius: 8
            color: "#313244"
            border.color: input.invalid ? "#f38ba8" : "#45475a"
        }
    }

    component SectionLabel: Label {
        color: "#89b4fa"
        font.pixelSize: 15
        font.bold: true
        Layout.topMargin: 14
        Layout.fillWidth: true
    }

    ScrollView {
        id: scrollView
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: scrollView.availableWidth
            spacing: 12

            RowLayout {
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                Layout.topMargin: 24
                spacing: 12

                Button {
                    text: qsTr("\u2190 Back")
                    background: Rectangle { color: "transparent" }
                    contentItem: Label { text: parent.text; color: "#89b4fa"; font.pixelSize: 16 }
                    onClicked: page.StackView.view.pop()
                }
                Item { Layout.fillWidth: true }
            }

            ColumnLayout {
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                spacing: 4

                Label {
                    text: page.isAdd ? qsTr("Add Launcher") : qsTr("Edit Launcher")
                    color: "#cdd6f4"
                    font.pixelSize: 24
                    font.bold: true
                }
                Label {
                    visible: !page.isAdd
                    text: page.agentData.running
                          ? qsTr("This agent is running. Changes take effect on the next launch.")
                          : qsTr("Changes are saved to the configuration file.")
                    color: "#6c7086"
                    font.pixelSize: 12
                }
            }

            // --- Basics ---------------------------------------------------
            ColumnLayout {
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                spacing: 10
                Layout.fillWidth: true

                SectionLabel { text: qsTr("Basics") }

                FormLabel {
                    labelText: qsTr("Name")
                    isRequired: true
                    tip: qsTr("Display name shown on the launcher card, e.g. \"Kimi Code\".")
                }
                FormTextField {
                    id: nameField
                    text: page.agentData.name || ""
                    placeholderText: qsTr("e.g. Kimi Code")
                    invalid: !page.nameValid
                }

                FormLabel {
                    labelText: qsTr("Command")
                    isRequired: true
                    tip: qsTr("Command line that starts the agent, e.g. \"kimi web --port 58628\". It runs in the background without a visible window.")
                }
                FormTextField {
                    id: commandField
                    text: page.agentData.command || ""
                    placeholderText: qsTr("e.g. opencode web --port 4096")
                    invalid: !page.commandValid
                }

                FormLabel {
                    labelText: qsTr("Web URL")
                    isRequired: true
                    tip: qsTr("The agent's web UI address. Used as a health check to detect whether the agent is running, and opened in the browser. Keep the port in sync with the command.")
                }
                FormTextField {
                    id: webUrlField
                    text: page.agentData.webUrl || ""
                    placeholderText: qsTr("e.g. http://127.0.0.1:4096")
                    invalid: !page.webUrlValid
                }
                Label {
                    visible: !page.webUrlValid && webUrlField.text.trim().length > 0
                    text: qsTr("Must be a valid http:// or https:// URL.")
                    color: "#f38ba8"
                    font.pixelSize: 11
                }

                FormLabel {
                    labelText: qsTr("ID")
                    tip: qsTr("Unique identifier stored in the configuration file. Leave empty to generate it from the name. It cannot be changed after creation.")
                }
                FormTextField {
                    id: idField
                    text: page.isAdd ? "" : page.agentId
                    placeholderText: qsTr("auto-generated from name")
                    readOnly: !page.isAdd
                    color: page.isAdd ? "#cdd6f4" : "#7f849c"
                    invalid: !page.idValid
                }

                FormLabel {
                    labelText: qsTr("Config directory")
                    tip: qsTr("The agent's own configuration folder, e.g. \"%USERPROFILE%/.kimi-code\". Opened from the card's context menu. %VAR% and ~ are expanded.")
                }
                FormTextField {
                    id: configDirField
                    text: page.agentData.configDir || ""
                    placeholderText: qsTr("e.g. %USERPROFILE%/.config/opencode")
                }
            }

            // --- Appearance -----------------------------------------------
            ColumnLayout {
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                spacing: 10
                Layout.fillWidth: true

                SectionLabel { text: qsTr("Appearance") }

                FormLabel {
                    labelText: qsTr("Icon")
                    tip: qsTr("Built-in icon (qrc:/icons/<name>.svg), a local file path (%VAR% and ~ expanded), or an http(s):// URL. Leave empty for the default icon. Click a built-in icon below to fill the field.")
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    FormTextField {
                        id: iconField
                        text: page.agentData.icon || ""
                        placeholderText: qsTr("qrc:/icons/<name>.svg, file path or URL")
                    }
                    // Live preview (qrc/http/file only; raw local paths are
                    // resolved by the C++ side on save).
                    Image {
                        source: {
                            const t = iconField.text.trim()
                            if (t.startsWith("qrc:/") || t.startsWith("http://")
                                    || t.startsWith("https://") || t.startsWith("file://"))
                                return t
                            return "qrc:/icons/default.svg"
                        }
                        sourceSize: Qt.size(30, 30)
                        fillMode: Image.PreserveAspectFit
                    }
                }
                // Built-in icon quick picks.
                Row {
                    spacing: 6

                    Repeater {
                        model: ["default", "terminal", "cube", "bot"]

                        delegate: Item {
                            width: 30
                            height: 30

                            Rectangle {
                                anchors.fill: parent
                                radius: 8
                                color: pickArea.containsMouse ? "#45475a" : "#313244"
                                border.color: "#45475a"
                            }
                            Image {
                                anchors.centerIn: parent
                                source: "qrc:/icons/" + modelData + ".svg"
                                sourceSize: Qt.size(20, 20)
                                fillMode: Image.PreserveAspectFit
                            }
                            MouseArea {
                                id: pickArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: iconField.text = "qrc:/icons/" + modelData + ".svg"
                            }
                        }
                    }
                }

                FormLabel {
                    labelText: qsTr("Color")
                    tip: qsTr("Accent color of the card in #RRGGBB form, e.g. \"#89b4fa\". Leave empty to auto-assign a color from the built-in palette.")
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    FormTextField {
                        id: colorField
                        text: page.agentData.color || ""
                        placeholderText: qsTr("auto-assigned")
                        invalid: !page.colorValid
                    }
                    Rectangle {
                        width: 30
                        height: 30
                        radius: 8
                        color: page.colorValid && colorField.text.trim().length > 0
                               ? colorField.text.trim() : "transparent"
                        border.color: "#45475a"
                    }
                }

                FormLabel {
                    labelText: qsTr("Card color")
                    tip: qsTr("Background color of the card in #RRGGBB form while the agent is not running. Leave empty for the default (#313244).")
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    FormTextField {
                        id: cardColorField
                        text: page.agentData.cardColor || ""
                        placeholderText: qsTr("default (#313244)")
                        invalid: !page.cardColorValid
                    }
                    Rectangle {
                        width: 30
                        height: 30
                        radius: 8
                        color: page.cardColorValid && cardColorField.text.trim().length > 0
                               ? cardColorField.text.trim() : "transparent"
                        border.color: "#45475a"
                    }
                }
            }

            // --- Install & maintenance ------------------------------------
            ColumnLayout {
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                spacing: 10
                Layout.fillWidth: true

                SectionLabel { text: qsTr("Install & Maintenance") }

                FormLabel {
                    labelText: qsTr("Install command")
                    tip: qsTr("Command that installs the agent, e.g. \"npm install -g @kimi-code/cli\". Offered on the card when the agent is not installed.")
                }
                FormTextField {
                    id: installField
                    text: page.agentData.installCommand || ""
                    placeholderText: qsTr("e.g. npm install -g opencode-ai")
                }

                FormLabel {
                    labelText: qsTr("Update command")
                    tip: qsTr("Command that updates the agent to the latest version, e.g. \"npm update -g @kimi-code/cli\". Run from the card's context menu.")
                }
                FormTextField {
                    id: updateField
                    text: page.agentData.updateCommand || ""
                    placeholderText: qsTr("e.g. npm update -g opencode-ai")
                }

                FormLabel {
                    labelText: qsTr("Version command")
                    tip: qsTr("Command that prints the agent's version, e.g. \"kimi --version\". Run silently at startup to detect whether the agent is installed.")
                }
                FormTextField {
                    id: versionField
                    text: page.agentData.versionCommand || ""
                    placeholderText: qsTr("e.g. opencode --version")
                }
            }

            // --- Advanced -------------------------------------------------
            ColumnLayout {
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                spacing: 10
                Layout.bottomMargin: 24
                Layout.fillWidth: true

                SectionLabel { text: qsTr("Advanced") }

                FormLabel {
                    labelText: qsTr("First-run setup command")
                    tip: qsTr("One-time command run before the agent's first launch (e.g. generating a token). Runs only once; a successful run is remembered. Leave empty for no setup.")
                }
                FormTextField {
                    id: setupField
                    text: page.agentData.setupCommand || ""
                    placeholderText: qsTr("optional")
                }

                FormLabel {
                    labelText: qsTr("Token file")
                    tip: qsTr("Path to a bearer-token file (%VAR% and ~ expanded). Its content is passed to the agent on launch and appended to the Web URL as #token=... when opening the browser.")
                }
                FormTextField {
                    id: tokenFileField
                    text: page.agentData.tokenFile || ""
                    placeholderText: qsTr("optional")
                }

                Row {
                    spacing: 12
                    Layout.topMargin: 16
                    Layout.alignment: Qt.AlignRight

                    Button {
                        text: qsTr("Cancel")
                        background: Rectangle { radius: 8; color: parent.down ? "#45475a" : "#313244"; border.color: "#45475a" }
                        contentItem: Label { text: parent.text; color: "#cdd6f4"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        onClicked: page.StackView.view.pop()
                    }
                    Button {
                        id: saveButton
                        readonly property color accent: page.agentData.color || "#89b4fa"
                        enabled: page.formValid
                        text: qsTr("Save")
                        background: Rectangle {
                            radius: 8
                            color: saveButton.enabled
                                   ? (saveButton.down ? Qt.darker(saveButton.accent, 1.3) : saveButton.accent)
                                   : "#313244"
                            border.color: "#45475a"
                        }
                        contentItem: Label { text: parent.text; color: "#ffffff"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        onClicked: page.save()
                    }
                }
            }
        }
    }

    // Shown when addAgent/updateAgentFull could not write agents.json.
    Popup {
        id: saveErrorPopup
        anchors.centerIn: parent
        modal: true
        focus: true
        width: 420
        padding: 20
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: "#1e1e2e"
            border.color: "#f38ba8"
            border.width: 1
            radius: 12
        }

        ColumnLayout {
            width: saveErrorPopup.availableWidth
            spacing: 12

            Label {
                text: qsTr("Save failed")
                color: "#f38ba8"
                font.pixelSize: 16
                font.bold: true
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Could not write the configuration file:")
                color: "#cdd6f4"
                font.pixelSize: 13
                wrapMode: Text.Wrap
            }
            Label {
                Layout.fillWidth: true
                text: launcher.configFilePath()
                color: "#89b4fa"
                font.pixelSize: 12
                font.family: "Consolas, Monaco, monospace"
                wrapMode: Text.WrapAnywhere
            }
            Button {
                Layout.alignment: Qt.AlignRight
                text: qsTr("OK")
                background: Rectangle { radius: 8; color: parent.down ? "#45475a" : "#313244"; border.color: "#45475a" }
                contentItem: Label { text: parent.text; color: "#cdd6f4"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                onClicked: saveErrorPopup.close()
            }
        }
    }
}
```

- [ ] **Step 2: Register the file in CMakeLists.txt**

In the `qt_add_resources(AgentLauncher "app_resources"` `FILES` list, after the existing `qml/` entries (`qml/main.qml`, `qml/AgentCard.qml`, `qml/ConfigPage.qml`), add:

```cmake
        qml/AgentEditPage.qml
```

In the `qt6_create_translation(...)` argument list, after `qml/ConfigPage.qml` add:

```cmake
    qml/AgentEditPage.qml
```

- [ ] **Step 3: Build and verify**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: build succeeds, tests still pass. The page is not reachable yet (wired in Task 6).

---

### Task 5: SettingsPage.qml — launcher list, delete, restore

**Files:**
- Create: `qml/SettingsPage.qml`
- Modify: `CMakeLists.txt` (resources + translation lists)

**Interfaces:**
- Consumes: `agentModel` (roles `agentId`, `name`, `command`, `icon`, `running`), `launcher` (`removeAgent`, `restoreDefaults`, `isDefaultAgent`, `configFilePath`), `AgentEditPage.qml` (Task 4).
- Produces: `qml/SettingsPage.qml` — a `Page` that pops itself and pushes `AgentEditPage` for add/edit.

- [ ] **Step 1: Create `qml/SettingsPage.qml`**

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Settings entry: launcher management. Structured as sections inside a
// scrollable column so more settings can be added below "Launchers" later.
Page {
    id: page
    background: Rectangle { color: "#1e1e2e" }

    function openEditor(agentId) {
        page.StackView.view.push(agentEditComp, { "agentId": agentId })
    }

    Component {
        id: agentEditComp
        AgentEditPage {}
    }

    // --- Delete confirmation -----------------------------------------------
    Popup {
        id: deleteConfirmPopup
        property string pendingId: ""
        property string pendingName: ""
        property bool pendingRunning: false
        property bool pendingBuiltin: false
        anchors.centerIn: parent
        modal: true
        focus: true
        width: 420
        padding: 20
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: "#1e1e2e"
            border.color: "#f38ba8"
            border.width: 1
            radius: 12
        }

        ColumnLayout {
            width: deleteConfirmPopup.availableWidth
            spacing: 12

            Label {
                text: qsTr("Delete Launcher")
                color: "#f38ba8"
                font.pixelSize: 16
                font.bold: true
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Remove \"%1\" from the launcher list?")
                      .arg(deleteConfirmPopup.pendingName)
                color: "#cdd6f4"
                font.pixelSize: 13
                wrapMode: Text.Wrap
            }
            Label {
                Layout.fillWidth: true
                visible: deleteConfirmPopup.pendingRunning
                text: qsTr("The agent is currently running. Deleting it does not stop the process; stop it via its own command if needed.")
                color: "#f9e2af"
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }
            Label {
                Layout.fillWidth: true
                visible: deleteConfirmPopup.pendingBuiltin
                text: qsTr("This is a built-in launcher. You can bring it back later with \"Restore default launchers\".")
                color: "#7f849c"
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Button {
                    Layout.fillWidth: true
                    text: qsTr("Delete")
                    background: Rectangle { radius: 8; color: parent.down ? Qt.darker("#f38ba8", 1.3) : (parent.hovered ? Qt.darker("#f38ba8", 1.15) : "#f38ba8") }
                    contentItem: Label { text: parent.text; color: "#1e1e2e"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    onClicked: {
                        if (!launcher.removeAgent(deleteConfirmPopup.pendingId))
                            errorPopup.open()
                        deleteConfirmPopup.close()
                    }
                }
                Button {
                    Layout.fillWidth: true
                    text: qsTr("Cancel")
                    background: Rectangle { radius: 8; color: parent.down ? "#45475a" : (parent.hovered ? "#4a4d62" : "#313244"); border.color: "#45475a" }
                    contentItem: Label { text: parent.text; color: "#cdd6f4"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    onClicked: deleteConfirmPopup.close()
                }
            }
        }
    }

    // Shown when removeAgent/restoreDefaults could not write agents.json.
    Popup {
        id: errorPopup
        anchors.centerIn: parent
        modal: true
        focus: true
        width: 420
        padding: 20
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: "#1e1e2e"
            border.color: "#f38ba8"
            border.width: 1
            radius: 12
        }

        ColumnLayout {
            width: errorPopup.availableWidth
            spacing: 12

            Label {
                text: qsTr("Save failed")
                color: "#f38ba8"
                font.pixelSize: 16
                font.bold: true
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Could not write the configuration file:")
                color: "#cdd6f4"
                font.pixelSize: 13
                wrapMode: Text.Wrap
            }
            Label {
                Layout.fillWidth: true
                text: launcher.configFilePath()
                color: "#89b4fa"
                font.pixelSize: 12
                font.family: "Consolas, Monaco, monospace"
                wrapMode: Text.WrapAnywhere
            }
            Button {
                Layout.alignment: Qt.AlignRight
                text: qsTr("OK")
                background: Rectangle { radius: 8; color: parent.down ? "#45475a" : "#313244"; border.color: "#45475a" }
                contentItem: Label { text: parent.text; color: "#cdd6f4"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                onClicked: errorPopup.close()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 12

        RowLayout {
            spacing: 12

            Button {
                text: qsTr("\u2190 Back")
                background: Rectangle { color: "transparent" }
                contentItem: Label { text: parent.text; color: "#89b4fa"; font.pixelSize: 16 }
                onClicked: page.StackView.view.pop()
            }
            Item { Layout.fillWidth: true }
        }

        Label {
            text: qsTr("Settings")
            color: "#cdd6f4"
            font.pixelSize: 24
            font.bold: true
        }

        // --- Launchers section -------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Label {
                text: qsTr("Launchers")
                color: "#a6adc8"
                font.pixelSize: 16
                font.bold: true
            }
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("Add Launcher")
                background: Rectangle { radius: 8; color: parent.down ? Qt.darker("#89b4fa", 1.3) : (parent.hovered ? Qt.darker("#89b4fa", 1.15) : "#89b4fa") }
                contentItem: Label { text: parent.text; color: "#1e1e2e"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                onClicked: page.openEditor("")
            }
        }

        ScrollView {
            id: scrollView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: scrollView.availableWidth
                spacing: 8

                Repeater {
                    model: agentModel

                    delegate: Rectangle {
                        Layout.fillWidth: true
                        height: 60
                        radius: 10
                        color: "#313244"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 10
                            spacing: 12

                            Image {
                                source: model.icon
                                sourceSize: Qt.size(28, 28)
                                fillMode: Image.PreserveAspectFit
                            }

                            ColumnLayout {
                                spacing: 2
                                Layout.fillWidth: true

                                Label {
                                    text: model.name
                                    color: "#cdd6f4"
                                    font.pixelSize: 14
                                    font.bold: true
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: model.command
                                    color: "#7f849c"
                                    font.pixelSize: 11
                                    elide: Text.ElideMiddle
                                }
                            }

                            // Running-state dot with tooltip.
                            Rectangle {
                                width: 10
                                height: 10
                                radius: 5
                                color: model.running ? "#a6e3a1" : "#585b70"
                                ToolTip.visible: dotArea.containsMouse
                                ToolTip.delay: 300
                                ToolTip.text: model.running ? qsTr("Running") : qsTr("Stopped")

                                MouseArea {
                                    id: dotArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                }
                            }

                            Button {
                                text: qsTr("Edit")
                                background: Rectangle { radius: 8; color: parent.down ? "#45475a" : (parent.hovered ? "#4a4d62" : "#313244"); border.color: "#45475a" }
                                contentItem: Label { text: parent.text; color: "#89b4fa"; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                onClicked: page.openEditor(model.agentId)
                            }
                            Button {
                                text: qsTr("Delete")
                                background: Rectangle { radius: 8; color: parent.down ? "#45475a" : (parent.hovered ? "#4a4d62" : "#313244"); border.color: "#45475a" }
                                contentItem: Label { text: parent.text; color: "#f38ba8"; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                onClicked: {
                                    deleteConfirmPopup.pendingId = model.agentId
                                    deleteConfirmPopup.pendingName = model.name
                                    deleteConfirmPopup.pendingRunning = model.running
                                    deleteConfirmPopup.pendingBuiltin =
                                        launcher.isDefaultAgent(model.agentId)
                                    deleteConfirmPopup.open()
                                }
                            }
                        }
                    }
                }
            }
        }

        Button {
            text: qsTr("Restore default launchers")
            background: Rectangle { color: "transparent" }
            contentItem: Label { text: parent.text; color: "#7f849c"; font.pixelSize: 12 }
            onClicked: {
                if (!launcher.restoreDefaults())
                    errorPopup.open()
            }
        }
    }
}
```

- [ ] **Step 2: Register the file in CMakeLists.txt**

In the `qt_add_resources(AgentLauncher "app_resources"` `FILES` list, after `qml/AgentEditPage.qml` (added in Task 4), add:

```cmake
        qml/SettingsPage.qml
```

In the `qt6_create_translation(...)` argument list, after `qml/AgentEditPage.qml` add:

```cmake
    qml/SettingsPage.qml
```

- [ ] **Step 3: Build and verify**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: build succeeds, tests still pass. Page not reachable yet (Task 6).

---

### Task 6: main.qml integration — settings button, card entry, remove ConfigPage

**Files:**
- Create: `icons/gear.svg`
- Modify: `qml/main.qml`, `src/AgentLauncher.h`, `src/AgentLauncher.cpp`, `CMakeLists.txt`
- Delete: `qml/ConfigPage.qml`

**Interfaces:**
- Consumes: `SettingsPage.qml` (Task 5), `AgentEditPage.qml` (Task 4).
- Produces: the user-facing entry points — floating settings button (bottom-right), card "Configure" opens the full editor.

- [ ] **Step 1: Create `icons/gear.svg`**

```svg
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24"
     fill="none" stroke="#cdd6f4" stroke-width="2" stroke-linecap="round"
     stroke-linejoin="round">
  <circle cx="12" cy="12" r="3"/>
  <path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z"/>
</svg>
```

- [ ] **Step 2: Update `qml/main.qml`**

(a) In the home page's card delegate, change the configure handler:

```qml
                        Repeater {
                            model: agentModel
                            delegate: AgentCard {
                                width: 260
                                onConfigureRequested: function(id) {
                                    stack.push(agentEditPageComp, { "agentId": id })
                                }
                            }
                        }
```

(b) Replace the `configPageComp` Component block with:

```qml
    Component {
        id: settingsPageComp
        SettingsPage {}
    }

    Component {
        id: agentEditPageComp
        AgentEditPage {}
    }
```

(c) Add the floating settings button **after** the `StackView` block (later siblings render on top):

```qml
    // Floating settings entry in the bottom-right corner.
    Button {
        id: settingsButton
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 20
        width: 44
        height: 44

        ToolTip.visible: hovered
        ToolTip.delay: 300
        ToolTip.text: qsTr("Settings")

        background: Rectangle {
            radius: 12
            color: parent.down ? "#45475a" : (parent.hovered ? "#4a4d62" : "#313244")
            border.color: "#45475a"
        }
        contentItem: Image {
            source: "qrc:/icons/gear.svg"
            sourceSize: Qt.size(22, 22)
            fillMode: Image.PreserveAspectFit
        }
        onClicked: stack.push(settingsPageComp)
    }
```

- [ ] **Step 3: Delete ConfigPage.qml, the old updateAgent invokable, and update CMakeLists.txt**

```bash
rm qml/ConfigPage.qml
```

In `src/AgentLauncher.h`, delete the declaration:

```cpp
    Q_INVOKABLE bool updateAgent(const QString &id, const QString &command, const QString &webUrl, const QString &setupCommand);
```

In `src/AgentLauncher.cpp`, delete the whole old `AgentLauncher::updateAgent(const QString &id, const QString &command, const QString &webUrl, const QString &setupCommand)` function body (near line 435).

In `CMakeLists.txt`:
- Remove `qml/ConfigPage.qml` from the `qt_add_resources` `FILES` list; add `icons/gear.svg`.
- Remove `qml/ConfigPage.qml` from the `qt6_create_translation` source list.

- [ ] **Step 4: Build and run the full manual walkthrough**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/AgentLauncher.exe &
```

Verify each item (this is the acceptance pass for Tasks 4–6 together):

1. Gear button visible bottom-right on the home page, hover shows "Settings" tooltip, click opens the Settings page.
2. Settings lists all agents with icon, name, command, running dot; Edit opens the edit form pre-filled.
3. Tooltips appear when hovering every field label (and its `ℹ` icon); required fields (Name, Command, Web URL) show a red `*`.
4. Add a launcher with only required fields: card appears on the home page with auto id (visible in the edit page later), auto palette color, default icon.
5. Add a second launcher with the same name: id gets a `-2` suffix.
6. Enter an invalid Web URL (`abc`) → red border + hint; Save disabled. Fix it → Save enabled.
7. Enter an invalid color (`blue`) → red border; `#89b4fa` → swatch preview updates.
8. Icon quick-pick thumbnails fill the icon field on click; the live preview updates for `qrc:/` icons.
9. Edit a built-in launcher's name and Save → home page card updates immediately; restart the app → change persisted (`%LOCALAPPDATA%\AgentLauncher\agents.json`).
10. Delete a user-added launcher → row disappears, confirmation dialog shown.
11. Delete a built-in launcher → gone from list; **restart the app** → it does NOT come back; click "Restore default launchers" → it returns.
12. Card "Configure" (context menu / button as before) opens the full edit page for that agent (not the old 3-field page).

---

### Task 7: Chinese translations + AGENTS.md documentation

**Files:**
- Modify: `translations/agentlauncher_zh_CN.ts`, `AGENTS.md`

**Interfaces:**
- Consumes: all `qsTr()` strings introduced in Tasks 4–6.
- Produces: complete zh_CN translations; documented `"removed"` schema key.

- [ ] **Step 1: Sync and fill the .ts file**

Build once so `lupdate` adds the new contexts (`SettingsPage`, `AgentEditPage`, `main`), then fill every `type="unfinished"` `<translation>` for the new strings. Delete the obsolete `ConfigPage` context block. Translation table:

| Source | 中文 |
|---|---|
| Settings | 设置 |
| Launchers | 启动器 |
| Add Launcher | 添加启动器 |
| Edit Launcher | 编辑启动器 |
| Edit / Delete | 编辑 / 删除 |
| Delete Launcher | 删除启动器 |
| Remove "%1" from the launcher list? | 从启动器列表中移除"%1"？ |
| The agent is currently running. Deleting it does not stop the process; stop it via its own command if needed. | 该 Agent 正在运行。删除不会终止进程；如需停止请使用其自身的停止命令。 |
| This is a built-in launcher. You can bring it back later with "Restore default launchers". | 这是内置启动器。之后可通过"恢复默认启动器"找回。 |
| Restore default launchers | 恢复默认启动器 |
| Running / Stopped | 运行中 / 已停止 |
| This agent is running. Changes take effect on the next launch. | 该 Agent 正在运行，修改将在下次启动时生效。 |
| Changes are saved to the configuration file. | 修改将保存到配置文件。 |
| Basics / Appearance / Install & Maintenance / Advanced | 基本信息 / 外观 / 安装与维护 / 高级 |
| Name | 名称 |
| Display name shown on the launcher card, e.g. "Kimi Code". | 显示在启动器卡片上的名称，例如 "Kimi Code"。 |
| e.g. Kimi Code | 例如 Kimi Code |
| Command | 启动命令 |
| Command line that starts the agent, e.g. "kimi web --port 58628". It runs in the background without a visible window. | 用于启动 Agent 的命令行，例如 "kimi web --port 58628"。将在后台运行，无可见窗口。 |
| e.g. opencode web --port 4096 | 例如 opencode web --port 4096 |
| Web URL | Web 地址 |
| The agent's web UI address. Used as a health check to detect whether the agent is running, and opened in the browser. Keep the port in sync with the command. | Agent 的 Web 界面地址。用于健康检查（检测 Agent 是否在运行），并在启动后于浏览器中打开。请保持端口与启动命令一致。 |
| e.g. http://127.0.0.1:4096 | 例如 http://127.0.0.1:4096 |
| Must be a valid http:// or https:// URL. | 必须是合法的 http:// 或 https:// 地址。 |
| ID | ID |
| Unique identifier stored in the configuration file. Leave empty to generate it from the name. It cannot be changed after creation. | 存储在配置文件中的唯一标识。留空则根据名称自动生成，创建后不可修改。 |
| auto-generated from name | 留空则由名称自动生成 |
| Config directory | 配置目录 |
| The agent's own configuration folder, e.g. "%USERPROFILE%/.kimi-code". Opened from the card's context menu. %VAR% and ~ are expanded. | Agent 自身的配置文件夹，例如 "%USERPROFILE%/.kimi-code"。可从卡片右键菜单打开。支持 %VAR% 与 ~ 展开。 |
| e.g. %USERPROFILE%/.config/opencode | 例如 %USERPROFILE%/.config/opencode |
| Icon | 图标 |
| Built-in icon (qrc:/icons/<name>.svg), a local file path (%VAR% and ~ expanded), or an http(s):// URL. Leave empty for the default icon. Click a built-in icon below to fill the field. | 内置图标（qrc:/icons/<名称>.svg）、本地文件路径（支持 %VAR% 与 ~ 展开）或 http(s):// URL。留空使用默认图标。点击下方内置图标可快速填入。 |
| qrc:/icons/<name>.svg, file path or URL | qrc:/icons/<名称>.svg、文件路径或 URL |
| Color | 主题色 |
| Accent color of the card in #RRGGBB form, e.g. "#89b4fa". Leave empty to auto-assign a color from the built-in palette. | 卡片的强调色，#RRGGBB 格式，例如 "#89b4fa"。留空则自动分配内置调色板颜色。 |
| auto-assigned | 留空自动分配 |
| Card color | 卡片背景色 |
| Background color of the card in #RRGGBB form while the agent is not running. Leave empty for the default (#313244). | Agent 未运行时卡片的背景色，#RRGGBB 格式。留空使用默认值（#313244）。 |
| default (#313244) | 默认（#313244） |
| Install command | 安装命令 |
| Command that installs the agent, e.g. "npm install -g @kimi-code/cli". Offered on the card when the agent is not installed. | 安装该 Agent 的命令，例如 "npm install -g @kimi-code/cli"。Agent 未安装时显示在卡片上。 |
| e.g. npm install -g opencode-ai | 例如 npm install -g opencode-ai |
| Update command | 更新命令 |
| Command that updates the agent to the latest version, e.g. "npm update -g @kimi-code/cli". Run from the card's context menu. | 将 Agent 更新到最新版本的命令，例如 "npm update -g @kimi-code/cli"。可从卡片右键菜单执行。 |
| e.g. npm update -g opencode-ai | 例如 npm update -g opencode-ai |
| Version command | 版本命令 |
| Command that prints the agent's version, e.g. "kimi --version". Run silently at startup to detect whether the agent is installed. | 输出 Agent 版本号的命令，例如 "kimi --version"。启动时静默执行，用于检测 Agent 是否已安装。 |
| e.g. opencode --version | 例如 opencode --version |
| First-run setup command | 首次运行准备命令 |
| One-time command run before the agent's first launch (e.g. generating a token). Runs only once; a successful run is remembered. Leave empty for no setup. | 首次启动前执行的一次性命令（例如生成令牌）。仅执行一次，成功后会被记录。留空表示无需准备。 |
| Token file | 令牌文件 |
| Path to a bearer-token file (%VAR% and ~ expanded). Its content is passed to the agent on launch and appended to the Web URL as #token=... when opening the browser. | Bearer 令牌文件路径（支持 %VAR% 与 ~ 展开）。内容会在启动时传给 Agent，并在打开浏览器时以 #token=... 追加到 Web 地址。 |
| optional | 选填 |
| Cancel / Save / OK | 取消 / 保存 / 确定 |
| ← Back | ← 返回 |
| Save failed | 保存失败 |
| Could not write the configuration file: | 无法写入配置文件： |

- [ ] **Step 2: Document the schema change in AGENTS.md**

In `AGENTS.md`, in the "Config schema (agents.json)" section, after the paragraph about the `title` field, add:

```markdown
An optional root `removed` array lists ids of built-in agents the user
deleted in the Settings page. `AgentConfig::migrate()` skips these ids when
merging defaults, so deleted built-ins stay deleted across restarts. The
"Restore default launchers" button in Settings clears this list and
re-appends the missing defaults.

Agents can also be added, edited, and deleted from the Settings page
(bottom-right gear button); the UI writes the same `agents.json` via
`AgentLauncher::addAgent()` / `updateAgentFull()` / `removeAgent()`.
```

- [ ] **Step 3: Final verification**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/AgentLauncher.exe &
```

Expected: on a zh-CN system the new UI is fully in Chinese (Settings page, form labels, tooltips, dialogs, error popups); tests pass; the Task 6 walkthrough still passes.

- [ ] **Step 4: Checkpoint with the user**

Per AGENTS.md no commit happens without an explicit request. Report completion and ask the user whether to commit (suggest a single `feat: settings page with launcher management` commit or logical per-task commits).
