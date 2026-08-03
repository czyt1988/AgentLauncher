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
        LaunchingRole
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

private:
    QList<Agent> m_agents;
};

#endif // AGENTMODEL_H
