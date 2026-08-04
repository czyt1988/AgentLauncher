#ifndef AGENTLAUNCHER_H
#define AGENTLAUNCHER_H

#include "AgentModel.h"

#include <QHash>
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
    Q_INVOKABLE bool stop(const QString &id);
    Q_INVOKABLE void openWeb(const QString &id);
    Q_INVOKABLE void openConfigDir(const QString &id);
    Q_INVOKABLE bool updateAgent(const QString &id, const QString &command, const QString &webUrl);
    Q_INVOKABLE void install(const QString &id);
    Q_INVOKABLE void updateTool(const QString &id);

    // True if at least one agent was started from the launcher this session.
    Q_INVOKABLE bool hasLaunchedAgents() const;

    // Terminate every process this launcher started this session. Returns the
    // number of process trees successfully killed.
    Q_INVOKABLE int stopAll();

    // Start the background health-check polling.
    void start();

signals:
    // Emitted when a launch/stop attempt fails. The UI shows an at-place
    // flash on the matching card plus a detailed popup.
    void launchFailed(const QString &id, const QString &message);

    // Emitted when an install/update finishes (success or failure).
    void installFinished(const QString &id, bool success, const QString &message);

private slots:
    void checkAll();

private:
    AgentModel *m_model;
    QNetworkAccessManager *m_nam;
    QTimer *m_timer;

    // id -> PID of the most recent process this launcher started (in-memory,
    // current session only). Used by stop(); cleared if the launcher restarts.
    QHash<QString, qint64> m_pids;

    // id -> launch epoch, bumped on each successful launch. Used to invalidate
    // stale "launching" safety timeouts so a re-launch can't be cleared early
    // by a timer from a previous attempt.
    QHash<QString, int> m_launchEpoch;

    QString expandEnv(const QString &path) const;

    // Resolve a bare command (e.g. "qwen") to a full executable path,
    // applying PATHEXT on Windows so .cmd/.bat shims are found.
    static QString resolveProgram(const QString &program);

    // Run each agent's versionCommand silently on startup.
    void checkVersions();
    void checkVersion(const QString &id);

    // Extract a x.y.z version string from command output.
    static QString extractVersion(const QString &output);
};

#endif // AGENTLAUNCHER_H
