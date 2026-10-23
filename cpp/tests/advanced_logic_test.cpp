#include <QtTest>
#include <algorithm>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include "config/app_config.h"
#include "features/advanced_logic/advanced_logic_engine.h"
#include "features/advanced_logic/advanced_logic_service.h"
#include "ui/pages/advanced_logic/advanced_logic_page.h"
#include "ui/pages/color_rule/color_rule_dialog.h"
#include "ui/theme.h"

namespace {
QList<ColorRule> rules()
{
    QList<ColorRule> result;
    for (int number = 1; number <= 3; ++number) {
        ColorRule rule;
        rule.number = number;
        rule.note = QStringLiteral("事件 %1").arg(number);
        result.append(rule);
    }
    return result;
}

AdvancedLogicConfig config()
{
    WhitelistSequence sequence;
    sequence.id = QStringLiteral("evade");
    sequence.name = QStringLiteral("规避");
    sequence.timeoutMs = 100;
    sequence.nodes = {{1, WhitelistNodeEffect::Keep, {}}, {2, WhitelistNodeEffect::Suppress, {}}};
    return {true, {sequence}};
}

AdvancedLogicEvent logicEvent(quint64 id, int rule, QString source = QStringLiteral("alpha"),
    ColorRulePriority priority = ColorRulePriority::Normal)
{
    return {id, id, source, source, rule, priority};
}

bool decision(const QList<AdvancedLogicDecision>& values, quint64 id, bool execute)
{
    return std::any_of(values.cbegin(), values.cend(), [&](const auto& value) {
        return value.eventId == id && value.execute == execute;
    });
}
}

class AdvancedLogicTest final : public QObject {
    Q_OBJECT
private slots:
    void matchesMixedNodeEffectsAndConsumesOnce()
    {
        AdvancedLogicEngine engine;
        engine.configure(config(), rules());
        QVERIFY(engine.submit(logicEvent(1, 1), 0).isEmpty());
        const auto matched = engine.submit(logicEvent(2, 2), 50);
        QCOMPARE(matched.size(), 2);
        QVERIFY(decision(matched, 1, true));
        QVERIFY(decision(matched, 2, false));
        QVERIFY(decision(engine.submit(logicEvent(3, 2), 60), 3, true));
        QCOMPARE(engine.nextDeadline(), qint64{-1});
    }

    void immediateRuleBypassesSuppressionAndStillMatches()
    {
        auto settings = config();
        settings.sequences[0].nodes[0].effect = WhitelistNodeEffect::Suppress;
        AdvancedLogicEngine engine;
        engine.configure(settings, rules());
        const auto first = engine.submit(logicEvent(1, 1, QStringLiteral("alpha"), ColorRulePriority::Immediate), 0);
        QCOMPARE(first.size(), 1);
        QVERIFY(decision(first, 1, true));
        const auto matched = engine.submit(logicEvent(2, 2), 1);
        QCOMPARE(matched.size(), 1);
        QVERIFY(decision(matched, 2, false));
        QVERIFY(engine.advance(1000).isEmpty());
    }

    void immediateFinalNodeAlsoCannotBeSuppressed()
    {
        AdvancedLogicEngine engine;
        engine.configure(config(), rules());
        engine.submit(logicEvent(1, 1), 0);
        const auto matched = engine.submit(logicEvent(2, 2, QStringLiteral("alpha"), ColorRulePriority::Immediate), 10);
        QCOMPARE(matched.first().eventId, quint64{2});
        QVERIFY(decision(matched, 2, true));
        QVERIFY(decision(matched, 1, true));
    }

    void unmatchedEventsExecuteImmediatelyAndTimeoutRestoresEffects()
    {
        auto settings = config();
        settings.sequences[0].nodes[0].effect = WhitelistNodeEffect::Suppress;
        AdvancedLogicEngine engine;
        engine.configure(settings, rules());
        QVERIFY(decision(engine.submit(logicEvent(1, 2), 0), 1, true));
        QVERIFY(engine.submit(logicEvent(2, 1), 10).isEmpty());
        QVERIFY(engine.advance(110).isEmpty());
        QVERIFY(decision(engine.advance(111), 2, true));
        QVERIFY(decision(engine.submit(logicEvent(3, 2), 112), 3, true));
    }

