#include "AgentConfig.h"

#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

void AgentConfig::load()
{
    const QString path = configFilePath();
    QFile file(path);

    // Seed the user config from the bundled default on first run.
    if (!file.exists()) {
        QFile seed(QStringLiteral(":/config/default_agents.json"));
        if (seed.open(QIODevice::ReadOnly)) {
            const QByteArray data = seed.readAll();
            seed.close();

            QDir().mkpath(QFileInfo(path).absolutePath());
            QFile out(path);
            if (out.open(QIODevice::WriteOnly))
                out.write(data);
        }
    }

    if (!file.open(QIODevice::ReadOnly)) {
        // Fall back to the bundled default directly.
        QFile def(QStringLiteral(":/config/default_agents.json"));
        if (def.open(QIODevice::ReadOnly))
            m_agents = parse(def.readAll());
        return;
    }

    m_agents = parse(file.readAll());
}

bool AgentConfig::save()
{
    QJsonArray arr;
    for (const Agent &a : m_agents) {
        QJsonObject o;
        o[QStringLiteral("id")] = a.id;
        o[QStringLiteral("name")] = a.name;
        o[QStringLiteral("command")] = a.command;
        o[QStringLiteral("webUrl")] = a.webUrl;
        o[QStringLiteral("configDir")] = a.configDir;
        o[QStringLiteral("icon")] = a.icon;
        o[QStringLiteral("color")] = a.color;
        arr.append(o);
    }
    QJsonObject root;
    root[QStringLiteral("agents")] = arr;

    const QString path = configFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

void AgentConfig::updateAgent(const QString &id, const QString &command, const QString &webUrl)
{
    for (Agent &a : m_agents) {
        if (a.id == id) {
            a.command = command;
            a.webUrl = webUrl;
            break;
        }
    }
}

QString AgentConfig::configFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return dir + QStringLiteral("/agents.json");
}

QList<Agent> AgentConfig::parse(const QByteArray &data) const
{
    QList<Agent> result;
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    const QJsonArray arr = doc.object().value(QStringLiteral("agents")).toArray();
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        Agent a;
        a.id = o.value(QStringLiteral("id")).toString();
        a.name = o.value(QStringLiteral("name")).toString();
        a.command = o.value(QStringLiteral("command")).toString();
        a.webUrl = o.value(QStringLiteral("webUrl")).toString();
        a.configDir = o.value(QStringLiteral("configDir")).toString();
        a.icon = o.value(QStringLiteral("icon")).toString();
        a.color = o.value(QStringLiteral("color")).toString();
        result.append(a);
    }
    return result;
}
