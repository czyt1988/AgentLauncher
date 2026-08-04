#include "AgentLauncher.h"
#include "AgentConfig.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

AgentLauncher::AgentLauncher(AgentModel *model, QObject *parent)
    : QObject(parent)
    , m_model(model)
    , m_nam(new QNetworkAccessManager(this))
    , m_timer(new QTimer(this))
{
    connect(m_timer, &QTimer::timeout, this, &AgentLauncher::checkAll);
}

void AgentLauncher::start()
{
    loadSetupState();
    checkAll();
    checkVersions();
    m_timer->start(3000);
}

void AgentLauncher::launch(const QString &id)
{
    const int row = m_model->indexOf(id);
    if (row < 0)
        return;

    const Agent &a = m_model->agents().at(row);

    // If the agent has a one-time setup command that hasn't been run yet,
    // run it first; doLaunch() is called from the setup's finished handler
    // on success.
    if (!a.setupCommand.isEmpty() && !a.setupDone) {
        runSetup(id);
        return;
    }

    doLaunch(id);
}

void AgentLauncher::doLaunch(const QString &id)
{
    const int row = m_model->indexOf(id);
    if (row < 0)
        return;
    const QString command = m_model->agents().at(row).command;

    // Split command into program + arguments on whitespace.
    const QStringList parts = QProcess::splitCommand(command);
    if (parts.isEmpty()) {
        emit launchFailed(id, tr("Startup command is empty."));
        return;
    }

    const QString program = parts.first();
    const QStringList args = parts.mid(1);

    // Resolve the bare program through PATH (PATHEXT on Windows) so npm-style
    // .cmd/.bat shims (e.g. "qwen" -> "qwen.cmd") are found. CreateProcess on
    // its own does not try those extensions, which is why "qwen serve" failed
    // silently before.
    const QString resolved = resolveProgram(program);
    if (resolved.isEmpty()) {
        const QString msg = tr("Cannot find '%1' on your PATH. "
                               "Make sure it is installed and on PATH.")
                                .arg(program);
        qWarning() << "AgentLauncher: launch failed for" << id << "-" << msg;
        emit launchFailed(id, msg);
        return;
    }

    // startDetached so the agent keeps running after this launcher closes.
    QProcess proc;
#ifdef Q_OS_WIN
    // A .cmd/.bat shim cannot be executed directly by CreateProcess; run it
    // through cmd.exe so the batch is interpreted (and a /T kill later covers
    // the whole cmd -> qwen.cmd -> node tree).
    if (resolved.endsWith(QStringLiteral(".cmd"), Qt::CaseInsensitive)
        || resolved.endsWith(QStringLiteral(".bat"), Qt::CaseInsensitive)) {
        proc.setProgram(QStringLiteral("cmd"));
        QStringList cmdArgs;
        cmdArgs << QStringLiteral("/c") << resolved << args;
        proc.setArguments(cmdArgs);
    } else
#endif
    {
        proc.setProgram(resolved);
        proc.setArguments(args);
    }
    proc.setWorkingDirectory(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));

    qint64 pid = 0;
    const bool ok = proc.startDetached(&pid);
    if (!ok) {
        const QString msg = tr("Failed to start '%1'.").arg(program);
        qWarning() << "AgentLauncher: startDetached failed for" << id << "-"
                   << msg;
        emit launchFailed(id, msg);
        return;
    }

    m_pids.insert(id, pid);

    // Mark the card as "launching" so the action button shows a spinner until
    // the health check confirms the server is up — or a 30s safety timeout
    // fires in case the agent crashes on boot. The epoch guards against a
    // stale timeout from an earlier attempt clearing a newer launch.
    const int epoch = ++m_launchEpoch[id];
    m_model->setLaunching(id, true);
    QTimer::singleShot(30000, this, [this, id, epoch]() {
        if (m_launchEpoch.value(id) == epoch)
            m_model->setLaunching(id, false);
    });

    // Re-check shortly so the card flips to running fast.
    QTimer::singleShot(1500, this, &AgentLauncher::checkAll);
}