    void sourceScopeAndTitleFilterPreventUnrelatedAccountsFromConsuming()
    {
        auto settings = config();
        settings.sequences[0].nodes[1].sourceTitle = QStringLiteral("alpha");
        AdvancedLogicEngine engine;
        engine.configure(settings, rules());
        engine.submit(logicEvent(1, 1), 0);
        QVERIFY(decision(engine.submit(logicEvent(2, 2, QStringLiteral("beta")), 1), 2, true));
        QVERIFY(decision(engine.submit(logicEvent(3, 2), 2), 3, false));
        settings.sequences[0].sameSource = false;
        settings.sequences[0].nodes[1].sourceTitle.clear();
        engine.configure(settings, rules());
        engine.submit(logicEvent(4, 1), 10);
        QVERIFY(decision(engine.submit(logicEvent(5, 2, QStringLiteral("beta")), 11), 5, false));
    }

    void strictOrderBreaksOnUnrelatedEventsWhilePermissiveOrderWaits()
    {
        auto settings = config();
        settings.sequences[0].ignoreUnrelated = false;
        AdvancedLogicEngine engine;
        engine.configure(settings, rules());
        engine.submit(logicEvent(1, 1), 0);
        const auto broken = engine.submit(logicEvent(2, 3), 1);
        QVERIFY(decision(broken, 1, true));
        QVERIFY(decision(broken, 2, true));
        QVERIFY(decision(engine.submit(logicEvent(3, 2), 2), 3, true));
        settings.sequences[0].ignoreUnrelated = true;
        engine.configure(settings, rules());
        engine.submit(logicEvent(4, 1), 10);
        QVERIFY(decision(engine.submit(logicEvent(5, 3), 11), 5, true));
        QVERIFY(decision(engine.submit(logicEvent(6, 2), 12), 6, false));
    }

    void overlappingSequencesResolveByListOrderWithoutDoubleConsumption()
    {
        auto settings = config();
        auto second = settings.sequences.first();
        second.id = QStringLiteral("second");
        second.nodes[1].effect = WhitelistNodeEffect::Keep;
        settings.sequences.append(second);
        AdvancedLogicEngine engine;
        engine.configure(settings, rules());
        engine.submit(logicEvent(1, 1), 0);
        const auto matched = engine.submit(logicEvent(2, 2), 10);
        QCOMPARE(matched.size(), 2);
        QVERIFY(decision(matched, 2, false));
        QVERIFY(engine.advance(1000).isEmpty());
    }

    void winningSequenceDoesNotConsumeAnotherSequencesUnmatchedPrefix()
    {
        auto settings = config();
        auto second = settings.sequences.first();
        second.id = QStringLiteral("work-finished");
        second.nodes[0].ruleNumber = 3;
        settings.sequences.append(second);
        AdvancedLogicEngine engine;
        engine.configure(settings, rules());
        engine.submit(logicEvent(1, 1, QStringLiteral("alpha"), ColorRulePriority::Immediate), 0);
        engine.submit(logicEvent(2, 3, QStringLiteral("alpha"), ColorRulePriority::Immediate), 1);
        QVERIFY(decision(engine.submit(logicEvent(3, 2), 2), 3, false));
        QVERIFY(decision(engine.submit(logicEvent(4, 2), 3), 4, false));
        QVERIFY(decision(engine.submit(logicEvent(5, 2), 4), 5, true));
    }

    void cancellationAndReconfigurationReleaseRemainingOrdinaryEvents()
    {
        auto settings = config();
        settings.sequences[0].nodes.append({3, WhitelistNodeEffect::Suppress, {}});
        AdvancedLogicEngine engine;
        engine.configure(settings, rules());
        engine.submit(logicEvent(1, 1), 0);
        engine.submit(logicEvent(2, 2), 1);
        const auto cancelled = engine.cancelOwner(1);
        QCOMPARE(cancelled.size(), 1);
        QVERIFY(decision(cancelled, 2, true));
        engine.submit(logicEvent(3, 1), 10);
        settings.enabled = false;
        QVERIFY(decision(engine.configure(settings, rules()), 3, true));
        QVERIFY(decision(engine.submit(logicEvent(4, 1), 11), 4, true));
    }

