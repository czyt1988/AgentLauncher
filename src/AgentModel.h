#ifndef AGENTMODEL_H
#define AGENTMODEL_H

#include "AgentConfig.h"

#include <QAbstractListModel>

class AgentModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        CommandRole,
        WebUrlRole,
        ConfigDirRole,
        IconRole,
        ColorRole,
        RunningRole,
        LaunchingRole,
        InstallCommandRole,
        UpdateCommandRole,
        VersionCommandRole,
        SetupCommandRole,
        InstalledRole,
        VersionRole,
        InstallingRole,
        SetupDoneRole,
        SetuppingRole
    };
    Q_ENUM(Roles)

    explicit AgentModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setAgents(const QList<Agent> &agents);
    QList<Agent> &agents() { return m_agents; }

    Q_INVOKABLE int indexOf(const QString &id) const;
    Q_INVOKABLE QVariantMap agent(const QString &id) const;

public slots:
    void setRunning(const QString &id, bool running);
    void setLaunching(const QString &id, bool launching);
    void setInstalled(const QString &id, bool installed);
    void setVersion(const QString &id, const QString &version);
    void setInstalling(const QString &id, bool installing);
    void setSetupDone(const QString &id, bool done);
    void setSetupping(const QString &id, bool setupping);

private:
    QList<Agent> m_agents;
};

#endif // AGENTMODEL_H
