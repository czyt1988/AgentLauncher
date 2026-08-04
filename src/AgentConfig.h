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
    QString installCommand;
    QString updateCommand;
    QString versionCommand;
    QString setupCommand; // one-time setup command run before first launch
    bool running = false;
    bool launching = false; // transient UI state, never persisted
    bool installed = false;       // runtime, detected via versionCommand
    QString version;              // runtime, parsed from versionCommand output
    bool installing = false;      // transient UI state, never persisted
};

class AgentConfig
{
public:
    void load();
    bool save();

    QList<Agent> agents() const { return m_agents; }
    void setAgents(const QList<Agent> &agents) { m_agents = agents; }

    void updateAgent(const QString &id, const QString &command, const QString &webUrl);

    static QString configFilePath();

private:
    QList<Agent> m_agents;

    QList<Agent> parse(const QByteArray &data) const;

    // Fill in empty fields from the bundled default config and add missing
    // agents. Called after load() so on-disk configs created from older
    // defaults (missing installCommand/updateCommand/versionCommand) get
    // the new fields populated automatically.
    void migrate(const QList<Agent> &defaults);
};

#endif // AGENTCONFIG_H
