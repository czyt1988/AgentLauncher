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

    // Load the bundled default config for migration fallback.
    QList<Agent> defaults;
    {
        QFile def(QStringLiteral(":/config/default_agents.json"));
        if (def.open(QIODevice::ReadOnly))
            defaults = parse(def.readAll());
    }

    if (!file.open(QIODevice::ReadOnly)) {
        // Fall back to the bundled default directly.
        m_agents = defaults;
        return;
    }

    m_agents = parse(file.readAll());

    // Merge in any fields missing from an older on-disk config.
    if (!defaults.isEmpty())
        migrate(defaults);
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
        o[QStringLiteral("installCommand")] = a.installCommand;
        o[QStringLiteral("updateCommand")] = a.updateCommand;
        o[QStringLiteral("versionCommand")] = a.versionCommand;
        o[QStringLiteral("setupCommand")] = a.setupCommand;
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
        a.installCommand = o.value(QStringLiteral("installCommand")).toString();
        a.updateCommand = o.value(QStringLiteral("updateCommand")).toString();
        a.versionCommand = o.value(QStringLiteral("versionCommand")).toString();
        a.setupCommand = o.value(QStringLiteral("setupCommand")).toString();
        result.append(a);
    }
    return result;
}

void AgentConfig::migrate(const QList<Agent> &defaults)
{
    bool changed = false;
    for (const Agent &def : defaults) {
        auto it = std::find_if(m_agents.begin(), m_agents.end(),
            [&](const Agent &a) { return a.id == def.id; });
        if (it == m_agents.end()) {
            // Agent in default but not on disk — add it.
            m_agents.append(def);
            changed = true;
        } else {
            // Fill in any empty fields from the default.
            auto fill = [&](QString &field, const QString &defVal) {
                if (field.isEmpty() && !defVal.isEmpty()) {
                    field = defVal;
                    changed = true;
                }
            };
            fill(it->name, def.name);
            fill(it->command, def.command);
            fill(it->webUrl, def.webUrl);
            fill(it->configDir, def.configDir);
            fill(it->icon, def.icon);
            fill(it->color, def.color);
            fill(it->installCommand, def.installCommand);
            fill(it->updateCommand, def.updateCommand);
            fill(it->versionCommand, def.versionCommand);
            fill(it->setupCommand, def.setupCommand);
        }
    }
    if (changed)
        save();
}