    void deletedRulesDisableTheEntireSequence()
    {
        auto available = rules();
        available.removeAt(1);
        AdvancedLogicEngine engine;
        engine.configure(config(), available);
        QVERIFY(decision(engine.submit(logicEvent(1, 1), 0), 1, true));
        QCOMPARE(engine.nextDeadline(), qint64{-1});
    }

    void higherPriorityIsReleasedFirst()
    {
        auto settings = config();
        settings.sequences[0].nodes[1].effect = WhitelistNodeEffect::Keep;
        AdvancedLogicEngine engine;
        engine.configure(settings, rules());
        engine.submit(logicEvent(1, 1), 0);
        const auto matched = engine.submit(logicEvent(2, 2, QStringLiteral("alpha"), ColorRulePriority::High), 1);
        QCOMPARE(matched.first().eventId, quint64{2});
    }

    void serviceUsesTimerAndDoesNotReplayImmediateEvents()
    {
        auto available = rules();
        available[0].priority = ColorRulePriority::Immediate;
        AdvancedLogicService service;
        service.setConfiguration(config(), available);
        QObject first, second;
        QList<bool> results;
        service.submit(&first, 1, QStringLiteral("alpha"), QStringLiteral("alpha"), 1,
            [&](bool execute) { results.append(execute); });
        QCOMPARE(results, QList<bool>{true});
        service.submit(&second, 2, QStringLiteral("alpha"), QStringLiteral("alpha"), 2,
            [&](bool execute) { results.append(execute); });
        QCOMPARE(results, (QList<bool>{true, false}));
        QTest::qWait(120);
        QCOMPARE(results.size(), 2);
        available[0].priority = ColorRulePriority::Normal;
        service.setConfiguration(config(), available);
        service.submit(&first, 1, QStringLiteral("alpha"), QStringLiteral("alpha"), 1,
            [&](bool execute) { results.append(execute); });
        QCOMPARE(results.size(), 2);
        QTRY_COMPARE_WITH_TIMEOUT(results.size(), 3, 500);
        QCOMPARE(results.last(), true);
    }

    void destroyedOwnersCannotLeaveHistoryThatSuppressesAnotherWindow()
    {
        auto available = rules();
        available[0].priority = ColorRulePriority::Immediate;
        AdvancedLogicService service;
        service.setConfiguration(config(), available);
        auto* first = new QObject;
        QObject second;
        int effects = 0;
        service.submit(first, 1, QStringLiteral("alpha"), QStringLiteral("alpha"), 1,
            [&](bool execute) { effects += execute ? 1 : 0; });
        QCOMPARE(effects, 1);
        delete first;
        service.submit(&second, 2, QStringLiteral("alpha"), QStringLiteral("alpha"), 2,
            [&](bool execute) { effects += execute ? 1 : 0; });
        QCOMPARE(effects, 2);
    }

    void editingColorRulesCancelsPendingEffectsWhileDisablingWhitelistReleasesThem()
    {
        AdvancedLogicService service;
        auto available = rules();
        auto settings = config();
        service.setConfiguration(settings, available);
        QObject owner;
        QList<bool> results;
        const auto submit = [&] {
            service.submit(&owner, 1, QStringLiteral("alpha"), QStringLiteral("alpha"), 1,
                [&](bool execute) { results.append(execute); });
        };
        submit();
        QVERIFY(results.isEmpty());
        available[0].note = QStringLiteral("changed rule");
        service.setConfiguration(settings, available);
        QCOMPARE(results, QList<bool>{false});
        submit();
        settings.enabled = false;
        service.setConfiguration(settings, available);
        QCOMPARE(results, (QList<bool>{false, true}));
        QTest::qWait(120);
        QCOMPARE(results.size(), 2);
    }