bool AgentLauncher::stop(const QString &id)
{
    const auto it = m_pids.constFind(id);
    if (it == m_pids.constEnd() || *it == 0) {
        const QString msg = tr("This agent wasn't started from the launcher; "
                               "stop it with its own command.");
        qWarning() << "AgentLauncher: cannot stop" << id << "-" << msg;
        emit launchFailed(id, msg);
        return false;
    }

    const qint64 pid = *it;
    m_pids.erase(it);

    bool ok = false;
#ifdef Q_OS_WIN
    // /F force, /T kills the whole process tree (cmd -> qwen.cmd -> node).
    ok = QProcess::startDetached(
        QStringLiteral("taskkill"),
        { QStringLiteral("/F"), QStringLiteral("/T"),
          QStringLiteral("/PID"), QString::number(pid) });
#else
    ok = QProcess::startDetached(
        QStringLiteral("kill"),
        { QStringLiteral("-9"), QString::number(pid) });
#endif
    if (!ok) {
        const QString msg = tr("Failed to stop process (PID %1).").arg(pid);
        qWarning() << "AgentLauncher:" << msg;
        emit launchFailed(id, msg);
    }

    // Re-check so the card flips back to Stopped once the port is down.
    QTimer::singleShot(500, this, &AgentLauncher::checkAll);
    return ok;
}

bool AgentLauncher::hasLaunchedAgents() const
{
    return !m_pids.isEmpty();
}

int AgentLauncher::stopAll()
{
    int killed = 0;
    for (auto it = m_pids.constBegin(); it != m_pids.constEnd(); ++it) {
        const qint64 pid = *it;
        if (pid == 0)
            continue;
#ifdef Q_OS_WIN
        const bool ok = QProcess::startDetached(
            QStringLiteral("taskkill"),
            { QStringLiteral("/F"), QStringLiteral("/T"),
              QStringLiteral("/PID"), QString::number(pid) });
#else
        const bool ok = QProcess::startDetached(
            QStringLiteral("kill"),
            { QStringLiteral("-9"), QString::number(pid) });
#endif
        if (ok)
            ++killed;
    }
    m_pids.clear();
    return killed;
}

void AgentLauncher::openWeb(const QString &id)
{
    const int row = m_model->indexOf(id);
    if (row < 0)
        return;
    const QString url = m_model->agents().at(row).webUrl;
    if (url.isEmpty())
        return;
    QDesktopServices::openUrl(QUrl(url));
}

