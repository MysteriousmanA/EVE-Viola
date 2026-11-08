#include <QtTest>
#include <QUuid>

#include "platform/single_instance.h"

class SingleInstanceTest final : public QObject {
    Q_OBJECT

private slots:
    void primaryCanReceiveActivationFromSecondary()
    {
        const QString key = QStringLiteral("EVE-Viola.SingleInstanceTest.%1")
                                .arg(QUuid::createUuid().toString(QUuid::Id128));
        SingleInstance primary(key);
        QCOMPARE(primary.state(), SingleInstance::State::Primary);

        int activations = 0;
        primary.setActivationHandler([&activations] { ++activations; });
        SingleInstance secondary(key);
        QCOMPARE(secondary.state(), SingleInstance::State::Secondary);
        QVERIFY(secondary.notifyPrimary());
        QTRY_COMPARE(activations, 1);
    }

    void applicationScopedKeyIsStableAndDirectorySpecific()
    {
        const QString first =
            QStringLiteral("D:/Apps/EVE-Viola-one/EVE-Viola.exe");
        const QString equivalent =
            QStringLiteral("d:/apps/EVE-VIOLA-ONE/EVE-VIOLA.exe");
        const QString second =
            QStringLiteral("D:/Apps/EVE-Viola-two/EVE-Viola.exe");

        const QString firstKey = singleInstanceKeyForApplication(first);
        QCOMPARE(firstKey, singleInstanceKeyForApplication(equivalent));
        QVERIFY(firstKey != singleInstanceKeyForApplication(second));
        QVERIFY(firstKey.startsWith(QStringLiteral("EVE-Viola.SingleInstance.v2.")));
    }
};

QTEST_MAIN(SingleInstanceTest)

#include "single_instance_test.moc"
