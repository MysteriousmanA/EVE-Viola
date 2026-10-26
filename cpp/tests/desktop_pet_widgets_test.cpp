#include <QtTest>

#include <QApplication>
#include <QAction>
#include <QActionGroup>
#include <QCheckBox>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <memory>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollBar>
#include <QSpinBox>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QTextBrowser>
#include <QTime>
#include <QWheelEvent>

#include "features/desktop_pet/desktop_pet_settings_store.h"
#include "features/desktop_pet/astrbot_notifier.h"
#include "ui/desktop_pet/avatar_crop_widget.h"
#include "ui/desktop_pet/floating_pet_window.h"
#include "ui/desktop_pet_page.h"

namespace {

class AstrBotTestServer {
public:
    QTcpServer server;
    QList<QByteArray> requests;
    QByteArray responseBody{"{\"status\":\"ok\"}"};
    int responseStatus{200};
    int firstResponseStatus{};
    bool holdResponse{false};

    AstrBotTestServer()
    {
        QObject::connect(&server, &QTcpServer::newConnection, &server, [this] {
            while (server.hasPendingConnections()) {
                QTcpSocket* socket = server.nextPendingConnection();
                QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                QObject::connect(socket, &QTcpSocket::readyRead, socket,
                    [this, socket, buffer = QByteArray{}, handled = false]() mutable {
                        buffer += socket->readAll();
                        const auto headerEnd = buffer.indexOf("\r\n\r\n");
                        if (handled || headerEnd < 0) {
                            return;
                        }
                        int length = 0;
                        for (const QByteArray& line : buffer.left(headerEnd).split('\n')) {
                            if (line.toLower().startsWith("content-length:")) {
                                length = line.mid(15).trimmed().toInt();
                            }
                        }
                        if (buffer.size() < headerEnd + 4 + length) {
                            return;
                        }
                        handled = true;
                        requests.append(buffer);
                        if (holdResponse) {
                            return;
                        }
                        const int status = requests.size() == 1 && firstResponseStatus != 0
                            ? firstResponseStatus : responseStatus;
                        socket->write("HTTP/1.1 " + QByteArray::number(status)
                            + " Result\r\nContent-Type: application/json\r\nConnection: close\r\n"
                              "Location: http://127.0.0.1:1/redirect\r\nContent-Length: "
                            + QByteArray::number(responseBody.size()) + "\r\n\r\n" + responseBody);
                        socket->disconnectFromHost();
                    });
            }
        });
    }

    AstrBotSettings settings() const
    {
        AstrBotSettings settings;
        settings.enabled = true;
        settings.serverUrl = QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort());
        settings.apiKey = QStringLiteral("abk_test_secret");
        settings.umo = QStringLiteral("test:FriendMessage:12345");
        return settings;
    }
};

FloatingPetWindow* findPetWindow()
{
    for (QWidget* widget : QApplication::topLevelWidgets()) {
        if (auto* pet = qobject_cast<FloatingPetWindow*>(widget)) {
            return pet;
        }
    }
    return nullptr;
}

void sendWheel(FloatingPetWindow& pet, int delta)
{
    const QPoint position(pet.width() / 2, 64);
    QWheelEvent event(QPointF(position), QPointF(pet.mapToGlobal(position)),
                     QPoint{}, QPoint(0, delta), Qt::NoButton, Qt::NoModifier,
                     Qt::NoScrollPhase, false);
    QApplication::sendEvent(&pet, &event);
}

} // namespace

class DesktopPetWidgetsTest final : public QObject {
    Q_OBJECT

private slots:
    void contextMenuModesAreExclusiveAndStopIsSeparateFromDisable()
    {
        FloatingPetWindow pet;
        std::unique_ptr<QMenu> menu(pet.buildContextMenu());
        const auto actions = menu->findChild<QActionGroup*>()->actions();
        QCOMPARE(actions.size(), 3);
        QVERIFY(actions[0]->isChecked());
        QSignalSpy modes(&pet, &FloatingPetWindow::modeChanged);
        QSignalSpy stopped(&pet, &FloatingPetWindow::stopSoftwareRequested);
        QSignalSpy hidden(&pet, &FloatingPetWindow::hiddenByUser);
        for (int i : {1, 1, 2, 0}) {
            actions[i]->trigger();
            int checked = 0;
            for (auto* action : actions) { checked += action->isChecked(); }
            QCOMPARE(checked, 1);
            QVERIFY(actions[i]->isChecked());
            QCOMPARE(static_cast<int>(pet.mode()), i);
        }
        QCOMPARE(modes.count(), 3);
        menu->findChild<QAction*>(QStringLiteral("desktopPetStopSoftware"))->trigger();
        QCOMPARE(stopped.count(), 1);
        QCOMPARE(hidden.count(), 0);
        menu->findChild<QAction*>(QStringLiteral("desktopPetDisable"))->trigger();
        QCOMPARE(hidden.count(), 1);
        QCOMPARE(stopped.count(), 1);
    }