void AgentLauncher::openConfigDir(const QString &id)
{
    const int row = m_model->indexOf(id);
    if (row < 0)
        return;
    QString dir = expandEnv(m_model->agents().at(row).configDir);
    if (dir.isEmpty())
        return;
    dir = QDir::fromNativeSeparators(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

bool AgentLauncher::updateAgent(const QString &id, const QString &command, const QString &webUrl, const QString &setupCommand)
{
    QList<Agent> &agents = m_model->agents();
    bool changed = false;
    for (Agent &a : agents) {
        if (a.id == id) {
            a.command = command;
            a.webUrl = webUrl;
            a.setupCommand = setupCommand;
            changed = true;
            break;
        }
    }
    if (!changed)
        return false;

    // Refresh the model view for this row.
    const int row = m_model->indexOf(id);
    if (row >= 0) {
        const QModelIndex idx = m_model->index(row, 0);
        emit m_model->dataChanged(idx, idx, { AgentModel::CommandRole, AgentModel::WebUrlRole, AgentModel::SetupCommandRole });
    }

    // Persist to disk.
    AgentConfig cfg;
    cfg.setAgents(agents);
    return cfg.save();
}

void AgentLauncher::checkAll()
{
    const QList<Agent> &agents = m_model->agents();
    for (const Agent &a : agents) {
        if (a.webUrl.isEmpty())
            continue;
        QNetworkReply *reply = m_nam->get(QNetworkRequest(QUrl(a.webUrl)));
        reply->setParent(this);
        const QString id = a.id;
        // Any HTTP response (even an error status) means the server is up.
        connect(reply, &QNetworkReply::finished, this, [this, id, reply]() {
            bool up = false;
            if (reply->error() == QNetworkReply::NoError) {
                up = true;
            } else {
                // Connection-refused/timeout => not running.
                // Got an HTTP error status (e.g. 401/404) => server is up.
                const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                up = (code > 0);
            }
            // Once the server is up, the launch is done: clear the spinner.
            if (up)
                m_model->setLaunching(id, false);
            m_model->setRunning(id, up);
            reply->deleteLater();
        });
    }
}

QString AgentLauncher::expandEnv(const QString &path) const
{
    QString result = path;
    // Expand %VAR% style variables (Windows), e.g. %USERPROFILE%.
    static const QRegularExpression re(QStringLiteral("%(\\w+)%"));
    QRegularExpressionMatchIterator it = re.globalMatch(result);
    QString out;
    int cursor = 0;
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        out += result.mid(cursor, m.capturedStart() - cursor);
        const QString var = m.captured(1);
        const QString val = qEnvironmentVariable(qUtf8Printable(var));
        out += val.isEmpty() ? m.captured(0) : val;
        cursor = m.capturedEnd();
    }
    out += result.mid(cursor);
    // Expand ~ to the home directory (unix style, convenience).
    out.replace(QStringLiteral("~/"),
                QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + QStringLiteral("/"));
    return out;
}

QString AgentLauncher::resolveProgram(const QString &program)
{
    if (program.isEmpty())
        return QString();
    // findExecutable searches PATH and, on Windows, appends the PATHEXT
    // extensions (.exe/.cmd/.bat/...), which is exactly what is needed to
    // resolve npm-style shims like "qwen" -> "qwen.cmd".
    return QStandardPaths::findExecutable(program);
}

// --- Version detection ---------------------------------------------------

void AgentLauncher::checkVersions()
{
    for (const Agent &a : m_model->agents())
        checkVersion(a.id);
}

void AgentLauncher::checkVersion(const QString &id)
{
    const int row = m_model->indexOf(id);
    if (row < 0)
        return;
    const QString cmd = m_model->agents().at(row).versionCommand;
    if (cmd.isEmpty())
        return;

    auto *proc = new QProcess(this);
    proc->setProgram(QStringLiteral("cmd"));
    proc->setArguments({QStringLiteral("/c"), cmd});
    // Default channel mode → CREATE_NO_WINDOW → no visible console.

    const QString capturedId = id;
    connect(proc, &QProcess::finished, this,
            [this, capturedId, proc](int exitCode, QProcess::ExitStatus) {
                if (exitCode == 0) {
                    const QString output =
                        QString::fromLocal8Bit(proc->readAllStandardOutput());
                    m_model->setInstalled(capturedId, true);
                    m_model->setVersion(capturedId, extractVersion(output));
                } else {
                    m_model->setInstalled(capturedId, false);
                    m_model->setVersion(capturedId, QString());
                }
                proc->deleteLater();
            });

    // Safety timeout: kill hung version commands after 10s.
    QTimer::singleShot(10000, proc, [proc]() {
        if (proc->state() != QProcess::NotRunning)
            proc->kill();
    });

    proc->start();
}

QString AgentLauncher::extractVersion(const QString &output)
{
    // Match x.y.z (optionally with a pre-release suffix), e.g. "1.2.3",
    // "v1.2.3-beta", "1.2.3.4".
    static const QRegularExpression re(QStringLiteral("(\\d+\\.\\d+\\.\\d+[\\w.-]*)"));
    const auto match = re.match(output);
    return match.hasMatch() ? match.captured(1) : QString();
}

// --- Setup (first-run prerequisite) ----------------------------------------

void AgentLauncher::runSetup(const QString &id)
{
    const int row = m_model->indexOf(id);
    if (row < 0)
        return;
    const QString cmd = m_model->agents().at(row).setupCommand;
    if (cmd.isEmpty())
        return;

    m_model->setSetupping(id, true);

    auto *proc = new QProcess(this);
    proc->setProgram(QStringLiteral("cmd"));
    proc->setArguments({QStringLiteral("/c"), cmd});
    // Default channel mode → CREATE_NO_WINDOW → no visible console.

    const QString capturedId = id;
    connect(proc, &QProcess::finished, this,
            [this, capturedId, proc](int exitCode, QProcess::ExitStatus) {
                m_model->setSetupping(capturedId, false);

                if (exitCode == 0) {
                    markSetupDone(capturedId);
                    // Proceed with the actual launch.
                    doLaunch(capturedId);
                } else {
                    const QString errOutput =
                        QString::fromLocal8Bit(proc->readAllStandardError());
                    const QString stdOutput =
                        QString::fromLocal8Bit(proc->readAllStandardOutput());
                    QString detail = errOutput.isEmpty() ? stdOutput : errOutput;
                    if (detail.isEmpty())
                        detail = tr("(no output)");
                    emit launchFailed(capturedId,
                        tr("Setup command failed (exit code %1):\n%2")
                            .arg(exitCode)
                            .arg(detail.trimmed()));
                }
                proc->deleteLater();
            });

    connect(proc, &QProcess::errorOccurred, this,
            [this, capturedId, proc](QProcess::ProcessError) {
                if (proc->state() == QProcess::NotRunning) {
                    m_model->setSetupping(capturedId, false);
                    emit launchFailed(capturedId,
                        tr("Failed to start setup command."));
                    proc->deleteLater();
                }
            });

    // Safety timeout: kill hung setup commands after 30s.
    QTimer::singleShot(30000, proc, [proc]() {
        if (proc->state() != QProcess::NotRunning)
            proc->kill();
    });

    proc->start();
}

// --- Setup state persistence -----------------------------------------------

QString AgentLauncher::stateFilePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return dir + QStringLiteral("/agent_state.json");
}

