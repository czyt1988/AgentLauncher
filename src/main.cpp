#include "AgentConfig.h"
#include "AgentLauncher.h"
#include "AgentModel.h"
#include "Logger.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QGuiApplication::setApplicationName(QStringLiteral("AgentLauncher"));

    Logger::install();

    QGuiApplication app(argc, argv);
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
    launcher.start();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("agentModel"), &model);
    engine.rootContext()->setContextProperty(QStringLiteral("launcher"), &launcher);

    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
