#ifndef AGENTCONFIG_H
#define AGENTCONFIG_H

#include <QList>
#include <QString>

struct Agent
{
    QString id;
    QString name;
    QString command;
    QString webUrl;
    QString configDir;
    QString icon;
    QString color;
    bool running = false;
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
};

#endif // AGENTCONFIG_H