    void configRoundTripsAndCorruptAdvancedConfigDoesNotOverwriteOtherSections()
    {
        QTemporaryDir directory;
        JsonConfigStore store(directory.path());
        AppConfig expected;
        expected.advancedLogic = config();
        expected.colorRules = rules();
        expected.colorRules[0].priority = ColorRulePriority::Immediate;
        QString error;
        QVERIFY2(store.save(expected, &error), qPrintable(error));
        AppConfig actual;
        QVERIFY2(store.load(actual, &error), qPrintable(error));
        QVERIFY(actual.advancedLogic == expected.advancedLogic);
        QVERIFY(actual.colorRules == expected.colorRules);
        QFile rulesFile(directory.filePath(QStringLiteral("colorRules.json")));
        QVERIFY(rulesFile.open(QIODevice::ReadOnly));
        const auto original = rulesFile.readAll();
        rulesFile.close();
        QFile advancedFile(directory.filePath(QStringLiteral("advancedLogic.json")));
        QVERIFY(advancedFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        advancedFile.write("{broken");
        advancedFile.close();
        expected.colorRules.clear();
        QVERIFY(!store.save(expected, &error));
        QVERIFY(!store.load(actual, &error));
        QVERIFY(rulesFile.open(QIODevice::ReadOnly));
        QCOMPARE(rulesFile.readAll(), original);
    }

    void pageSavesNodePoliciesAndRulePriorityWithoutLosingExistingSettings()
    {
        AdvancedLogicPage page;
        auto available = rules();
        page.setConfiguration(config(), available);
        const auto previewPath = qEnvironmentVariable("EVE_ADVANCED_LOGIC_PREVIEW_PATH");
        if (!previewPath.isEmpty()) {
            page.setStyleSheet(kAppStyle);
            page.resize(1050, 720);
            page.show();
            QCoreApplication::processEvents();
            QVERIFY(page.grab().save(previewPath));
        }
        QSignalSpy changed(&page, &AdvancedLogicPage::configurationChanged);
        QSignalSpy priorityChanged(&page, &AdvancedLogicPage::priorityChanged);
        auto* priorities = page.findChild<QTableWidget*>(QStringLiteral("colorRulePriorityTable"));
        QVERIFY(priorities);
        qobject_cast<QComboBox*>(priorities->cellWidget(0, 1))->setCurrentIndex(2);
        QCOMPARE(priorityChanged.size(), 1);
        QCOMPARE(qvariant_cast<ColorRulePriority>(priorityChanged.first()[1]), ColorRulePriority::Immediate);
        auto* table = page.findChild<QTableWidget*>(QStringLiteral("whitelistSequenceTable"));
        table->selectRow(0);
        QTimer::singleShot(0, [] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            dialog->findChild<QLineEdit*>(QStringLiteral("sequenceName"))->setText(QStringLiteral("修改后的规避"));
            auto* nodes = dialog->findChild<QTableWidget*>(QStringLiteral("whitelistNodes"));
            QCOMPARE(nodes->rowCount(), 2);
            const auto preview = qEnvironmentVariable("EVE_ADVANCED_LOGIC_DIALOG_PREVIEW_PATH");
            if (!preview.isEmpty()) { QVERIFY(dialog->grab().save(preview)); }
            qobject_cast<QComboBox*>(nodes->cellWidget(0, 2))->setCurrentIndex(1);
            dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
        });
        page.findChild<QPushButton*>(QStringLiteral("editWhitelistSequence"))->click();
        QCOMPARE(changed.size(), 1);
        const auto sequence = page.configuration().sequences.first();
        QCOMPARE(sequence.name, QStringLiteral("修改后的规避"));
        QCOMPARE(sequence.nodes[0].effect, WhitelistNodeEffect::Suppress);
        QCOMPARE(sequence.nodes[1].effect, WhitelistNodeEffect::Suppress);
        QCOMPARE(sequence.timeoutMs, 100);
    }

    void editingColorRulePreservesImmediatePriority()
    {
        ColorRule initial = rules().first();
        initial.priority = ColorRulePriority::Immediate;
        QTimer::singleShot(0, [] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            auto* priority = dialog->findChild<QComboBox*>(QStringLiteral("colorRulePriority"));
            QVERIFY(priority);
            QCOMPARE(priority->currentIndex(), 2);
            dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
        });
        const auto edited = promptColorRule(nullptr, initial);
        QVERIFY(edited);
        QCOMPARE(edited->priority, ColorRulePriority::Immediate);
    }
};

QTEST_MAIN(AdvancedLogicTest)
#include "advanced_logic_test.moc"
