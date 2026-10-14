#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QDateTimeEdit>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QtTest>

#include "ui/expedition_timing_page.h"

class ExpeditionTimingPageTest final : public QObject {
    Q_OBJECT

private slots:
    void defaultsMatchReferenceModule()
    {
        ExpeditionTimingPage page;

        auto* factionGroup = page.findChild<QButtonGroup*>(
            QStringLiteral("expeditionFactionGroup"));
        auto* typeGroup = page.findChild<QButtonGroup*>(
            QStringLiteral("expeditionTypeGroup"));
        auto* deltaDays = page.findChild<QSpinBox*>(
            QStringLiteral("expeditionDeltaDays"));
        auto* deltaHours = page.findChild<QSpinBox*>(
            QStringLiteral("expeditionDeltaHours"));
        auto* deltaMinutes = page.findChild<QSpinBox*>(
            QStringLiteral("expeditionDeltaMinutes"));

        QVERIFY(factionGroup != nullptr);
        QVERIFY(typeGroup != nullptr);
        QVERIFY(deltaDays != nullptr);
        QVERIFY(deltaHours != nullptr);
        QVERIFY(deltaMinutes != nullptr);
        QCOMPARE(factionGroup->buttons().size(), 6);
        QCOMPARE(typeGroup->buttons().size(), 12);
        QCOMPARE(factionGroup->checkedButton()->text(), QStringLiteral("兔子"));
        QCOMPARE(typeGroup->checkedButton()->text(), QStringLiteral("10-10"));
        QCOMPARE(deltaDays->value(), 0);
        QCOMPARE(deltaHours->value(), 23);
        QCOMPARE(deltaMinutes->value(), 59);
    }

    void capitalTypesDefaultToTwoDaysTwentyThreeHoursFiftyNineMinutes()
    {
        ExpeditionTimingPage page;
        auto* deltaDays = page.findChild<QSpinBox*>(
            QStringLiteral("expeditionDeltaDays"));
        auto* deltaHours = page.findChild<QSpinBox*>(
            QStringLiteral("expeditionDeltaHours"));
        auto* deltaMinutes = page.findChild<QSpinBox*>(
            QStringLiteral("expeditionDeltaMinutes"));
        auto* dreadnought = page.findChild<QRadioButton*>(
            QStringLiteral("expeditionTypeCapitalDreadnought"));
        auto* shieldMothership = page.findChild<QRadioButton*>(
            QStringLiteral("expeditionTypeShieldMothership"));
        auto* standard = page.findChild<QRadioButton*>(
            QStringLiteral("expeditionType10_10"));

        QVERIFY(deltaDays != nullptr);
        QVERIFY(deltaHours != nullptr);
        QVERIFY(deltaMinutes != nullptr);
        QVERIFY(dreadnought != nullptr);
        QVERIFY(shieldMothership != nullptr);
        QVERIFY(standard != nullptr);

        dreadnought->click();
        QCOMPARE(deltaDays->value(), 2);
        QCOMPARE(deltaHours->value(), 23);
        QCOMPARE(deltaMinutes->value(), 59);
        shieldMothership->click();
        QCOMPARE(deltaDays->value(), 2);
        QCOMPARE(deltaHours->value(), 23);
        QCOMPARE(deltaMinutes->value(), 59);
        standard->click();
        QCOMPARE(deltaDays->value(), 0);
        QCOMPARE(deltaHours->value(), 23);
        QCOMPARE(deltaMinutes->value(), 59);
    }

    void calculationFormatsAndCopiesExpiry()
    {
        ExpeditionTimingPage page;
        auto* start = page.findChild<QDateTimeEdit*>(
            QStringLiteral("expeditionStartTime"));
        auto* deltaHours = page.findChild<QSpinBox*>(
            QStringLiteral("expeditionDeltaHours"));
        auto* deltaMinutes = page.findChild<QSpinBox*>(
            QStringLiteral("expeditionDeltaMinutes"));
        auto* calculate = page.findChild<QPushButton*>(
            QStringLiteral("calculateExpeditionExpiry"));
        auto* copy = page.findChild<QPushButton*>(
            QStringLiteral("copyExpeditionResult"));
        auto* result = page.findChild<QLabel*>(
            QStringLiteral("expeditionResultValue"));

        QVERIFY(start != nullptr);
        QVERIFY(deltaHours != nullptr);
        QVERIFY(deltaMinutes != nullptr);
        QVERIFY(calculate != nullptr);
        QVERIFY(copy != nullptr);
        QVERIFY(result != nullptr);
        QVERIFY(!copy->isEnabled());

        start->setDateTime(QDateTime(QDate(2026, 12, 31), QTime(23, 30)));
        page.findChild<QSpinBox*>(QStringLiteral("expeditionDeltaDays"))->setValue(0);
        deltaHours->setValue(1);
        deltaMinutes->setValue(45);
        calculate->click();

        const QString expected = QStringLiteral("兔子10-10 1月1日1时15分过期");
        QCOMPARE(result->text(), expected);
        QVERIFY(copy->isEnabled());

        copy->click();
        QCOMPARE(QApplication::clipboard()->text(), expected);
    }
};

QTEST_MAIN(ExpeditionTimingPageTest)

#include "expedition_timing_page_test.moc"
