#include "appearance_controller.h"
#include "ui_preview_controller.h"
#include "tunnel_startup_progress.h"
#include <QtTest>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QQuickItem>
#include <QSettings>
#include <QFontDatabase>
#include <QDir>
#include <memory>

class ClientUiTest : public QObject {
    Q_OBJECT
    std::unique_ptr<AppearanceController> appearance;
    std::unique_ptr<UiPreviewController> demo;
    std::unique_ptr<QQmlApplicationEngine> engine;
    QStringList warnings;
    QQuickWindow *window = nullptr;
    bool load(const QString &page) {
        appearance = std::make_unique<AppearanceController>(true);
        appearance->setThemeMode("light");
        demo = std::make_unique<UiPreviewController>(page);
        engine = std::make_unique<QQmlApplicationEngine>();
        engine->rootContext()->setContextProperty("appearance", appearance.get());
        engine->rootContext()->setContextProperty("appController", demo.get());
        engine->rootContext()->setContextProperty("uiPreviewMode", true);
        engine->rootContext()->setContextProperty("uiPreviewPage", page);
        connect(engine.get(), &QQmlApplicationEngine::warnings, this, [this](const QList<QQmlError> &errors) {
            for (const auto &error : errors) warnings << error.toString();
        });
        engine->load(QUrl::fromLocalFile(QStringLiteral(CLIENT_UI_SOURCE)));
        if (engine->rootObjects().isEmpty()) return false;
        window = qobject_cast<QQuickWindow *>(engine->rootObjects().first());
        return window != nullptr;
    }
private slots:
    void initTestCase() {
        QQuickStyle::setStyle("Basic");
        for (const QString &name : {QString("msyh.ttc"), QString("msyhbd.ttc"), QString("segoeui.ttf"), QString("segmdl2.ttf"), QString("consola.ttf")})
            QFontDatabase::addApplicationFont(QDir(qEnvironmentVariable("WINDIR", "C:/Windows")).filePath("Fonts/" + name));
    }
    void cleanup() { engine.reset(); window = nullptr; demo.reset(); appearance.reset(); warnings.clear(); }
    void allPagesAdaptToLightAndDark_data() {
        QTest::addColumn<QString>("page");
        for (const auto &page : {"login", "dashboard", "nodes", "logs", "settings"}) QTest::newRow(page) << QString(page);
    }
    void allPagesAdaptToLightAndDark() {
        QFETCH(QString, page);
        QVERIFY(load(page));
        QTRY_VERIFY(!window->property("dark").toBool());
        appearance->setThemeMode("dark");
        QTRY_VERIFY(window->property("dark").toBool());
        appearance->setThemeMode("light");
        QTRY_VERIFY(!window->property("dark").toBool());
        appearance->setThemeMode("invalid");
        QCOMPARE(appearance->themeMode(), QString("light"));
        appearance->setGlassEnabled(false);
        QVERIFY(!appearance->backdropActive());
        QTest::qWait(30);
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
    }
    void navigationAndScrollKeepControlsAvailableAtMinimumSize() {
        QVERIFY(load("dashboard"));
        window->resize(880, 640);
        auto *remote = window->findChild<QObject *>("remotePort");
        auto *local = window->findChild<QObject *>("localPort");
        QVERIFY(remote && local);
        QCOMPARE(remote->property("from").toInt(), 10000);
        QCOMPARE(remote->property("to").toInt(), 30000);
        QCOMPARE(local->property("value").toInt(), 22);
        auto *scroll = window->findChild<QObject *>("tunnelScroll");
        QVERIFY(scroll);
        QTRY_VERIFY(scroll->property("contentHeight").toReal() > scroll->property("availableHeight").toReal());
        auto *logs = window->findChild<QObject *>("navLogs");
        QVERIFY(logs && QMetaObject::invokeMethod(logs, "clicked"));
        QCOMPARE(window->property("pageIndex").toInt(), 1);
        auto *settings = window->findChild<QObject *>("navSettings");
        QVERIFY(settings && QMetaObject::invokeMethod(settings, "clicked"));
        QCOMPARE(window->property("pageIndex").toInt(), 2);
        auto *settingsScroll = window->findChild<QObject *>("settingsScroll");
        QVERIFY(settingsScroll);
        QTRY_VERIFY(settingsScroll->property("contentHeight").toReal() > settingsScroll->property("availableHeight").toReal());
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
    }
    void nodeSelectionTransitionsWithoutProductionServices() {
        QVERIFY(load("nodes"));
        auto *dialog = window->findChild<QObject *>("nodeSelectionDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(!demo->value("connected").toBool());
        QCOMPARE(demo->value("nodeList").toList().size(), 3);
        demo->selectNode(0);
        QTRY_VERIFY(window->findChild<QObject *>("navTunnels") != nullptr);
        QVERIFY(demo->value("connected").toBool());
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
    }
    void demoThemeChangesDoNotPersistPreferences() {
        QSettings settings;
        const auto beforeMode = settings.value("appearance/theme");
        const auto beforeGlass = settings.value("appearance/glass");
        AppearanceController isolated(true);
        isolated.setThemeMode("dark"); isolated.setGlassEnabled(false); isolated.setThemeMode("system");
        QCOMPARE(settings.value("appearance/theme"), beforeMode);
        QCOMPARE(settings.value("appearance/glass"), beforeGlass);
    }

    void tunnelStartupUsesRealEventsAndCountsDistinctProxies() {
        TunnelStartupProgress progress;
        progress.begin(2, true);
        QCOMPARE(progress.step, 2);
        QVERIFY(!progress.observe("frpc 进程已启动"));
        QCOMPARE(progress.step, 2);
        QVERIFY(progress.observe("[I] [MEOW_TLS] trust-fetch complete enabled=true"));
        QCOMPARE(progress.step, 3);
        QVERIFY(progress.observe("[I] login to server success, get run id [test]"));
        QCOMPARE(progress.step, 4);
        QVERIFY(!progress.observe("[I] [MEOW_TLS] trust-fetch begin"));
        QCOMPARE(progress.step, 4);
        QVERIFY(progress.observe("[I] [test] [lease.ssh] start proxy success"));
        QCOMPARE(progress.step, 4);
        QVERIFY(progress.observe("[I] [test] [lease.ssh] start proxy success"));
        QCOMPARE(progress.step, 4); // A repeated log is not a second tunnel.
        QVERIFY(progress.observe("[I] [test] [lease.game] start proxy success"));
        QCOMPARE(progress.step, 5);
        QVERIFY(!progress.failed);
        QVERIFY(!progress.observe("[I] [MEOW_TLS] trust-fetch begin"));
        QCOMPARE(progress.step, 5); // Ordinary work connections do not reset startup.
        QVERIFY(progress.observe("[I] try to connect to server..."));
        QCOMPARE(progress.step, 2);
        QVERIFY(progress.observe("[W] [MEOW_TLS] trust-fetch failed"));
        QVERIFY(progress.failed);
        QCOMPARE(progress.step, 2);
        QVERIFY(progress.observe("[W] connect to server error: certificate rejected"));
        QCOMPARE(progress.step, 2);
        QVERIFY(progress.observe("[I] try to connect to server..."));
        QVERIFY(!progress.failed);
        QVERIFY(progress.observe("[I] [MEOW_TLS] trust-fetch complete enabled=false"));
        QCOMPARE(progress.step, 3);
        QVERIFY(progress.tlsDisabled);
        progress.stop();
        QVERIFY(!progress.observe("[I] [test] [lease.game] start proxy success"));
        QCOMPARE(progress.step, 0);
    }

    void startupErrorsNeverBecomeSuccessfulFromProcessOrLocalServiceLogs() {
        TunnelStartupProgress progress;
        progress.begin(1, true);
        progress.observe("[I] [MEOW_TLS] trust-fetch complete enabled=true");
        QVERIFY(progress.observe("[W] connect to server error: EOF"));
        QVERIFY(progress.failed);
        QCOMPARE(progress.step, 3);
        QVERIFY(!progress.observe("[E] connect to local service [127.0.0.1:25565] error: refused"));
        QVERIFY(progress.failed);
        progress.observe("[I] try to connect to server...");
        progress.observe("[I] [MEOW_TLS] trust-fetch complete enabled=true");
        progress.observe("[I] login to server success, get run id [test]");
        QVERIFY(progress.observe("[W] [lease.ssh] start proxy error: port occupied"));
        QCOMPARE(progress.step, 4);
        QVERIFY(progress.failed);
        progress.observe("[I] [test] [lease.ssh] start proxy success");
        QCOMPARE(progress.step, 5);
        QVERIFY(!progress.failed);
        QVERIFY(!progress.observe("[E] connect to local service [127.0.0.1:25565] error: refused"));
        QCOMPARE(progress.step, 5); // Local service failure is not FRP login failure.
    }

    void centeredLoginAndTunnelProgressAdaptToThemes() {
        QVERIFY(load("login"));
        window->resize(880, 640);
        auto *card = qobject_cast<QQuickItem *>(window->findChild<QObject *>("loginCard"));
        QVERIFY(card);
        QTRY_VERIFY(qAbs(card->mapToScene(QPointF(card->width()/2,0)).x() - window->width()/2) < 2);
        QVERIFY(window->findChild<QObject *>("loginScroll"));
        QVERIFY(!window->findChild<QObject *>("tunnelStartupProgress"));
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
        cleanup();
        QVERIFY(load("dashboard"));
        window->resize(880, 640);
        demo->startTunnels();
        auto *progress = window->findChild<QObject *>("tunnelStartupProgress");
        auto *message = window->findChild<QObject *>("tunnelStartupMessage");
        QVERIFY(progress && message);
        QTRY_VERIFY(progress->property("visible").toBool());
        QCOMPARE(progress->property("step").toInt(), 2);
        QCOMPARE(message->property("text").toString(), demo->value("tunnelStartMessage").toString());
        for (const auto &theme : {"light", "dark"}) {
            appearance->setThemeMode(theme);
            for (int step=1; step<=5; ++step) {
                demo->insert("tunnelStartStep",step);
                QTRY_COMPARE(progress->property("step").toInt(), step);
            }
            demo->insert("tunnelStartFailed",true);
            QTRY_VERIFY(progress->property("failed").toBool());
            demo->insert("tunnelStartTLSDisabled",true);
            QTest::qWait(20);
            QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
        }
    }

    void captureUpdatedUiWhenRequested() {
        const QString output = qEnvironmentVariable("MEOWFRP_UI_CAPTURE_DIR");
        if (output.isEmpty()) QSKIP("Optional local UI captures disabled");
        QVERIFY(QDir().mkpath(output));
        for (const auto &page : {"login", "dashboard"}) {
            QVERIFY(load(page));
            if (QString(page) == "dashboard") demo->startTunnels();
            for (const auto &theme : {"light", "dark"}) {
                appearance->setThemeMode(theme);
                QTest::qWait(200);
                const QImage screenshot = window->grabWindow();
                QVERIFY(!screenshot.isNull());
                QVERIFY(screenshot.save(QDir(output).filePath(QString("%1-%2.png").arg(page,theme))));
            }
            QVERIFY2(warnings.isEmpty(),qPrintable(warnings.join('\n')));
            cleanup();
        }
    }
};

QTEST_MAIN(ClientUiTest)
#include "ui_test.moc"
