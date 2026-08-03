#include "AgentLauncher.h"
#include "AgentConfig.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
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
    checkAll();
    m_timer->start(3000);
}

void AgentLauncher::launch(const QString &id)
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
    QTimer::singleShot(1500, this, &AgentLauncher::checkAll);
    return ok;
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

bool AgentLauncher::updateAgent(const QString &id, const QString &command, const QString &webUrl)
{
    QList<Agent> &agents = m_model->agents();
    bool changed = false;
    for (Agent &a : agents) {
        if (a.id == id) {
            a.command = command;
            a.webUrl = webUrl;
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
        emit m_model->dataChanged(idx, idx, { AgentModel::CommandRole, AgentModel::WebUrlRole });
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
