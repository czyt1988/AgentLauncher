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
    if (parts.isEmpty())
        return;

    const QString program = parts.first();
    const QStringList args = parts.mid(1);

    // startDetached so the agent keeps running after this launcher closes.
    QProcess proc;
    proc.setProgram(program);
    proc.setArguments(args);
    proc.setWorkingDirectory(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    bool ok = proc.startDetached();

    if (ok) {
        // Re-check shortly so the card flips to running fast.
        QTimer::singleShot(1500, this, &AgentLauncher::checkAll);
    }
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
