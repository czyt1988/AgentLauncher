#include "AgentModel.h"

#include <QVariantMap>

AgentModel::AgentModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int AgentModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_agents.size();
}

QVariant AgentModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_agents.size())
        return {};
    const Agent &a = m_agents.at(index.row());

    switch (role) {
    case IdRole:        return a.id;
    case NameRole:      return a.name;
    case CommandRole:   return a.command;
    case WebUrlRole:    return a.webUrl;
    case ConfigDirRole: return a.configDir;
    case IconRole:      return a.icon;
    case ColorRole:     return a.color;
    case CardColorRole: return a.cardColor;
    case RunningRole:   return a.running;
    case LaunchingRole: return a.launching;
    case InstallCommandRole: return a.installCommand;
    case UpdateCommandRole:  return a.updateCommand;
    case VersionCommandRole: return a.versionCommand;
    case SetupCommandRole:  return a.setupCommand;
    case InstalledRole:  return a.installed;
    case VersionRole:    return a.version;
    case InstallingRole: return a.installing;
    case SetupDoneRole:  return a.setupDone;
    case SetuppingRole:  return a.setupping;
    }
    return {};
}

QHash<int, QByteArray> AgentModel::roleNames() const
{
    return {
        { IdRole,        "agentId" },
        { NameRole,      "name" },
        { CommandRole,   "command" },
        { WebUrlRole,    "webUrl" },
        { ConfigDirRole, "configDir" },
        { IconRole,      "icon" },
        { ColorRole,     "color" },
        { CardColorRole, "cardColor" },
        { RunningRole,   "running" },
        { LaunchingRole, "launching" },
        { InstallCommandRole, "installCommand" },
        { UpdateCommandRole,  "updateCommand" },
        { VersionCommandRole, "versionCommand" },
        { SetupCommandRole,   "setupCommand" },
        { InstalledRole,  "installed" },
        { VersionRole,     "version" },
        { InstallingRole,  "installing" },
        { SetupDoneRole,   "setupDone" },
        { SetuppingRole,   "setupping" }
    };
}

void AgentModel::setAgents(const QList<Agent> &agents)
{
    beginResetModel();
    m_agents = agents;
    endResetModel();
}

int AgentModel::indexOf(const QString &id) const
{
    for (int i = 0; i < m_agents.size(); ++i) {
        if (m_agents.at(i).id == id)
            return i;
    }
    return -1;
}

QVariantMap AgentModel::agent(const QString &id) const
{
    QVariantMap m;
    for (const Agent &a : m_agents) {
        if (a.id == id) {
            m[QStringLiteral("id")] = a.id;
            m[QStringLiteral("name")] = a.name;
            m[QStringLiteral("command")] = a.command;
            m[QStringLiteral("webUrl")] = a.webUrl;
            m[QStringLiteral("configDir")] = a.configDir;
            m[QStringLiteral("icon")] = a.icon;
            m[QStringLiteral("color")] = a.color;
            m[QStringLiteral("cardColor")] = a.cardColor;
            m[QStringLiteral("running")] = a.running;
            m[QStringLiteral("launching")] = a.launching;
            m[QStringLiteral("installCommand")] = a.installCommand;
            m[QStringLiteral("updateCommand")] = a.updateCommand;
            m[QStringLiteral("versionCommand")] = a.versionCommand;
            m[QStringLiteral("setupCommand")] = a.setupCommand;
            m[QStringLiteral("installed")] = a.installed;
            m[QStringLiteral("version")] = a.version;
            m[QStringLiteral("installing")] = a.installing;
            m[QStringLiteral("setupDone")] = a.setupDone;
            m[QStringLiteral("setupping")] = a.setupping;
            break;
        }
    }
    return m;
}

void AgentModel::setRunning(const QString &id, bool running)
{
    const int row = indexOf(id);
    if (row < 0)
        return;
    if (m_agents[row].running == running)
        return;
    m_agents[row].running = running;
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, { RunningRole });
}

void AgentModel::setLaunching(const QString &id, bool launching)
{
    const int row = indexOf(id);
    if (row < 0)
        return;
    if (m_agents[row].launching == launching)
        return;
    m_agents[row].launching = launching;
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, { LaunchingRole });
}

void AgentModel::setInstalled(const QString &id, bool installed)
{
    const int row = indexOf(id);
    if (row < 0)
        return;
    if (m_agents[row].installed == installed)
        return;
    m_agents[row].installed = installed;
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, { InstalledRole });
}

void AgentModel::setVersion(const QString &id, const QString &version)
{
    const int row = indexOf(id);
    if (row < 0)
        return;
    if (m_agents[row].version == version)
        return;
    m_agents[row].version = version;
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, { VersionRole });
}

void AgentModel::setInstalling(const QString &id, bool installing)
{
    const int row = indexOf(id);
    if (row < 0)
        return;
    if (m_agents[row].installing == installing)
        return;
    m_agents[row].installing = installing;
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, { InstallingRole });
}

void AgentModel::setSetupDone(const QString &id, bool done)
{
    const int row = indexOf(id);
    if (row < 0)
        return;
    if (m_agents[row].setupDone == done)
        return;
    m_agents[row].setupDone = done;
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, { SetupDoneRole });
}

void AgentModel::setSetupping(const QString &id, bool setupping)
{
    const int row = indexOf(id);
    if (row < 0)
        return;
    if (m_agents[row].setupping == setupping)
        return;
    m_agents[row].setupping = setupping;
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, { SetuppingRole });
}