void AgentLauncher::loadSetupState()
{
    QFile file(stateFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    const QJsonObject root = doc.object();

    for (Agent &a : m_model->agents()) {
        const QJsonObject agentState = root.value(a.id).toObject();
        if (agentState.value(QStringLiteral("setupDone")).toBool())
            a.setupDone = true;
    }
}

void AgentLauncher::markSetupDone(const QString &id)
{
    m_model->setSetupDone(id, true);

    // Persist to agent_state.json.
    QFile file(stateFilePath());
    QJsonObject root;
    if (file.open(QIODevice::ReadOnly)) {
        root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
    }

    QJsonObject agentState = root.value(id).toObject();
    agentState[QStringLiteral("setupDone")] = true;
    root[id] = agentState;

    QDir().mkpath(QFileInfo(stateFilePath()).absolutePath());
    if (file.open(QIODevice::WriteOnly))
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void AgentLauncher::resetSetup(const QString &id)
{
    m_model->setSetupDone(id, false);

    // Remove from agent_state.json.
    QFile file(stateFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    file.close();
    root.remove(id);

    if (file.open(QIODevice::WriteOnly))
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

// --- Install / Update -----------------------------------------------------

void AgentLauncher::install(const QString &id)
{
    const int row = m_model->indexOf(id);
    if (row < 0)
        return;
    const Agent &a = m_model->agents().at(row);

    if (a.running) {
        emit launchFailed(id, tr("请先关闭 %1 后再进行安装/更新。").arg(a.name));
        return;
    }
    if (a.installCommand.isEmpty()) {
        emit installFinished(id, false,
            tr("No install command configured for %1.").arg(a.name));
        return;
    }

    m_model->setInstalling(id, true);

    auto *proc = new QProcess(this);
    proc->setProgram(QStringLiteral("cmd"));
    proc->setArguments({QStringLiteral("/c"), a.installCommand});
    // Default channel mode → CREATE_NO_WINDOW → no visible console; output is
    // captured so we can report success/failure in installFinished.

    const QString capturedId = id;
    QPointer<QProcess> guard(proc);
    bool *handled = new bool(false);

    connect(proc, &QProcess::finished, this,
            [this, capturedId, guard, handled](int exitCode, QProcess::ExitStatus) {
                if (*handled)
                    return;
                *handled = true;
                m_model->setInstalling(capturedId, false);

                // Read output before deleteLater (reads become no-ops after).
                const QString output = guard
                    ? QString::fromLocal8Bit(guard->readAllStandardOutput()) : QString();
                const QString errOutput = guard
                    ? QString::fromLocal8Bit(guard->readAllStandardError()) : QString();

                if (guard)
                    guard->deleteLater();
                delete handled;

                if (exitCode == 0) {
                    emit installFinished(capturedId, true, QString());
                } else {
                    QString detail = errOutput.isEmpty() ? output : errOutput;
                    if (detail.isEmpty())
                        detail = tr("(no output)");
                    emit installFinished(capturedId, false,
                        tr("Install failed (exit code %1):\n%2")
                            .arg(exitCode).arg(detail.trimmed()));
                }
                // Re-check version to refresh the card.
                checkVersion(capturedId);
            });

    connect(proc, &QProcess::errorOccurred, this,
            [this, capturedId, guard, handled](QProcess::ProcessError) {
                // Only act if the process never started; otherwise finished
                // will handle cleanup.
                if (*handled)
                    return;
                if (guard && guard->state() == QProcess::NotRunning) {
                    *handled = true;
                    m_model->setInstalling(capturedId, false);
                    guard->deleteLater();
                    delete handled;
                    emit installFinished(capturedId, false,
                        tr("Failed to start install command."));
                }
            });

    proc->start();
}

void AgentLauncher::updateTool(const QString &id)
{
    const int row = m_model->indexOf(id);
    if (row < 0)
        return;
    const Agent &a = m_model->agents().at(row);

    if (a.running) {
        emit launchFailed(id, tr("请先关闭 %1 后再进行安装/更新。").arg(a.name));
        return;
    }
    if (a.updateCommand.isEmpty()) {
        emit installFinished(id, false,
            tr("No update command configured for %1.").arg(a.name));
        return;
    }

    m_model->setInstalling(id, true);

    auto *proc = new QProcess(this);
    proc->setProgram(QStringLiteral("cmd"));
    proc->setArguments({QStringLiteral("/c"), a.updateCommand});
    // Default channel mode → CREATE_NO_WINDOW → no visible console; output is
    // captured so we can report success/failure in installFinished.

    const QString capturedId = id;
    QPointer<QProcess> guard(proc);
    bool *handled = new bool(false);

    connect(proc, &QProcess::finished, this,
            [this, capturedId, guard, handled](int exitCode, QProcess::ExitStatus) {
                if (*handled)
                    return;
                *handled = true;
                m_model->setInstalling(capturedId, false);

                const QString output = guard
                    ? QString::fromLocal8Bit(guard->readAllStandardOutput()) : QString();
                const QString errOutput = guard
                    ? QString::fromLocal8Bit(guard->readAllStandardError()) : QString();

                if (guard)
                    guard->deleteLater();
                delete handled;

                if (exitCode == 0) {
                    emit installFinished(capturedId, true, QString());
                } else {
                    QString detail = errOutput.isEmpty() ? output : errOutput;
                    if (detail.isEmpty())
                        detail = tr("(no output)");
                    emit installFinished(capturedId, false,
                        tr("Update failed (exit code %1):\n%2")
                            .arg(exitCode).arg(detail.trimmed()));
                }
                checkVersion(capturedId);
            });

    connect(proc, &QProcess::errorOccurred, this,
            [this, capturedId, guard, handled](QProcess::ProcessError) {
                if (*handled)
                    return;
                if (guard && guard->state() == QProcess::NotRunning) {
                    *handled = true;
                    m_model->setInstalling(capturedId, false);
                    guard->deleteLater();
                    delete handled;
                    emit installFinished(capturedId, false,
                        tr("Failed to start update command."));
                }
            });

    proc->start();
}
