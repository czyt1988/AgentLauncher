#include "AgentConfig.h"
#include "AgentLauncher.h"
#include "AgentModel.h"
#include "Logger.h"

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QGuiApplication::setApplicationName(QStringLiteral("AgentLauncher"));

    Logger::install();

    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app-icon.png")));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    // Load locale-appropriate translation from embedded :/i18n/ resources.
    QTranslator translator;
    if (translator.load(QLocale(), QStringLiteral("agentlauncher"),
                        QStringLiteral("_"), QStringLiteral(":/i18n")))
        app.installTranslator(&translator);

    AgentConfig config;
    config.load(); // loads or seeds default agents.json

    AgentModel model;
    model.setAgents(config.agents());

    AgentLauncher launcher(&model);
    // Deletion records for built-in agents, persisted in agents.json.
    launcher.setRemovedIds(config.removedIds());
    // Root window title, preserved by the launcher across config saves.
    launcher.setTitle(config.title());
    launcher.start();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("agentModel"), &model);
    engine.rootContext()->setContextProperty(QStringLiteral("launcher"), &launcher);
    engine.rootContext()->setContextProperty(QStringLiteral("appTitle"), config.title());

    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
