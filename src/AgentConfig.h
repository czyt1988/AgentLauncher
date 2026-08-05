#ifndef AGENTCONFIG_H
#define AGENTCONFIG_H

#include <QList>
#include <QString>

#include <algorithm>

struct Agent
{
    QString id;
    QString name;
    QString command;
    QString webUrl;
    QString configDir;
    QString icon;
    QString color;
    QString cardColor; // optional card background color (non-running state)
    QString installCommand;
    QString updateCommand;
    QString versionCommand;
    QString setupCommand; // one-time setup command run before first launch
    QString tokenFile;    // path to a bearer token file (env-expanded); set as
                          // QWEN_SERVER_TOKEN on launch and appended as
                          // #token=<value> to the web URL when opening the browser
    bool running = false;
    bool launching = false; // transient UI state, never persisted
    bool installed = false;       // runtime, detected via versionCommand
    QString version;              // runtime, parsed from versionCommand output
    bool installing = false;      // transient UI state, never persisted
    bool setupDone = false;       // runtime state, persisted in agent_state.json
    bool setupping = false;       // transient UI state, never persisted
    bool checkingVersion = false; // transient UI state: version check in progress
};

class AgentConfig
{
public:
    void load();
    bool save();

    QList<Agent> agents() const { return m_agents; }
    void setAgents(const QList<Agent> &agents) { m_agents = agents; }

    QString title() const { return m_title; }
    void setTitle(const QString &title) { m_title = title; }

    void updateAgent(const QString &id, const QString &command, const QString &webUrl);

    static QString configFilePath();

private:
    QList<Agent> m_agents;
    QString m_title;

    QList<Agent> parse(const QByteArray &data, QString &outTitle) const;

    // Resolve an icon string to a usable image URL:
    //   empty            → qrc:/icons/default.svg
    //   qrc:/...         → as-is
    //   http(s)://...    → as-is
    //   local file path  → expand env vars, check existence, convert to
    //                      file:/// URL; fall back to default if not found
    static QString resolveIcon(const QString &raw);

    // Expand %VAR% environment variables and ~ in a path.
    static QString expandEnv(const QString &path);

    // Fill in empty fields from the bundled default config and add missing
    // agents. Called after load() so on-disk configs created from older
    // defaults (missing installCommand/updateCommand/versionCommand) get
    // the new fields populated automatically.
    void migrate(const QList<Agent> &defaults, const QString &defaultTitle);

    // Assign a color from the built-in palette to every agent whose `color`
    // is still empty after migration. Colors are assigned by cycling through
    // the palette based on the agent's position in the list. Returns true if
    // any color was assigned (so the caller can persist).
    bool assignPaletteColors();
};

#endif // AGENTCONFIG_H