    void doNotDisturbClearsExistingBubbleAndPreservesEnabledPet()
    {
        FloatingPetWindow pet;
        pet.show();
        pet.showMessage(QStringLiteral("提醒"));
        auto* bubble = pet.findChild<QLabel*>(QStringLiteral("desktopPetSpeechBubble"));
        QVERIFY(bubble->isVisible());
        pet.setMode(DesktopPetMode::DoNotDisturb);
        QVERIFY(pet.isVisible());
        QVERIFY(!bubble->isVisible());
        pet.showMessage(QStringLiteral("勿扰期间禁止显示"));
        QVERIFY(!bubble->isVisible());
        pet.setMode(DesktopPetMode::Resident);
        pet.showMessage(QStringLiteral("恢复提醒"));
        QVERIFY(bubble->isVisible());
    }

    void notificationChannelsRespectModeAndRestoreSavedPreferences()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AstrBotTestServer server;
        QVERIFY(server.server.listen(QHostAddress::LocalHost, 0));
        DesktopPetSettingsStore store(directory.path());
        QImage image(64, 64, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::green);
        QVERIFY(store.saveSourceImage(image));
        QVERIFY(store.save(QJsonObject{
            {QStringLiteral("astrBot"), server.settings().toJson()},
            {QStringLiteral("triggerMessages"), QJsonObject{{QStringLiteral("1"), QStringLiteral("提醒")}}}}));
        {
            DesktopPetPage page(directory.path());
            QCOMPARE(page.mode(), DesktopPetMode::Resident);
            auto* test = page.findChild<QPushButton*>(QStringLiteral("desktopPetAstrBotTest"));
            QVERIFY(!test->isEnabled());
            page.showRuleMessage(1, QStringLiteral("EVE - Alpha"));
            auto* pet = findPetWindow();
            QVERIFY(pet->isVisible());
            page.setMode(DesktopPetMode::Fish);
            QVERIFY(test->isEnabled());
            page.showRuleMessage(1, QStringLiteral("EVE - Alpha"));
            QTRY_COMPARE(server.requests.size(), 1);
            QTRY_VERIFY(page.findChild<QLabel*>(QStringLiteral("desktopPetAstrBotStatus"))
                            ->text().contains(QStringLiteral("已接受")));
            page.setMode(DesktopPetMode::DoNotDisturb);
            QVERIFY(!test->isEnabled());
            QVERIFY(!pet->isVisible());
            page.showRuleMessage(1, QStringLiteral("EVE - Beta"));
            QVERIFY(!pet->isVisible());
            page.setMode(DesktopPetMode::Resident);
            page.showRuleMessage(1, QStringLiteral("EVE - Gamma"));
            QVERIFY(pet->isVisible());
            QCOMPARE(server.requests.size(), 1);
            page.setMode(DesktopPetMode::DoNotDisturb);
        }
        DesktopPetPage restored(directory.path());
        QCOMPARE(restored.mode(), DesktopPetMode::DoNotDisturb);
        QVERIFY(restored.findChild<QCheckBox*>(QStringLiteral("desktopPetAstrBotEnabled"))->isChecked());
        QVERIFY(restored.findChild<QCheckBox*>(QStringLiteral("desktopPetLocalMessagesEnabled"))->isChecked());
        restored.setMode(DesktopPetMode::Fish);
        QVERIFY(restored.findChild<QCheckBox*>(QStringLiteral("desktopPetAstrBotEnabled"))->isEnabled());
    }

    void leavingFishModeCancelsPendingAstrBotRequest()
    {
        QTemporaryDir directory;
        AstrBotTestServer server;
        QVERIFY(server.server.listen(QHostAddress::LocalHost, 0));
        server.holdResponse = true;
        DesktopPetSettingsStore store(directory.path());
        QVERIFY(store.save(QJsonObject{
            {QStringLiteral("mode"), QStringLiteral("fish")},
            {QStringLiteral("astrBot"), server.settings().toJson()},
            {QStringLiteral("triggerMessages"), QJsonObject{{QStringLiteral("1"), QStringLiteral("提醒")}}}}));
        DesktopPetPage page(directory.path());
        auto* notifier = page.findChild<AstrBotNotifier*>();
        QSignalSpy results(notifier, &AstrBotNotifier::deliveryFinished);
        page.showRuleMessage(1, QStringLiteral("EVE - Alpha"));
        QTRY_COMPARE(server.requests.size(), 1);
        QCOMPARE(results.size(), 0);
        page.setMode(DesktopPetMode::Resident);
        QTRY_COMPARE(results.size(), 1);
        QVERIFY(!results.first().first().toBool());
    }

    void astrBotSendsAuthenticatedUtf8MessageAndDeduplicates()
    {
        AstrBotTestServer server;
        QVERIFY(server.server.listen(QHostAddress::LocalHost, 0));
        AstrBotNotifier notifier;
        QSignalSpy results(&notifier, &AstrBotNotifier::deliveryFinished);
        auto settings = server.settings();
        settings.serverUrl += QStringLiteral("/proxy/");
        const QString message = QStringLiteral("Alpha：条件已经满足");
        QVERIFY(notifier.sendMessage(settings, message, QStringLiteral("1:Alpha")));
        QVERIFY(!notifier.sendMessage(settings, message, QStringLiteral("1:Alpha")));
        QVERIFY(notifier.sendMessage(settings, message, QStringLiteral("1:Beta")));
        QTRY_COMPARE(results.count(), 2);
        QCOMPARE(server.requests.size(), 2);
        QVERIFY(results.first().first().toBool());
        const auto request = server.requests.first();
        QVERIFY(request.startsWith("POST /proxy/api/v1/im/message HTTP/1.1\r\n"));
        QVERIFY(request.contains("Authorization: Bearer abk_test_secret\r\n"));
        const auto body = QJsonDocument::fromJson(request.mid(request.indexOf("\r\n\r\n") + 4)).object();
        QCOMPARE(body.value(QStringLiteral("umo")).toString(), settings.umo);
        QCOMPARE(body.value(QStringLiteral("message")).toArray().first().toObject()
                     .value(QStringLiteral("text")).toString(), message);
        settings.cooldownSeconds = 0;
        QVERIFY(notifier.sendMessage(settings, message, QStringLiteral("1:Alpha")));
        // Explicit tests bypass trigger cooldown.
        QVERIFY(notifier.sendMessage(settings, message));
        QTRY_COMPARE(results.count(), 4);
    }

    void astrBotRejectsFailuresWithoutLeakingSecrets_data()
    {
        QTest::addColumn<int>("status");
        QTest::addColumn<QByteArray>("body");
        QTest::newRow("invalid-key") << 401 << QByteArray("abk_test_secret");
        QTest::newRow("missing-im-scope") << 403 << QByteArray("abk_test_secret");
        QTest::newRow("server-error") << 500 << QByteArray("{\"status\":\"ok\"}");
        QTest::newRow("application-error") << 200 << QByteArray("{\"status\":\"error\",\"message\":\"abk_test_secret\"}");
        QTest::newRow("html-login-page") << 200 << QByteArray("<html>Login</html>");
        QTest::newRow("redirect") << 302 << QByteArray("{\"status\":\"ok\"}");
    }

    void astrBotSupportsAlternateMessageRoute()
    {
        AstrBotTestServer server;
        QVERIFY(server.server.listen(QHostAddress::LocalHost, 0));
        server.firstResponseStatus = 404;
        AstrBotNotifier notifier;
        QSignalSpy results(&notifier, &AstrBotNotifier::deliveryFinished);
        QVERIFY(notifier.sendMessage(server.settings(), QStringLiteral("test")));
        QTRY_COMPARE(results.count(), 1);
        QVERIFY(results.first().first().toBool());
        QCOMPARE(server.requests.size(), 2);
        QVERIFY(server.requests.first().startsWith("POST /api/v1/im/message HTTP/1.1"));
        QVERIFY(server.requests.last().startsWith("POST /api/v1/im/messages HTTP/1.1"));
        auto settings = server.settings();
        settings.serverUrl += QStringLiteral("/api/v1/im/messages/");
        QVERIFY(notifier.sendMessage(settings, QStringLiteral("test")));
        QTRY_COMPARE(results.count(), 2);
        QCOMPARE(server.requests.size(), 3);
        QVERIFY(server.requests.last().startsWith("POST /api/v1/im/messages HTTP/1.1"));
    }

    void astrBotRejectsFailuresWithoutLeakingSecrets()
    {
        QFETCH(int, status);
        QFETCH(QByteArray, body);
        AstrBotTestServer server;
        QVERIFY(server.server.listen(QHostAddress::LocalHost, 0));
        server.responseStatus = status;
        server.responseBody = body;
        AstrBotNotifier notifier;
        QSignalSpy results(&notifier, &AstrBotNotifier::deliveryFinished);
        QVERIFY(notifier.sendMessage(server.settings(), QStringLiteral("test")));
        QTRY_COMPARE(results.count(), 1);
        QVERIFY(!results.first().first().toBool());
        QVERIFY(!results.first().at(1).toString().contains(QStringLiteral("abk_test_secret")));
        QCOMPARE(server.requests.size(), 1);
    }

    void astrBotValidatesConfigurationBeforeNetwork()
    {
        AstrBotNotifier notifier;
        QSignalSpy results(&notifier, &AstrBotNotifier::deliveryFinished);
        AstrBotSettings settings;
        QVERIFY(!notifier.sendMessage(settings, QStringLiteral("test")));
        settings.apiKey = QStringLiteral("abk_test");
        settings.umo = QStringLiteral("bot:FriendMessage:123");
        for (const auto& url : {QStringLiteral("file:///tmp/test"), QStringLiteral("http://user:secret@localhost"),
                                QStringLiteral("http://localhost?token=secret"), QStringLiteral("not a url")}) {
            settings.serverUrl = url;
            QVERIFY(!notifier.sendMessage(settings, QStringLiteral("test")));
        }
        settings.serverUrl = QStringLiteral("https://localhost/api/v1/im/message/");
        QCOMPARE(settings.messageUrl().path(), QStringLiteral("/api/v1/im/message"));
        QVERIFY(settings.validationError().isEmpty());
        settings.apiKey += QStringLiteral("\r\nInjected: header");
        QVERIFY(!notifier.sendMessage(settings, QStringLiteral("test")));
        QCOMPARE(results.count(), 6);
    }

    void astrBotBoundsPendingRequestsAndTimesOut()
    {
        AstrBotTestServer server;
        QVERIFY(server.server.listen(QHostAddress::LocalHost, 0));
        server.holdResponse = true;
        AstrBotNotifier notifier;
        QSignalSpy results(&notifier, &AstrBotNotifier::deliveryFinished);
        for (int i = 0; i < 4; ++i) {
            QVERIFY(notifier.sendMessage(server.settings(), QStringLiteral("test")));
        }
        QVERIFY(!notifier.sendMessage(server.settings(), QStringLiteral("test")));
        QCOMPARE(results.count(), 1);
        QTRY_COMPARE(server.requests.size(), 4);
        QTRY_COMPARE_WITH_TIMEOUT(results.count(), 5, 15000);
        for (const auto& result : results) {
            QVERIFY(!result.first().toBool());
        }
    }

    void astrBotPagePersistsAndPushes_data()
    {
        QTest::addColumn<bool>("localWithAvatar");
        QTest::newRow("push-without-avatar") << false;
        QTest::newRow("bubble-and-push") << true;
    }

    void astrBotPagePersistsAndPushes()
    {
        QFETCH(bool, localWithAvatar);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AstrBotTestServer server;
        QVERIFY(server.server.listen(QHostAddress::LocalHost, 0));
        DesktopPetSettingsStore store(directory.path());
        if (localWithAvatar) {
            QVERIFY(store.save(QJsonObject{}));
            QImage image(64, 64, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::green);
            QVERIFY(store.saveSourceImage(image));
        }
        {
            DesktopPetPage page(directory.path());
            page.setMode(DesktopPetMode::Fish);
            auto* local = page.findChild<QCheckBox*>(QStringLiteral("desktopPetLocalMessagesEnabled"));
            auto* enabled = page.findChild<QCheckBox*>(QStringLiteral("desktopPetAstrBotEnabled"));
            auto* url = page.findChild<QLineEdit*>(QStringLiteral("desktopPetAstrBotServerUrl"));
            auto* key = page.findChild<QLineEdit*>(QStringLiteral("desktopPetAstrBotApiKey"));
            auto* umo = page.findChild<QLineEdit*>(QStringLiteral("desktopPetAstrBotUmo"));
            QVERIFY(local && enabled && url && key && umo);
            QVERIFY(local->isChecked());
            QVERIFY(!enabled->isChecked());
            QCOMPARE(key->echoMode(), QLineEdit::Password);
            local->setChecked(localWithAvatar);
            url->setText(server.settings().serverUrl);
            key->setText(server.settings().apiKey);
            umo->setText(server.settings().umo);
            page.findChild<QSpinBox*>(QStringLiteral("desktopPetAstrBotCooldown"))->setValue(47);
            ColorRule rule;
            rule.number = 1;
            page.setColorRules({rule});
            auto* message = page.findChild<QLineEdit*>(QStringLiteral("desktopPetRuleMessage_1"));
            QVERIFY(message);
            message->setText(QStringLiteral("条件满足"));
            page.showRuleMessage(1, QStringLiteral("EVE - Alpha"));
            QCOMPARE(server.requests.size(), 0);
            enabled->setChecked(true);
            page.showRuleMessage(1, QStringLiteral("EVE - Alpha"));
            QTRY_COMPARE(server.requests.size(), 1);
            auto* status = page.findChild<QLabel*>(QStringLiteral("desktopPetAstrBotStatus"));
            QVERIFY(status);
            QTRY_VERIFY(status->text().contains(QStringLiteral("已接受")));
            auto* pet = findPetWindow();
            QVERIFY(pet);
            QCOMPARE(pet->isVisible(), localWithAvatar);
            if (localWithAvatar) {
                QCOMPARE(pet->findChild<QLabel*>(QStringLiteral("desktopPetSpeechBubble"))->text(),
                         QStringLiteral("Alpha：条件满足"));
            }
            message->clear();
            page.showRuleMessage(1, QStringLiteral("EVE - Beta"));
            QCOMPARE(server.requests.size(), 1);
            page.findChild<QPushButton*>(QStringLiteral("desktopPetAstrBotTest"))->click();
            QTRY_COMPARE(server.requests.size(), 2);
            QTRY_VERIFY(store.load().settings.value(QStringLiteral("astrBot")).toObject()
                            .value(QStringLiteral("enabled")).toBool());
        }
        const auto saved = store.load();
        const auto config = AstrBotSettings::fromJson(saved.settings.value(QStringLiteral("astrBot")).toObject());
        QVERIFY(config.enabled);
        QCOMPARE(config.serverUrl, server.settings().serverUrl);
        QCOMPARE(config.apiKey, server.settings().apiKey);
        QCOMPARE(config.umo, server.settings().umo);
        QCOMPARE(config.cooldownSeconds, 47);
        QCOMPARE(saved.settings.value(QStringLiteral("localMessagesEnabled")).toBool(), localWithAvatar);
        DesktopPetPage restored(directory.path());
        QVERIFY(restored.findChild<QCheckBox*>(QStringLiteral("desktopPetAstrBotEnabled"))->isChecked());
        QCOMPARE(restored.findChild<QCheckBox*>(QStringLiteral("desktopPetLocalMessagesEnabled"))->isChecked(), localWithAvatar);
        QCOMPARE(restored.findChild<QLineEdit*>(QStringLiteral("desktopPetAstrBotUmo"))->text(), config.umo);
    }

    void astrBotChangesPreserveCorruptSettingsFile()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile file(directory.filePath(QStringLiteral("desktopPet.json")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("broken json");
        file.close();
        {
            DesktopPetPage page(directory.path());
            page.findChild<QCheckBox*>(QStringLiteral("desktopPetAstrBotEnabled"))->setChecked(true);
        }
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("broken json"));
    }

    void cropProducesCircularAvatarAndRespectsOffsets()
    {
        AvatarCropWidget crop;
        QVERIFY(crop.croppedImage().isNull());
        QImage source(400, 200, QImage::Format_ARGB32_Premultiplied);
        source.fill(Qt::red);
        {
            QPainter painter(&source);
            painter.fillRect(200, 0, 200, 200, Qt::blue);
        }
        QSignalSpy changed(&crop, &AvatarCropWidget::cropChanged);
        crop.setImage(source);
        QCOMPARE(changed.count(), 1);
        crop.setNormalizedOffset(QPointF(1, 0));
        const QImage left = crop.croppedImage();
        QCOMPARE(left.size(), QSize(256, 256));
        QCOMPARE(left.pixelColor(128, 128), QColor(Qt::red));
        QCOMPARE(left.pixelColor(0, 0).alpha(), 0);
        crop.setNormalizedOffset(QPointF(-1, 0));
        QCOMPARE(crop.croppedImage().pixelColor(128, 128), QColor(Qt::blue));
        crop.setZoomPercent(999);
        QCOMPARE(crop.zoomPercent(), 300);
        crop.setNormalizedOffset(QPointF(9, -9));
        QCOMPARE(crop.normalizedOffset(), QPointF(1, -1));
    }

    void wheelAccumulatesPartialStepsAndPreservesAvatarCenter()
    {
        FloatingPetWindow pet;
        pet.move(200, 100);
        const QPoint center = pet.mapToGlobal(QPoint(pet.width() / 2, 64));
        QSignalSpy resized(&pet, &FloatingPetWindow::sizeChanged);
        sendWheel(pet, 60);
        QCOMPARE(resized.count(), 0);
        sendWheel(pet, 60);
        QCOMPARE(resized.count(), 1);
        QCOMPARE(resized.first().first().toInt(), 140);
        QCOMPARE(pet.mapToGlobal(QPoint(pet.width() / 2, 70)), center);
        sendWheel(pet, 12000);
        QCOMPARE(resized.last().first().toInt(), 192);
        sendWheel(pet, -12000);
        QCOMPARE(resized.last().first().toInt(), 72);
    }

    void wheelOnlyScrollsWhileHistoryIsVisible()
    {
        FloatingPetWindow pet;
        pet.show();
        for (int i = 0; i < 10; ++i) {
            pet.appendTriggerRecord(QStringLiteral("12:34:56  Alpha · 规则 #%1\n%2")
                .arg(i).arg(QStringLiteral("历史记录详情\n").repeated(30)));
        }
        // A partial zoom gesture before opening history must not carry over after closing it.
        sendWheel(pet, 60);
        pet.toggleTriggerHistory();
        auto* history = pet.findChild<QTextBrowser*>(QStringLiteral("desktopPetHistoryBubble"));
        auto* scrollBar = history->verticalScrollBar();
        QTRY_VERIFY(scrollBar->maximum() > 0);
        const QSize size = pet.size();
        QSignalSpy resized(&pet, &FloatingPetWindow::sizeChanged);
        scrollBar->setValue(0);
        sendWheel(pet, -120);
        QVERIFY(scrollBar->value() > 0);
        sendWheel(pet, 120);
        QCOMPARE(scrollBar->value(), 0);

        const auto wheelOverHistory = [history](int delta) {
            QWidget* viewport = history->viewport();
            const QPoint point = viewport->rect().center();
            QWheelEvent event(QPointF(point), QPointF(viewport->mapToGlobal(point)),
                             QPoint{}, QPoint(0, delta), Qt::NoButton, Qt::NoModifier,
                             Qt::NoScrollPhase, false);
            QApplication::sendEvent(viewport, &event);
        };
        wheelOverHistory(-120);
        QVERIFY(scrollBar->value() > 0);
        scrollBar->setValue(scrollBar->maximum());
        wheelOverHistory(-120);
        sendWheel(pet, -120);
        QCOMPARE(scrollBar->value(), scrollBar->maximum());
        scrollBar->setValue(0);
        wheelOverHistory(120);
        sendWheel(pet, 120);
        QCOMPARE(scrollBar->value(), 0);
        QCOMPARE(pet.size(), size);
        QCOMPARE(resized.count(), 0);

        pet.toggleTriggerHistory();
        sendWheel(pet, 60);
        QCOMPARE(resized.count(), 0);
        sendWheel(pet, 60);
        QCOMPARE(resized.count(), 1);
        QCOMPARE(resized.first().first().toInt(), 140);
    }

    void messagesPreserveAvatarCenter_data()
    {
        QTest::addColumn<bool>("enabled");
        QTest::newRow("enabled") << true;
        QTest::newRow("temporarily-revealed") << false;
    }

    void messagesPreserveAvatarCenter()
    {
        QFETCH(bool, enabled);
        FloatingPetWindow pet;
        const QRect available = QGuiApplication::primaryScreen()->availableGeometry();
        pet.move(available.center().x() - pet.width() / 2, available.top() + 24);
        if (enabled) {
            pet.show();
        }
        const QPoint center = pet.mapToGlobal(QPoint(pet.width() / 2, 64));
        auto* timer = pet.findChild<QTimer*>();
        QVERIFY(timer);
        const QStringList messages{
            QStringLiteral("Alpha：条件已经满足"),
            QStringLiteral("Alpha、Beta、Gamma：条件已经满足，请切换账号处理。").repeated(4),
            QStringLiteral("Alpha、Beta：条件已经满足"),
            QStringLiteral("Alpha：条件已经满足")};
        const int initialWidth = pet.width();
        int maximumWidth = initialWidth;
        for (const QString& message : messages) {
            pet.showMessage(message);
            maximumWidth = qMax(maximumWidth, pet.width());
            QCOMPARE(pet.mapToGlobal(QPoint(pet.width() / 2, 64)), center);
        }
        QVERIFY(maximumWidth > initialWidth);
        QVERIFY(pet.width() < maximumWidth);
        timer->stop();
        QVERIFY(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection));
        QCOMPARE(pet.mapToGlobal(QPoint(pet.width() / 2, 64)), center);
        QCOMPARE(pet.isVisible(), enabled);
    }

    void messageFitsBubbleAndOnlyTemporarilyRevealsHiddenPet()
    {
        FloatingPetWindow pet;
        auto* bubble = pet.findChild<QLabel*>(QStringLiteral("desktopPetSpeechBubble"));
        QVERIFY(bubble);
        QSignalSpy autoHidden(&pet, &FloatingPetWindow::autoHiddenByMessage);
        QSignalSpy expired(&pet, &FloatingPetWindow::messageExpired);
        const QString message = QStringLiteral("这是需要完整显示的桌宠提示。").repeated(8);
        pet.showMessage(message);
        QVERIFY(pet.isVisible());
        QVERIFY(bubble->isVisible());
        QCOMPARE(bubble->text(), message);
        QVERIFY(bubble->height() >= bubble->heightForWidth(bubble->width()));
        QVERIFY(pet.mask().contains(bubble->geometry().center()));
        auto* timer = pet.findChild<QTimer*>();
        QVERIFY(timer);
        timer->stop();
        QVERIFY(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection));
        QVERIFY(!pet.isVisible());
        QCOMPARE(autoHidden.count(), 1);
        QCOMPARE(expired.count(), 1);

        pet.show();
        pet.showMessage(QStringLiteral("已启用的桌宠提示结束后继续显示"));
        timer->stop();
        QVERIFY(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection));
        QVERIFY(pet.isVisible());
        QVERIFY(!bubble->isVisible());
        QCOMPARE(autoHidden.count(), 1);
    }

    void clickShowsTenLatestRecordsInPetBubbleWithTimeOnly()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        DesktopPetSettingsStore store(directory.path());
        QImage image(64, 64, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::green);
        QVERIFY(store.saveSourceImage(image));
        QVERIFY(store.save(QJsonObject{
            {QStringLiteral("enabled"), true},
            {QStringLiteral("localMessagesEnabled"), false},
            {QStringLiteral("triggerMessages"), QJsonObject{
                {QStringLiteral("1"), QStringLiteral("条件满足")}}}}));
        DesktopPetPage page(directory.path());
        ColorRule rule;
        rule.number = 1;
        rule.note = QStringLiteral("测试规则");
        page.setColorRules({rule});
        auto* pet = findPetWindow();
        QVERIFY(pet);
        auto* history = pet->findChild<QTextBrowser*>(QStringLiteral("desktopPetHistoryBubble"));
        QVERIFY(history);
        QVERIFY(!history->isWindow());
        const auto windowCount = QApplication::topLevelWidgets().size();
        const auto clickAvatar = [pet] {
            QTest::mouseClick(pet, Qt::LeftButton, Qt::NoModifier,
                              QPoint(pet->width() / 2, 64));
        };
        clickAvatar();
        QVERIFY(history->isVisible());
        QVERIFY(history->toPlainText().contains(QStringLiteral("暂无触发记录")));
        QCOMPARE(QApplication::topLevelWidgets().size(), windowCount);
        clickAvatar();
        QVERIFY(!history->isVisible());

        const QString before = QTime::currentTime().toString(QStringLiteral("HH:mm:ss"));
        for (int i = 1; i <= 12; ++i) {
            page.showRuleMessage(1, QStringLiteral("EVE - Alpha%1").arg(i));
        }
        const QString after = QTime::currentTime().toString(QStringLiteral("HH:mm:ss"));
        clickAvatar();
        QVERIFY(history->isVisible());
        QString text = history->toPlainText();
        QVERIFY(text.contains(QStringLiteral("最近 10 条")));
        QCOMPARE(text.count(QStringLiteral("规则 #1  测试规则")), 10);
        QVERIFY(!text.contains(QStringLiteral("Alpha1 ·")));
        QVERIFY(!text.contains(QStringLiteral("Alpha2 ·")));
        QVERIFY(text.indexOf(QStringLiteral("Alpha12 ·")) < text.indexOf(QStringLiteral("Alpha3 ·")));
        QVERIFY(text.contains(before) || text.contains(after));
        const QRegularExpression recordHeader(QStringLiteral("^\\d{2}:\\d{2}:\\d{2}  Alpha12 · 规则 #1  测试规则$"),
                                             QRegularExpression::MultilineOption);
        QVERIFY(recordHeader.match(text).hasMatch());
        QVERIFY(pet->mask().contains(history->geometry().center()));
        QVERIFY(!pet->findChild<QTimer*>()->isActive());

        // Even a rule without a notification message is recorded, and an open history updates live.
        page.showRuleMessage(2, QStringLiteral("EVE - Beta"));
        text = history->toPlainText();
        QCOMPARE(text.count(QStringLiteral("规则 #")), 10);
        QVERIFY(text.indexOf(QStringLiteral("Beta · 规则 #2")) < text.indexOf(QStringLiteral("Alpha12 ·")));
        QVERIFY(!text.contains(QStringLiteral("Alpha3 ·")));
        clickAvatar();
        QVERIFY(!history->isVisible());
        QVERIFY(pet->isVisible());
        clickAvatar();
        QCOMPARE(history->toPlainText(), text);
    }

    void historyReplacesMessageAndSurvivesBubbleExpiry()
    {
        FloatingPetWindow pet;
        pet.show();
        pet.appendTriggerRecord(QStringLiteral("12:34:56  Alpha · 规则 #1\n提醒"));
        pet.showMessage(QStringLiteral("临时提醒"));
        auto* speech = pet.findChild<QLabel*>(QStringLiteral("desktopPetSpeechBubble"));
        auto* history = pet.findChild<QTextBrowser*>(QStringLiteral("desktopPetHistoryBubble"));
        auto* timer = pet.findChild<QTimer*>();
        QVERIFY(speech->isVisible());
        pet.toggleTriggerHistory();
        QVERIFY(!speech->isVisible());
        QVERIFY(history->isVisible());
        QVERIFY(!timer->isActive());
        pet.showMessage(QStringLiteral("新提醒"));
        QVERIFY(history->isVisible());
        QVERIFY(!speech->isVisible());
        QVERIFY(!timer->isActive());
        pet.toggleTriggerHistory();
        pet.showMessage(QStringLiteral("新提醒"));
        QVERIFY(speech->isVisible());
        QVERIFY(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection));
        pet.toggleTriggerHistory();
        QVERIFY(history->toPlainText().contains(QStringLiteral("12:34:56  Alpha")));
        pet.hideByUser();
        QVERIFY(!history->isVisible());
    }

    void ruleMessagesMergeAccountsAndKeepDisabledSetting()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        DesktopPetSettingsStore store(directory.path());
        QImage image(64, 64, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::green);
        QVERIFY(store.saveSourceImage(image));
        const QString message = QStringLiteral("条件已经满足");
        QVERIFY(store.save(QJsonObject{
            {QStringLiteral("enabled"), false},
            {QStringLiteral("triggerMessages"), QJsonObject{
                {QStringLiteral("1"), message}, {QStringLiteral("2"), message}}}}));
        {
            DesktopPetPage page(directory.path());
            ColorRule first;
            first.number = 1;
            ColorRule second;
            second.number = 2;
            page.setColorRules({first, second});
            auto* pet = findPetWindow();
            QVERIFY(pet);
            QVERIFY(!pet->isVisible());
            page.showRuleMessage(1, QStringLiteral("EVE - Alpha"));
            page.showRuleMessage(2, QStringLiteral("EVE - Beta"));
            page.showRuleMessage(1, QStringLiteral("EVE - Alpha"));
            auto* bubble = pet->findChild<QLabel*>(QStringLiteral("desktopPetSpeechBubble"));
            QVERIFY(bubble);
            QCOMPARE(bubble->text(), QStringLiteral("Alpha、Beta：条件已经满足"));
            auto* timer = pet->findChild<QTimer*>();
            QVERIFY(timer);
            timer->stop();
            QVERIFY(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection));
            QVERIFY(!pet->isVisible());
        }
        const auto saved = store.load();
        QVERIFY(saved.loaded);
        QVERIFY(!saved.settings.value(QStringLiteral("enabled")).toBool());
        QCOMPARE(saved.settings.value(QStringLiteral("triggerMessages")).toObject().size(), 2);
    }
};

QTEST_MAIN(DesktopPetWidgetsTest)
#include "desktop_pet_widgets_test.moc"
