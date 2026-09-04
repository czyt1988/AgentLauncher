#include <QtTest>
#include <QStandardPaths>
#include <QTemporaryFile>

#include "AgentConfig.h"
#include "AgentLauncher.h"
#include "AgentModel.h"

// Unit tests for the config/model/launcher core. All config reads and
// writes are isolated from the user's real config via
// QStandardPaths test mode.
class TestCore : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QStandardPaths::setTestModeEnabled(true);
        QFile::remove(AgentConfig::configFilePath());
    }

    void testSlugFromName()
    {
        QCOMPARE(AgentConfig::slugFromName(QStringLiteral("Kimi Code")),
                 QStringLiteral("kimi-code"));
        QCOMPARE(AgentConfig::slugFromName(QStringLiteral("My_Agent! 2")),
                 QStringLiteral("my-agent-2"));
        QCOMPARE(AgentConfig::slugFromName(QStringLiteral("  --Trim--  ")),
                 QStringLiteral("trim"));
        QCOMPARE(AgentConfig::slugFromName(QStringLiteral("???")),
                 QStringLiteral("agent"));
    }

    void testResolveIconPassthrough()
    {
        // Regression: file:// URLs must pass through unchanged, otherwise a
        // resolved local-file icon degrades to default.svg after save+reload.
        QCOMPARE(AgentConfig::resolveIcon(QStringLiteral("file:///C:/icons/a.svg")),
                 QStringLiteral("file:///C:/icons/a.svg"));
        QCOMPARE(AgentConfig::resolveIcon(QStringLiteral("")),
                 QStringLiteral("qrc:/icons/default.svg"));
        QCOMPARE(AgentConfig::resolveIcon(QStringLiteral("qrc:/icons/bot.svg")),
                 QStringLiteral("qrc:/icons/bot.svg"));

        QTemporaryFile tmp;
        QVERIFY(tmp.open());
        QVERIFY(AgentConfig::resolveIcon(tmp.fileName())
                    .startsWith(QStringLiteral("file:///")));
    }

    void testRemovedIdsRoundTrip()
    {
        {
            AgentConfig cfg;
            cfg.load(); // seed the test-mode file from the bundled defaults
            cfg.setRemovedIds({"agent-one", "agent-two"});
            QVERIFY(cfg.save());
        }
        AgentConfig reloaded;
        reloaded.load();
        QCOMPARE(reloaded.removedIds(),
                 QStringList({"agent-one", "agent-two"}));
    }

    void testMigrateSkipsRemovedDefaults()
    {
        {
            AgentConfig cfg;
            cfg.load(); // seed defaults into the test-mode location

            // Simulate a Settings-page deletion: drop the agent from the
            // list AND record its id, then persist (same sequence
            // AgentLauncher::removeAgent() produces).
            QList<Agent> remaining;
            for (const Agent &a : cfg.agents()) {
                if (a.id != QStringLiteral("kimi-code"))
                    remaining.append(a);
            }
            cfg.setAgents(remaining);
            cfg.setRemovedIds({QStringLiteral("kimi-code")});
            QVERIFY(cfg.save());
        }
        AgentConfig reloaded;
        reloaded.load();
        QVERIFY(reloaded.removedIds().contains(QStringLiteral("kimi-code")));
        for (const Agent &a : reloaded.agents())
            QVERIFY2(a.id != "kimi-code", "removed default must not resurrect");
    }

    void testDefaultAgentIds()
    {
        const QStringList ids = AgentConfig::defaultAgentIds();
        QVERIFY(ids.contains(QStringLiteral("kimi-code")));
        QVERIFY(ids.contains(QStringLiteral("opencode")));
    }

    void testModelInsertRemove()
    {
        AgentModel model;
        Agent a;
        a.id = "x1";
        a.name = "X";
        model.setAgents({a});
        QCOMPARE(model.rowCount(), 1);

        Agent b;
        b.id = "x2";
        b.name = "Y";
        model.insertAgent(0, b);
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.index(0, 0).data(AgentModel::IdRole).toString(),
                 QStringLiteral("x2"));
        QCOMPARE(model.index(1, 0).data(AgentModel::IdRole).toString(),
                 QStringLiteral("x1"));

        QVERIFY(model.removeAgentById("x1"));
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.index(0, 0).data(AgentModel::IdRole).toString(),
                 QStringLiteral("x2"));
        QVERIFY(!model.removeAgentById("missing"));
        QCOMPARE(model.rowCount(), 1);
    }

    void testLauncherCrud()
    {
        AgentConfig cfg;
        cfg.load(); // seed test-mode config with the bundled defaults
        AgentModel model;
        model.setAgents(cfg.agents());
        AgentLauncher launcher(&model);
        launcher.setRemovedIds(cfg.removedIds());
        const int baseCount = model.rowCount();
        QVERIFY(baseCount > 0);

        // --- addAgent with auto id --------------------------------------
        QVariantMap fields;
        fields.insert(QStringLiteral("name"), QStringLiteral("My Agent"));
        fields.insert(QStringLiteral("command"), QStringLiteral("myagent web"));
        fields.insert(QStringLiteral("webUrl"), QStringLiteral("http://127.0.0.1:9999"));
        QVERIFY(launcher.addAgent(fields));
        QCOMPARE(model.rowCount(), baseCount + 1);
        QCOMPARE(model.agent(QStringLiteral("my-agent"))
                     .value(QStringLiteral("name")).toString(),
                 QStringLiteral("My Agent"));
        // Auto-assigned palette color so the card renders correctly.
        QVERIFY(!model.agent(QStringLiteral("my-agent"))
                     .value(QStringLiteral("color")).toString().isEmpty());

        // Persisted?
        AgentConfig persisted;
        persisted.load();
        bool found = false;
        for (const Agent &a : persisted.agents())
            found = found || a.id == "my-agent";
        QVERIFY(found);

        // --- addAgent duplicate name -> -2 suffix ------------------------
        QVERIFY(launcher.addAgent(fields));
        QCOMPARE(model.rowCount(), baseCount + 2);
        QVERIFY(model.indexOf(QStringLiteral("my-agent-2")) >= 0);

        // --- addAgent explicit duplicate id -> false ----------------------
        QVariantMap dup;
        dup.insert(QStringLiteral("id"), QStringLiteral("my-agent"));
        dup.insert(QStringLiteral("name"), QStringLiteral("X"));
        dup.insert(QStringLiteral("command"), QStringLiteral("x"));
        dup.insert(QStringLiteral("webUrl"), QStringLiteral("http://127.0.0.1:1"));
        QVERIFY(!launcher.addAgent(dup));

        // --- updateAgentFull ---------------------------------------------
        fields.insert(QStringLiteral("name"), QStringLiteral("Renamed"));
        QVERIFY(launcher.updateAgentFull(QStringLiteral("my-agent"), fields));
        QCOMPARE(model.agent(QStringLiteral("my-agent"))
                     .value(QStringLiteral("name")).toString(),
                 QStringLiteral("Renamed"));

        // --- removeAgent of a built-in -> recorded in "removed" ----------
        QVERIFY(launcher.removeAgent(QStringLiteral("kimi-code")));
        QVERIFY(model.indexOf(QStringLiteral("kimi-code")) < 0);
        AgentConfig after;
        after.load();
        QVERIFY(after.removedIds().contains(QStringLiteral("kimi-code")));

        // --- restoreDefaults brings it back -------------------------------
        QVERIFY(launcher.restoreDefaults());
        QVERIFY(model.indexOf(QStringLiteral("kimi-code")) >= 0);
        AgentConfig after2;
        after2.load();
        QVERIFY(!after2.removedIds().contains(QStringLiteral("kimi-code")));

        // --- isDefaultAgent / configFilePath ------------------------------
        QVERIFY(launcher.isDefaultAgent(QStringLiteral("opencode")));
        QVERIFY(!launcher.isDefaultAgent(QStringLiteral("my-agent")));
        QVERIFY(launcher.configFilePath().endsWith(QStringLiteral("agents.json")));
    }
};

QTEST_MAIN(TestCore)
#include "tst_core.moc"
