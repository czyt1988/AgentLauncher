#ifndef AGENTLAUNCHER_H
#define AGENTLAUNCHER_H

#include "AgentModel.h"

#include <QObject>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class AgentLauncher : public QObject
{
    Q_OBJECT

public:
    explicit AgentLauncher(AgentModel *model, QObject *parent = nullptr);

    Q_INVOKABLE void launch(const QString &id);
    Q_INVOKABLE void openWeb(const QString &id);
    Q_INVOKABLE void openConfigDir(const QString &id);
    Q_INVOKABLE bool updateAgent(const QString &id, const QString &command, const QString &webUrl);

    // Start the background health-check polling.
    void start();

private slots:
    void checkAll();

private:
    AgentModel *m_model;
    QNetworkAccessManager *m_nam;
    QTimer *m_timer;

    QString expandEnv(const QString &path) const;
};

#endif // AGENTLAUNCHER_H
