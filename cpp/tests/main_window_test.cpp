#include <cstdint>
#include <cstring>
#include <cwchar>
#include <vector>

#include <QtTest>

#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QDateTimeEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QFile>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QMediaPlayer>
#include <QMessageBox>
#include <QMenu>
#include <memory>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTabBar>
#include <QTabWidget>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>

#include "config/app_config.h"
#include "ui/audio_feedback_manager.h"
#include "features/replica/statistics_coordinator.h"
#include "ui/main_window.h"
#include "ui/account_switch_controller.h"
#include "ui/radial_switch_overlay.h"
#include "ui/window_color_logic_runtime.h"
#include "ui/logic_binding_runtime.h"
#include "ui/logic_binding_source.h"
#include "features/color_rule/logic_binding_plan.h"
#include "ui/pages/logic_list/logic_list_page.h"
#include "features/quick_switch/quick_switch_service.h"
#include "ui/desktop_pet_page.h"
#include "ui/desktop_pet/floating_pet_window.h"
#include "ui/page_partition_page.h"
#include "ui/replica_row.h"
#include "ui/replica_window.h"
#include "ui/pixel_color_editor.h"
#include "ui/pages/color_rule/color_rule_dialog.h"
#include "ui/pages/color_rule/color_rule_controller.h"
#include "ui/pages/color_rule/replica_choice_label.h"
#include "ui/pages/region_preset/region_preset_controller.h"
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace {
int logicDataRow(QTableWidget* table, int index)
{
    for (int row = 0; row < table->rowCount(); ++row) {
        if (qobject_cast<QCheckBox*>(table->cellWidget(row, 0)) && index-- == 0) { return row; }
    }
    return -1;
}


class ControlledLogicAudioChannel final : public AudioChannelBackend {
public:
    using AudioChannelBackend::AudioChannelBackend;
    quint64 attempt{};
    bool playFile(quint64 id, const QString&) override
    {
        attempt = id;
        emit playbackStarted(id);
        return true;
    }
    void stopPlayback() override {}
};

EveWindowInfo mockWindows[2]{};
std::uint32_t mockWindowCount = 0;

int mockActivationCount = 0;
int mockCaptureWidth = 0;
int mockCaptureHeight = 0;
std::vector<QRgb> mockCaptureColors;

bool lastThumbnailVisible = true;
int mockSourceWidth = 1920;
int mockSourceHeight = 1080;
std::int32_t mockUpdateStatus = 0;

std::uint32_t version() { return 3; }
std::int32_t enumerate(EveWindowInfo** items, std::uint32_t* count)
{
    *items = mockWindowCount == 0 ? nullptr : mockWindows;
    *count = mockWindowCount;
    return 0;
}
void freeList(EveWindowInfo*) {}
std::int32_t create(std::uintptr_t, std::uintptr_t, std::uintptr_t* handle)
{
    *handle = 1;
    return 0;
}
std::int32_t sourceSize(std::uintptr_t, std::int32_t* width, std::int32_t* height)
{
    *width = mockSourceWidth;
    *height = mockSourceHeight;
    return 0;
}
std::int32_t update(
    std::uintptr_t,
    EveRect,
    EveRect,
    std::uint8_t,
    std::int32_t visible)
{
    lastThumbnailVisible = visible != 0;
    return mockUpdateStatus;
}
std::int32_t destroy(std::uintptr_t) { return 0; }
std::int32_t activate(std::uintptr_t hwnd, std::uintptr_t* observedForegroundHwnd)
{
    ++mockActivationCount;
    *observedForegroundHwnd = hwnd;
    return 0;
}
std::int32_t capture(std::uintptr_t, std::int32_t* width, std::int32_t* height, std::uint8_t** pixels)
{
    if (mockCaptureWidth <= 0 || mockCaptureHeight <= 0 || mockCaptureColors.empty()) {
        return -2;
    }
    *width = mockCaptureWidth;
    *height = mockCaptureHeight;
    const auto rowBytes = static_cast<std::size_t>(mockCaptureWidth) * 4;
    auto* buffer = new std::uint8_t[rowBytes * static_cast<std::size_t>(mockCaptureHeight)];
    std::memcpy(buffer, mockCaptureColors.data(), rowBytes * static_cast<std::size_t>(mockCaptureHeight));
    *pixels = buffer;
    return 0;
}
void freePixels(std::uint8_t* pixels) { delete[] pixels; }

NativeFunctions functions()
{
    return {version, enumerate, freeList, create, sourceSize, update, destroy, activate, capture, freePixels};
}

void setMockWindow(int index, std::uintptr_t hwnd, const wchar_t* title)
{
    mockWindows[index].hwnd = hwnd;
    std::wmemcpy(mockWindows[index].title, title, std::wcslen(title) + 1);
}

void bindGlobalRule(MainWindow& manager, ReplicaWindow* window, ColorRule rule)
{
    auto* runtime = manager.findChild<WindowColorLogicRuntime*>();
    const QString id = runtime->idForWindow(window);
    rule.windowConditions = {rule.triggerCondition1};
    rule.windowConditions[0].replicaId = id;
    rule.targetReplicaId = id;
    runtime->configure({rule});
    runtime->attach(id, window);
}

QString freshConfigPath()
{
    static QTemporaryDir tempDir;
    static int counter = 0;
    return tempDir.filePath(QStringLiteral("cache-%1").arg(++counter));
}

} // namespace

class MainWindowTest final : public QObject {
    Q_OBJECT

private slots:
    void radialCandidatesExcludeCurrentRecentAndInactiveAccounts()
    {
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        auto* accounts = manager.findChild<AccountSwitchController*>();
        QVERIFY(accounts);
        accounts->radialSwitchForegroundHwnd_ = 1;
        const QList<WindowInfo> clients{{1, "EVE - A"}, {2, "EVE - B"},
            {3, "EVE - C"}, {4, "EVE - D"}};
        accounts->setAccountTriggers({{1, {"#1 当前"}}, {2, {"#2 飘逸预警"}}});
        QCOMPARE(accounts->triggerCandidates(clients), QList<WindowInfo>{clients[1]});
        accounts->lastVisited_[2] = accounts->clock_.elapsed();
        QVERIFY(accounts->triggerCandidates(clients).isEmpty());
        accounts->lastVisited_[2] -= 11000;
        QCOMPARE(accounts->triggerCandidates(clients), QList<WindowInfo>{clients[1]});
        auto* recentSeconds = manager.findChild<QSpinBox*>(QStringLiteral("radialSwitchRecentSeconds"));
        QVERIFY(recentSeconds);
        QCOMPARE(recentSeconds->value(), 10);
        recentSeconds->setValue(20);
        QVERIFY(accounts->triggerCandidates(clients).isEmpty());
        accounts->lastVisited_[2] -= 10000;
        QCOMPARE(accounts->triggerCandidates(clients), QList<WindowInfo>{clients[1]});
        accounts->setAccountTriggers({{3, {"#3 异常"}}, {4, {"#4 速度"}}});
        QCOMPARE(accounts->triggerCandidates(clients), (QList<WindowInfo>{clients[2], clients[3]}));
        accounts->setAccountTriggers({});
        QVERIFY(accounts->triggerCandidates(clients).isEmpty());
        manager.close();
    }

    void globalLogicKeepsAccountReasonsForTenSecondsAfterTrigger()
    {
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        auto* a = manager.createReplica();
        auto* b = manager.createReplica();
        a->selectSource({123, QStringLiteral("EVE - A")});
        b->selectSource({456, QStringLiteral("EVE - B")});
        auto* runtime = manager.findChild<WindowColorLogicRuntime*>();
        auto* accounts = manager.findChild<AccountSwitchController*>();
        ColorRule first; first.number = 10; first.definitionNumber = 3;
        first.note = QStringLiteral("飘逸预警"); first.targetReplicaId = "reason-A";
        first.postTriggerAction = PostTriggerAction::ReverseWait;
        first.windowConditions = {{QStringLiteral("红色"), QStringLiteral(">=2"), -1, "reason-A"}};
        ColorRule second = first; second.number = 11; second.definitionNumber = 4;
        second.note = QStringLiteral("速度预警"); second.targetReplicaId = "reason-B";
        second.windowConditions[0].replicaId = "reason-B";
        runtime->configure({first, second});
        runtime->attach("reason-A", a); runtime->attach("reason-B", b);
        emit a->colorStatisticsChanged({10, 2, 0});
        QCOMPARE(accounts->accountTriggers_.value(123), QStringList{QStringLiteral("#3 飘逸预警")});
        emit b->colorStatisticsChanged({10, 2, 0});
        QCOMPARE(accounts->accountTriggers_.size(), 2);
        accounts->overlay()->setTriggerLabels(accounts->accountTriggers_);
        QCOMPARE(accounts->overlay()->triggerLabels(456), QStringList{QStringLiteral("#4 速度预警")});
        emit a->colorStatisticsChanged({10, 0, 0});
        emit b->pixelStatisticsUnavailable();
        QCOMPARE(accounts->accountTriggers_.size(), 2); // Feedback/condition ending does not erase the event.
        runtime->recentAccountTriggers_[10].expiresAt = runtime->clock_.elapsed() + 1000;
        runtime->refreshAccountTriggers();
        QVERIFY(accounts->accountTriggers_.contains(123));
        runtime->recentAccountTriggers_[10].expiresAt = runtime->clock_.elapsed();
        runtime->refreshAccountTriggers();
        QVERIFY(!accounts->accountTriggers_.contains(123));
        QVERIFY(accounts->accountTriggers_.contains(456));
        runtime->recentAccountTriggers_[11].expiresAt = runtime->clock_.elapsed();
        runtime->refreshAccountTriggers();
        QVERIFY(accounts->accountTriggers_.isEmpty());
        manager.close();
    }

    void editorsKeepReplicaInputAvailable_data()
    {
        QTest::addColumn<int>("editor");
        QTest::newRow("color-rule") << 0;
        QTest::newRow("statistics-colors") << 1;
        QTest::newRow("region-preset-name") << 2;
    }

    void editorsKeepReplicaInputAvailable()
    {
        QFETCH(int, editor);
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        auto* replica = manager.createReplica();
        replica->selectSource({123, QStringLiteral("EVE - Alpha")});
        replica->setStatisticsInterval(0);
        manager.show();
        replica->show();
        QTest::qWait(30);

        bool found = false;
        bool windowModal = false;
        bool ownerBlocked = false;
        bool replicaEnabled = false;
        bool editorStayedOpen = false;
        const int before = mockActivationCount;
        QTimer::singleShot(0, [&] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (!dialog) { return; }
            found = true;
            windowModal = dialog->windowModality() == Qt::WindowModal;
#ifdef Q_OS_WIN
            ownerBlocked = !IsWindowEnabled(reinterpret_cast<HWND>(manager.winId()));
            replicaEnabled = IsWindowEnabled(reinterpret_cast<HWND>(replica->winId()));
#else
            ownerBlocked = windowModal;
            replicaEnabled = replica->isEnabled();
#endif
            // The QWindow overload follows Qt's window-system input routing.
            QTest::mouseClick(replica->windowHandle(), Qt::LeftButton,
                Qt::NoModifier, replica->rect().center());
            editorStayedOpen = dialog->isVisible();
            dialog->reject();
        });
        if (editor == 0) {
            QVERIFY(!promptColorRule(&manager, {}, {}, {}).has_value());
        } else if (editor == 1) {
            QVERIFY(!promptStatisticsColors(&manager, defaultPixelColorTargets()).has_value());
        } else {
            AppConfig config;
            config.regionPresets = {{QStringLiteral("Original"), QRect(0, 0, 10, 10)}};
            RegionPresetController controller(config, &manager);
            controller.renameRegionPresetAt(0);
            QCOMPARE(config.regionPresets.first().name, QStringLiteral("Original"));
        }
        QVERIFY(found);
        QVERIFY(windowModal);
        QVERIFY(ownerBlocked);
        QVERIFY(replicaEnabled);
        QVERIFY(editorStayedOpen);
        QCOMPARE(mockActivationCount, before + 1);
    }

    void logicSourcesResolveByProcessAndAccountAfterRestart()
    {
        LogicBindingConfig binding;
        binding.sourceTitle = QStringLiteral("EVE - Alice");
        binding.sourceProcessName = QStringLiteral("EXEFILE.EXE");
        QList<WindowInfo> sources{{123, binding.sourceTitle, "unrelated.exe"},
            {456, QStringLiteral("EVE - Bob"), "exefile.exe"},
            {789, binding.sourceTitle, "exefile.exe"}};
        const auto* resolved = resolveLogicBindingSource(binding, sources);
        QVERIFY(resolved);
        QCOMPARE(resolved->hwnd, std::uintptr_t{789});
        sources[2].hwnd = 999; // Same process and account, new game session.
        resolved = resolveLogicBindingSource(binding, sources);
        QVERIFY(resolved);
        QCOMPARE(resolved->hwnd, std::uintptr_t{999});
        sources.append({1000, binding.sourceTitle, "exefile.exe"});
        QVERIFY(!resolveLogicBindingSource(binding, sources)); // Ambiguous account, never guess.
        sources.removeLast();
        sources.removeLast();
        QVERIFY(!resolveLogicBindingSource(binding, sources)); // Matching title in another process is insufficient.
    }

    void logicListLongPressSelectsAccounts_data()
    {
        QTest::addColumn<int>("mode");
        QTest::newRow("selected-account") << 0;
        QTest::newRow("select-all") << 1;
        QTest::newRow("cancel") << 2;
        QTest::newRow("no-accounts") << 3;
    }

    void logicListLongPressSelectsAccounts()
    {
        QFETCH(int, mode);
        LogicListPage page;
        ColorRule rule; rule.number = 7; rule.note = QStringLiteral("告警");
        const QList<WindowInfo> sources{{123, "EVE - A"}, {456, "EVE - B"}};
        page.setConfiguration({rule}, {}, mode == 3 ? QList<WindowInfo>{} : sources, {});
        page.show();
        auto* add = page.findChild<QPushButton*>(QStringLiteral("addLogicBinding"));
        QSignalSpy single(&page, &LogicListPage::addRequested);
        QSignalSpy batch(&page, &LogicListPage::addForSourcesRequested);
        QTest::mouseClick(add, Qt::LeftButton);
        QCOMPARE(single.size(), 1);
        single.clear();
        bool dialogSeen = false;
        bool initiallyDisabled = false;
        QTimer::singleShot(800, &page, [&] {
            auto* dialog = page.findChild<QDialog*>(QStringLiteral("logicSourceSelectionDialog"));
            if (!dialog) { return; }
            dialogSeen = true;
            auto* choices = dialog->findChild<QListWidget*>(QStringLiteral("logicSourceChoices"));
            auto* buttons = dialog->findChild<QDialogButtonBox*>();
            initiallyDisabled = !buttons->button(QDialogButtonBox::Ok)->isEnabled();
            QTest::mouseRelease(add, Qt::LeftButton);
            if (mode == 0) {
                choices->item(1)->setCheckState(Qt::Checked);
                buttons->button(QDialogButtonBox::Ok)->click();
            } else if (mode == 1) {
                dialog->findChild<QCheckBox*>(QStringLiteral("selectAllLogicSources"))->click();
                buttons->button(QDialogButtonBox::Ok)->click();
            } else {
                buttons->button(QDialogButtonBox::Cancel)->click();
            }
        });
        QTest::mousePress(add, Qt::LeftButton);
        QTest::qWait(950);
        QVERIFY(dialogSeen);
        QVERIFY(initiallyDisabled);
        QCOMPARE(single.size(), 0);
        QCOMPARE(batch.size(), mode < 2 ? 1 : 0);
        if (mode < 2) {
            QCOMPARE(batch.first()[0].toInt(), 7);
            QCOMPARE(batch.first()[1].value<QList<WindowInfo>>(), mode == 0 ? QList<WindowInfo>{sources[1]} : sources);
        }
        QTest::mouseClick(add, Qt::LeftButton);
        QCOMPARE(single.size(), 1);
    }

    void logicListBatchAdditionBindsAndPersistsEachAccount()
    {
        QTemporaryDir directory;
        AppConfig config;
        config.regionPresets = {{QStringLiteral("护盾"), QRect(0, 0, 2, 1)}};
        const auto preset = logicRegionPresetId(QStringLiteral("护盾"));
        ColorRule rule; rule.number = 1; rule.targetReplicaId = preset;
        rule.windowConditions = {{QStringLiteral("红色"), QStringLiteral(">=2"), -1, preset}};
        config.colorRules = {rule};
        JsonConfigStore store(directory.path()); QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));
        setMockWindow(0, 123, L"EVE - A"); setMockWindow(1, 456, L"EVE - B"); mockWindowCount = 2;
        NativeGateway gateway(functions());
        {
            MainWindow manager(gateway, directory.path());
            auto* page = manager.findChild<LogicListPage*>(); QVERIFY(page);
            emit page->addForSourcesRequested(1, {{123, "EVE - A"}, {456, "EVE - B"}});
            QCoreApplication::processEvents();
            auto* table = page->findChild<QTableWidget*>(QStringLiteral("logicBindingTable"));
            QCOMPARE(table->rowCount(), 4);
            for (int row = 0; row < 2; ++row) {
                QVERIFY(table->item(logicDataRow(table, row), 3)->text().contains(QStringLiteral("已绑定")));
            }
            manager.close();
        }
        mockWindowCount = 0;
        AppConfig saved; QVERIFY2(store.load(saved, &error), qPrintable(error));
        QCOMPARE(saved.logicBindings.size(), 2);
        QCOMPARE(saved.replicas.size(), 2);
        QVERIFY(saved.logicBindings[0].id != saved.logicBindings[1].id);
        for (int i = 0; i < 2; ++i) {
            QCOMPARE(saved.logicBindings[i].ruleNumber, 1);
            QCOMPARE(saved.logicBindings[i].sourceProcessName, QStringLiteral("exefile.exe"));
            QCOMPARE(saved.logicBindings[i].sourceTitle, i == 0 ? QStringLiteral("EVE - A") : QStringLiteral("EVE - B"));
            QCOMPARE(saved.logicBindings[i].windowIds.size(), 1);
        }
    }

    void gameProcessChangesPreserveExistingLogicState()
    {
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        auto* first = manager.createReplica();
        auto* existing = manager.createReplica();
        const WindowInfo sourceA{123, "EVE - A"};
        const WindowInfo sourceB{456, "EVE - B"};
        first->selectSource(sourceA); existing->selectSource(sourceB);
        first->setStatisticsInterval(1500); existing->setStatisticsInterval(1500);
        ReplicaConfig templateWindow; templateWindow.id = "template";
        templateWindow.capturedRegion = QRect(0, 0, 2, 1);
        ColorRule rule; rule.number = 7;
        rule.windowConditions = {{QStringLiteral("红色"), ">=2", -1, "template"}};
        rule.postTriggerAction = PostTriggerAction::ReverseWait;
        LogicBindingConfig a; a.id = "a"; a.ruleNumber = 7; a.sourceTitle = sourceA.title;
        a.windowIds.insert("template", "first");
        auto b = a; b.id = "b"; b.sourceTitle = sourceB.title;
        b.windowIds.insert("template", "existing");
        auto* runtime = manager.findChild<LogicBindingRuntime*>();
        QSignalSpy triggered(existing, &ReplicaWindow::colorLogicTriggered);
        const QHash<QString, ReplicaWindow*> replicas{{"first", first}, {"existing", existing}};
        const auto configure = [&](const QList<WindowInfo>& sources, bool force = false) {
            runtime->configure({rule}, {a, b}, {templateWindow}, sources, replicas, force);
        };
        configure({sourceB});
        emit existing->colorStatisticsChanged({10, 2, 0});
        QCOMPARE(triggered.size(), 1);
        configure({sourceA, sourceB}); // Earlier binding becomes available and shifts list positions.
        emit existing->colorStatisticsChanged({10, 2, 0});
        QCOMPARE(triggered.size(), 1);
        configure({sourceB}); // Earlier game process closes.
        emit existing->colorStatisticsChanged({10, 2, 0});
        QCOMPARE(triggered.size(), 1);
        configure({sourceB, sourceA}, true);
        emit existing->colorStatisticsChanged({10, 2, 0});
        QCOMPARE(triggered.size(), 1);
        emit existing->colorStatisticsChanged({10, 0, 0});
        emit existing->colorStatisticsChanged({10, 2, 0});
        QCOMPARE(triggered.size(), 2); // A real false-to-true edge still triggers.
        manager.close();
    }

    void logicInstancesShareOneSourceAndRemainIndependent()
    {
        QTemporaryDir directory;
        JsonConfigStore store(directory.path());
        AppConfig config;
        ReplicaConfig a; a.id = "A"; a.capturedRegion = QRect(0, 0, 2, 1); a.visible = false;
        ReplicaConfig b; b.id = "B"; b.capturedRegion = QRect(2, 0, 2, 1); b.visible = false;
        config.replicas = {a, b};
        config.regionPresets = {{QStringLiteral("护盾"), a.capturedRegion}, {QStringLiteral("本地"), b.capturedRegion}};
        ColorRule rule; rule.number = 1;
        rule.windowConditions = {{QStringLiteral("红色"), QStringLiteral(">=2"), -1, "A"},
            {QStringLiteral("白色"), QStringLiteral("<2"), -1, "B"}};
        rule.targetReplicaId = "A"; rule.postTriggerAction = PostTriggerAction::ReverseWait;
        config.colorRules = {rule};
        LogicBindingConfig x; x.id = "X"; x.ruleNumber = 1; x.sourceTitle = QStringLiteral("EVE - A");
        LogicBindingConfig y = x; y.id = "Y"; y.sourceTitle = QStringLiteral("EVE - B");
        config.logicBindings = {x, y};
        setMockWindow(0, 123, L"EVE - A"); setMockWindow(1, 456, L"EVE - B"); mockWindowCount = 2;
        QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));
        NativeGateway gateway(functions());
        {
            MainWindow manager(gateway, directory.path());
            auto* runtime = manager.findChild<LogicBindingRuntime*>();
            const auto first = runtime->windowsForBinding("X");
            const auto second = runtime->windowsForBinding("Y");
            QCOMPARE(first.size(), 2); QCOMPARE(second.size(), 2);
            ReplicaWindow* xa = nullptr; ReplicaWindow* xb = nullptr;
            ReplicaWindow* ya = nullptr; ReplicaWindow* yb = nullptr;
            for (auto* window : first) {
                QCOMPARE(window->source()->hwnd, std::uintptr_t{123});
                if (window->capturedRegion() == std::optional<QRect>(a.capturedRegion)) { xa = window; } else { xb = window; }
            }
            for (auto* window : second) {
                QCOMPARE(window->source()->hwnd, std::uintptr_t{456});
                if (window->capturedRegion() == std::optional<QRect>(a.capturedRegion)) { ya = window; } else { yb = window; }
            }
            QVERIFY(xa && xb && ya && yb);
            QSignalSpy firstTriggered(xa, &ReplicaWindow::colorLogicTriggered);
            QSignalSpy secondTriggered(ya, &ReplicaWindow::colorLogicTriggered);
            emit xa->colorStatisticsChanged({10, 2, 0});
            emit yb->colorStatisticsChanged({10, 0, 1});
            QCOMPARE(firstTriggered.size(), 0); // Samples from different instances cannot combine.
            QCOMPARE(secondTriggered.size(), 0);
            emit xb->colorStatisticsChanged({10, 0, 1});
            QCOMPARE(firstTriggered.size(), 1);
            QCOMPARE(firstTriggered.first()[0].toInt(), 1);
            emit ya->colorStatisticsChanged({10, 2, 0});
            QCOMPARE(secondTriggered.size(), 1);
            QCOMPARE(secondTriggered.first()[0].toInt(), 1);
            emit xa->colorStatisticsChanged({10, 2, 0});
            QCOMPARE(firstTriggered.size(), 1);
            QCOMPARE(secondTriggered.size(), 1);
            manager.close();
        }
        AppConfig saved;
        QVERIFY2(store.load(saved, &error), qPrintable(error));
        migrateLogicRegionReferences(config);
        QCOMPARE(saved.colorRules.first(), config.colorRules.first());
        QCOMPARE(saved.logicBindings.size(), config.logicBindings.size());
        for (int i = 0; i < saved.logicBindings.size(); ++i) {
            auto binding = saved.logicBindings[i];
            QCOMPARE(binding.windowIds.size(), 2);
            binding.windowIds.clear();
            QCOMPARE(binding, config.logicBindings[i]);
        }
        QCOMPARE(saved.replicas.size(), 4); // Reuse the two unbound windows for the first source.
        mockWindowCount = 0;
    }

    void logicListBindsAllTemplateWindowsBySelectingOneSource()
    {
        QTemporaryDir directory;
        AppConfig config;
        ReplicaConfig replica; replica.id = "template"; replica.capturedRegion = QRect(0, 0, 2, 1); replica.visible = false;
        config.replicas = {replica}; config.regionPresets = {{QStringLiteral("护盾"), replica.capturedRegion}};
        ColorRule rule; rule.number = 1; rule.note = QStringLiteral("告警"); rule.targetReplicaId = replica.id;
        rule.windowConditions = {{QStringLiteral("红色"), QStringLiteral(">=2"), -1, replica.id}};
        config.colorRules = {rule};
        JsonConfigStore store(directory.path()); QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));
        setMockWindow(0, 123, L"EVE - A"); mockWindowCount = 1;
        NativeGateway gateway(functions()); MainWindow manager(gateway, directory.path());
        auto* page = manager.findChild<LogicListPage*>(); QVERIFY(page);
        page->findChild<QPushButton*>(QStringLiteral("addLogicBinding"))->click();
        QCoreApplication::processEvents();
        auto* table = page->findChild<QTableWidget*>(QStringLiteral("logicBindingTable"));
        QCOMPARE(table->rowCount(), 2);
        auto* source = qobject_cast<QComboBox*>(table->cellWidget(logicDataRow(table, 0), 2)); QVERIFY(source);
        source->setCurrentIndex(1); emit source->activated(1);
        QCoreApplication::processEvents();
        QVERIFY(table->item(logicDataRow(table, 0), 3)->text().contains(QStringLiteral("已绑定")));
        manager.close();
        AppConfig saved; QVERIFY2(store.load(saved, &error), qPrintable(error));
        QCOMPARE(saved.logicBindings.size(), 1);
        QCOMPARE(saved.logicBindings.first().sourceTitle, QStringLiteral("EVE - A"));
        QCOMPARE(saved.colorRules.first().windowConditions.first().replicaId, logicRegionPresetId(QStringLiteral("护盾")));
        mockWindowCount = 0;
    }

    void logicListFiltersBySourceAndLogicAndKeepsOriginalSourceData()
    {
        LogicListPage page;
        ColorRule warning; warning.number = 1; warning.note = QStringLiteral("飘逸预警");
        ColorRule reverse; reverse.number = 2; reverse.note = QStringLiteral("反预警");
        LogicBindingConfig a; a.id = "a"; a.ruleNumber = 1; a.sourceTitle = "EVE - Alice";
        LogicBindingConfig b = a; b.id = "b"; b.ruleNumber = 2;
        LogicBindingConfig c = a; c.id = "c"; c.sourceTitle = "EVE - Bob";
        LogicBindingConfig unbound = a; unbound.id = "unbound"; unbound.sourceTitle.clear();
        const QList<LogicBindingConfig> bindings{a, b, c, unbound};
        const QList<WindowInfo> sources{{123, a.sourceTitle}, {456, c.sourceTitle}};
        page.setConfiguration({warning, reverse}, bindings, sources, {});
        auto* table = page.findChild<QTableWidget*>(QStringLiteral("logicBindingTable"));
        auto* sourceFilter = page.findChild<QComboBox*>(QStringLiteral("logicSourceFilter"));
        auto* logicFilter = page.findChild<QComboBox*>(QStringLiteral("logicNameFilter"));
        QVERIFY(sourceFilter); QVERIFY(logicFilter);
        auto* source = qobject_cast<QComboBox*>(table->cellWidget(logicDataRow(table, 0), 2)); QVERIFY(source);
        QCOMPARE(source->currentText(), QStringLiteral("Alice"));
        QVERIFY(!source->currentText().contains(QStringLiteral("EVE - ")));
        QCOMPARE(source->currentData().value<WindowInfo>().title, a.sourceTitle);
        QSignalSpy changed(&page, &LogicListPage::sourceChanged);
        source->setCurrentIndex(2); emit source->activated(2);
        QCOMPARE(changed.size(), 1);
        QCOMPARE(changed.first()[0].toString(), a.id);
        QCOMPARE(changed.first()[1].value<WindowInfo>().title, c.sourceTitle);
        QCOMPARE(sourceFilter->itemText(sourceFilter->findData(a.sourceTitle)), QStringLiteral("Alice"));
        sourceFilter->setCurrentIndex(sourceFilter->findData(a.sourceTitle));
        QVERIFY(!table->isRowHidden(logicDataRow(table, 0))); QVERIFY(!table->isRowHidden(logicDataRow(table, 1))); QVERIFY(table->isRowHidden(logicDataRow(table, 2)));
        logicFilter->setCurrentIndex(logicFilter->findData(2));
        QVERIFY(table->isRowHidden(logicDataRow(table, 0))); QVERIFY(!table->isRowHidden(logicDataRow(table, 1)));
        page.setConfiguration({warning, reverse}, bindings, {}, {{"a", QStringLiteral("失联")}});
        QCOMPARE(sourceFilter->currentData().toString(), a.sourceTitle);
        QCOMPARE(logicFilter->currentData().toInt(), 2);
        QVERIFY(table->isRowHidden(logicDataRow(table, 0))); QVERIFY(!table->isRowHidden(logicDataRow(table, 1)));
        source = qobject_cast<QComboBox*>(table->cellWidget(logicDataRow(table, 0), 2));
        QCOMPARE(source->currentText(), QStringLiteral("暂未找到 · Alice"));
        page.findChild<QPushButton*>(QStringLiteral("resetLogicFilters"))->click();
        for (int row = 0; row < table->rowCount(); ++row) { QVERIFY(!table->isRowHidden(row)); }
        sourceFilter->setCurrentIndex(sourceFilter->findData(QStringLiteral("")));
        QVERIFY(table->isRowHidden(logicDataRow(table, 0))); QVERIFY(!table->isRowHidden(logicDataRow(table, 3)));
        sourceFilter->setCurrentIndex(0);
        logicFilter->setCurrentIndex(logicFilter->findData(1));
        QVERIFY(!table->isRowHidden(logicDataRow(table, 0))); QVERIFY(table->isRowHidden(logicDataRow(table, 1))); QVERIFY(!table->isRowHidden(logicDataRow(table, 2)));
        page.setConfiguration({warning}, {a}, sources, {});
        QCOMPARE(logicFilter->currentData().toInt(), 1);
        QVERIFY(!table->isRowHidden(logicDataRow(table, 0)));
    }

    void logicListGroupsCollapseAndRetainStateAcrossRefresh()
    {
        LogicListPage page;
        ColorRule warning; warning.number = 1;
        ColorRule reverse; reverse.number = 2;
        LogicBindingConfig a; a.id = "a"; a.ruleNumber = 1; a.sourceTitle = "EVE - Alice";
        LogicBindingConfig b = a; b.id = "b"; b.ruleNumber = 2;
        LogicBindingConfig c = a; c.id = "c"; c.sourceTitle = "EVE - Bob";
        const QList<LogicBindingConfig> bindings{c, a, b};
        const QList<WindowInfo> sources{{123, a.sourceTitle}, {456, c.sourceTitle}};
        page.setConfiguration({warning, reverse}, bindings, sources, {});
        auto* table = page.findChild<QTableWidget*>(QStringLiteral("logicBindingTable"));
        QCOMPARE(table->rowCount(), 5);
        auto* alice = qobject_cast<QPushButton*>(table->cellWidget(0, 0));
        auto* bob = qobject_cast<QPushButton*>(table->cellWidget(3, 0));
        QVERIFY(alice); QVERIFY(bob);
        QVERIFY(alice->text().contains(QStringLiteral("Alice · 2 条逻辑")));
        QVERIFY(bob->text().contains(QStringLiteral("Bob · 1 条逻辑")));
        alice->click();
        QVERIFY(!table->isRowHidden(0));
        QVERIFY(table->isRowHidden(1)); QVERIFY(table->isRowHidden(2));
        QVERIFY(!table->isRowHidden(4));
        auto* filter = page.findChild<QComboBox*>(QStringLiteral("logicNameFilter"));
        filter->setCurrentIndex(filter->findData(2));
        QVERIFY(alice->text().contains(QStringLiteral("1 条逻辑")));
        QVERIFY(table->isRowHidden(3)); QVERIFY(table->isRowHidden(4));
        // Status updates and a temporarily missing source must not expand a group.
        page.setConfiguration({warning, reverse}, bindings, {}, {{"a", QStringLiteral("失联")}});
        QVERIFY(table->isRowHidden(1)); QVERIFY(table->isRowHidden(2));
        alice = qobject_cast<QPushButton*>(table->cellWidget(0, 0));
        alice->click();
        QVERIFY(table->isRowHidden(1)); QVERIFY(!table->isRowHidden(2));
        page.findChild<QPushButton*>(QStringLiteral("resetLogicFilters"))->click();
        page.findChild<QPushButton*>(QStringLiteral("collapseAllLogicGroups"))->click();
        QVERIFY(table->isRowHidden(1)); QVERIFY(table->isRowHidden(2));
        QVERIFY(table->isRowHidden(4));
        page.findChild<QPushButton*>(QStringLiteral("expandAllLogicGroups"))->click();
        for (int row = 0; row < table->rowCount(); ++row) { QVERIFY(!table->isRowHidden(row)); }
        QSignalSpy enabled(&page, &LogicListPage::enabledChanged);
        auto* check = page.findChild<QCheckBox*>(QStringLiteral("logicBindingEnabled_b"));
        QVERIFY(check); check->click();
        QCOMPARE(enabled.size(), 1); QCOMPARE(enabled.first()[0].toString(), b.id);
        QCOMPARE(enabled.first()[1].toBool(), false);
    }

    void logicListReusesExistingWindowsAndPersistsFineTuning()
    {
        QTemporaryDir directory;
        AppConfig config;
        const auto presetId = logicRegionPresetId(QStringLiteral("护盾"));
        ReplicaConfig replica; replica.id = "existing";
        replica.sourceTitle = QStringLiteral("EVE - A"); replica.capturedRegion = QRect(0, 0, 2, 1);
        replica.position = QPoint(210, 220); replica.size = QSize(180, 90);
        config.replicas = {replica};
        config.regionPresets = {{QStringLiteral("护盾"), replica.capturedRegion}};
        ColorRule rule; rule.number = 1;
        rule.windowConditions = {{QStringLiteral("红色"), QStringLiteral(">=2"), -1, presetId}};
        config.colorRules = {rule};
        LogicBindingConfig binding; binding.id = "first"; binding.ruleNumber = 1;
        binding.sourceTitle = replica.sourceTitle;
        config.logicBindings = {binding};
        JsonConfigStore store(directory.path()); QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));
        setMockWindow(0, 123, L"EVE - A"); setMockWindow(1, 456, L"EVE - B"); mockWindowCount = 2;
        NativeGateway gateway(functions());
        QString secondId;
        {
            MainWindow manager(gateway, directory.path());
            auto* runtime = manager.findChild<LogicBindingRuntime*>();
            QCOMPARE(manager.replicaWindows().size(), 1);
            const auto first = runtime->windowsForBinding("first"); QCOMPARE(first.size(), 1);
            QCOMPARE(first.first()->size(), replica.size);
            first.first()->setCapturedRegion(QRect(0, 0, 1, 1));
            auto* page = manager.findChild<LogicListPage*>();
            page->findChild<QPushButton*>(QStringLiteral("addLogicBinding"))->click();
            QCoreApplication::processEvents();
            QCOMPARE(manager.replicaWindows().size(), 1);
            auto* table = page->findChild<QTableWidget*>(QStringLiteral("logicBindingTable"));
            auto* source = qobject_cast<QComboBox*>(table->cellWidget(logicDataRow(table, 1), 2)); QVERIFY(source);
            source->setCurrentIndex(1); emit source->activated(1);
            QCoreApplication::processEvents();
            QCOMPARE(manager.replicaWindows().size(), 1);
            manager.close();
        }
        AppConfig saved; QVERIFY2(store.load(saved, &error), qPrintable(error));
        QCOMPARE(saved.replicas.size(), 1);
        QCOMPARE(saved.logicBindings.size(), 2);
        for (const auto& item : saved.logicBindings) { QCOMPARE(item.windowIds.value(presetId), replica.id); }
        secondId = saved.logicBindings.last().id;
        {
            MainWindow manager(gateway, directory.path());
            auto* runtime = manager.findChild<LogicBindingRuntime*>();
            QCOMPARE(manager.replicaWindows().size(), 1);
            QCOMPARE(runtime->windowsForBinding("first"), runtime->windowsForBinding(secondId));
            QCOMPARE(runtime->windowsForBinding("first").first()->capturedRegion(),
                std::optional<QRect>(QRect(0, 0, 1, 1)));
            auto* page = manager.findChild<LogicListPage*>();
            auto* table = page->findChild<QTableWidget*>(QStringLiteral("logicBindingTable"));
            auto* source = qobject_cast<QComboBox*>(table->cellWidget(logicDataRow(table, 1), 2)); QVERIFY(source);
            source->setCurrentIndex(2); emit source->activated(2);
            QCoreApplication::processEvents();
            QCOMPARE(manager.replicaWindows().size(), 2);
            QCOMPARE(runtime->windowsForBinding("first").first()->source()->hwnd, std::uintptr_t{123});
            QCOMPARE(runtime->windowsForBinding(secondId).first()->source()->hwnd, std::uintptr_t{456});
            manager.close();
        }
        mockWindowCount = 0;
    }

    void cachedLogicWindowsSurviveSourceRestartWithoutDuplicates_data()
    {
        QTest::addColumn<bool>("restoreWithGame");
        QTest::newRow("restore-on-startup") << false;
        QTest::newRow("restore-with-game") << true;
    }

    void cachedLogicWindowsSurviveSourceRestartWithoutDuplicates()
    {
        QFETCH(bool, restoreWithGame);
        QTemporaryDir directory;
        AppConfig config;
        config.restoreWindowsWithGame = restoreWithGame;
        const auto presetId = logicRegionPresetId(QStringLiteral("护盾"));
        config.regionPresets = {{QStringLiteral("护盾"), QRect(0, 0, 2, 1)}};
        ColorRule rule; rule.number = 1;
        rule.windowConditions = {{QStringLiteral("红色"), QStringLiteral(">=2"), -1, presetId}};
        config.colorRules = {rule};
        ReplicaConfig replica; replica.id = "saved-window";
        replica.sourceTitle = QStringLiteral("EVE - A");
        replica.capturedRegion = QRect(0, 0, 1, 1); // A saved fine-tuned region.
        replica.position = QPoint(210, 220); replica.size = QSize(180, 180);
        config.replicas = {replica};
        LogicBindingConfig binding; binding.id = "saved-binding"; binding.ruleNumber = 1;
        binding.sourceTitle = replica.sourceTitle;
        binding.windowIds.insert(presetId, replica.id);
        config.logicBindings = {binding};
        JsonConfigStore store(directory.path()); QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));
        NativeGateway gateway(functions());
        for (const auto hwnd : {456, 789, 123}) {
            setMockWindow(0, hwnd, L"EVE - A"); mockWindowCount = 1;
            MainWindow manager(gateway, directory.path());
            auto* page = manager.findChild<LogicListPage*>(); QVERIFY(page);
            for (int refresh = 0; refresh < 3; ++refresh) {
                emit page->refreshRequested();
                QCoreApplication::processEvents();
                QCOMPARE(manager.replicaWindows().size(), 1);
            }
            auto* window = manager.replicaWindows().first();
            QVERIFY(window->source());
            QCOMPARE(window->source()->hwnd, std::uintptr_t(hwnd));
            QCOMPARE(window->capturedRegion(), std::optional<QRect>(replica.capturedRegion));
            QCOMPARE(window->size(), replica.size);
            QCOMPARE(window->pos(), replica.position);
            manager.close();
            AppConfig saved; QVERIFY2(store.load(saved, &error), qPrintable(error));
            QCOMPARE(saved.replicas.size(), 1);
            QCOMPARE(saved.replicas.first().id, replica.id);
            QCOMPARE(saved.logicBindings.first().windowIds.value(presetId), replica.id);
            QCOMPARE(saved.logicBindings.first().sourceProcessName, QStringLiteral("exefile.exe"));
        }
        mockWindowCount = 0;
    }

    void globalLogicIgnoresDisabledWindowStatistics()
    {
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        auto* a = manager.createReplica();
        auto* b = manager.createReplica();
        a->selectSource({123, QStringLiteral("EVE - A")});
        b->selectSource({456, QStringLiteral("EVE - B")});
        auto* runtime = manager.findChild<WindowColorLogicRuntime*>();
        ColorRule rule;
        rule.number = 10; rule.targetReplicaId = "test-A";
        rule.windowConditions = {{QStringLiteral("红色"), QStringLiteral(">=2"), -1, "test-A"},
            {QStringLiteral("白色"), QStringLiteral("<2"), -1, "test-B"}};
        rule.postTriggerAction = PostTriggerAction::ReverseWait;
        runtime->configure({rule});
        runtime->attach("test-A", a); runtime->attach("test-B", b);
        QSignalSpy triggered(a, &ReplicaWindow::colorLogicTriggered);
        emit a->colorStatisticsChanged({10, 2, 0});
        b->setStatisticsInterval(0);
        emit b->colorStatisticsChanged({10, 0, 1});
        QCOMPARE(triggered.size(), 0);
        b->setStatisticsInterval(1500);
        emit b->colorStatisticsChanged({10, 0, 1});
        QCOMPARE(triggered.size(), 1);
        manager.close();
    }

    void logicBindingFlashesSelectedBordersForOnlyTheTriggeredAccount()
    {
        QTemporaryDir directory;
        AppConfig config;
        const QRect sampling(0, 0, 2, 1), firstBorder(2, 0, 2, 1), secondBorder(4, 0, 2, 1);
        config.regionPresets = {{QStringLiteral("采样"), sampling},
            {QStringLiteral("边框一"), firstBorder}, {QStringLiteral("边框二"), secondBorder}};
        ColorRule rule; rule.number = 1;
        rule.postTriggerAction = PostTriggerAction::Loop;
        rule.windowConditions = {{QStringLiteral("红色"), QStringLiteral(">=2"), -1,
            logicRegionPresetId(QStringLiteral("采样"))}};
        rule.windowSwitches = {{logicRegionPresetId(QStringLiteral("边框一")), QColor(46, 204, 64), 0},
            {logicRegionPresetId(QStringLiteral("边框二")), QColor(80, 160, 255), 0}};
        config.colorRules = {rule};
        LogicBindingConfig a; a.id = "A"; a.ruleNumber = 1; a.sourceTitle = "EVE - A";
        LogicBindingConfig b = a; b.id = "B"; b.sourceTitle = "EVE - B";
        config.logicBindings = {a, b};
        JsonConfigStore store(directory.path()); QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));
        setMockWindow(0, 123, L"EVE - A"); setMockWindow(1, 456, L"EVE - B"); mockWindowCount = 2;
        NativeGateway gateway(functions());
        MainWindow manager(gateway, directory.path());
        mockWindowCount = 0;
        auto* bindings = manager.findChild<LogicBindingRuntime*>();
        const auto windowsA = bindings->windowsForBinding("A");
        const auto windowsB = bindings->windowsForBinding("B");
        QCOMPARE(windowsA.size(), 3); QCOMPARE(windowsB.size(), 3);
        const auto findSampling = [&](const QList<ReplicaWindow*>& windows) -> ReplicaWindow* {
            for (auto* window : windows) {
                if (window->capturedRegion() == std::optional<QRect>(sampling)) { return window; }
            }
            return nullptr;
        };
        auto* sampleA = findSampling(windowsA); auto* sampleB = findSampling(windowsB);
        QVERIFY(sampleA); QVERIFY(sampleB);
        for (auto* window : windowsA + windowsB) { QVERIFY(!window->isFlashActive()); }
        emit sampleA->colorStatisticsChanged({10, 2, 0});
        for (auto* window : windowsA) {
            QCOMPARE(window->isFlashActive(), window != sampleA);
            if (window != sampleA) {
                QCOMPARE(window->borderColor(), window->capturedRegion() == std::optional<QRect>(firstBorder)
                    ? QColor(46, 204, 64) : QColor(80, 160, 255));
            }
        }
        for (auto* window : windowsB) { QVERIFY(!window->isFlashActive()); }
        emit sampleB->colorStatisticsChanged({10, 2, 0});
        for (auto* window : windowsB) { QCOMPARE(window->isFlashActive(), window != sampleB); }
        emit sampleA->colorStatisticsChanged({10, 0, 0});
        for (auto* window : windowsA) { QVERIFY(!window->isFlashActive()); }
        for (auto* window : windowsB) { QCOMPARE(window->isFlashActive(), window != sampleB); }
        emit sampleB->pixelStatisticsUnavailable();
        for (auto* window : windowsB) { QVERIFY(!window->isFlashActive()); }
        manager.close();
    }

    void separatelyListedStatesShareExecutionWhenMiddleClicked()
    {
        QTemporaryDir directory;
        AppConfig config;
        config.regionPresets = {{QStringLiteral("本地"), QRect(0, 0, 2, 1)}};
        const auto preset = logicRegionPresetId(QStringLiteral("本地"));
        ColorRule warning; warning.number = 31;
        warning.windowConditions = {{QStringLiteral("红色"), QStringLiteral(">=2"), -1, preset}};
        warning.windowSwitches = {{preset, QColor(46, 204, 64), 32}};
        ColorRule reverse = warning; reverse.number = 32;
        reverse.windowConditions[0].expression = QStringLiteral("<2");
        reverse.windowSwitches = {{preset, QColor(80, 160, 255), 31}};
        reverse.postTriggerAction = PostTriggerAction::Jump; reverse.jumpToRule = 31;
        config.colorRules = {warning, reverse};
        for (const auto& source : QList<WindowInfo>{{123, "EVE - A"}, {456, "EVE - B"}}) {
            for (int number : {31, 32}) {
                LogicBindingConfig binding; binding.ruleNumber = number;
                binding.sourceTitle = source.title;
                config.logicBindings.append(binding);
            }
        }
        JsonConfigStore store(directory.path()); QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));
        setMockWindow(0, 123, L"EVE - A"); setMockWindow(1, 456, L"EVE - B"); mockWindowCount = 2;
        NativeGateway gateway(functions());
        MainWindow manager(gateway, directory.path());
        mockWindowCount = 0;
        auto* bindings = manager.findChild<LogicBindingRuntime*>();
        const auto a = bindings->windowsForBinding(config.logicBindings[0].id);
        const auto b = bindings->windowsForBinding(config.logicBindings[2].id);
        QCOMPARE(a.size(), 1); QCOMPARE(b.size(), 1);
        QCOMPARE(bindings->windowsForBinding(config.logicBindings[1].id), a);
        QCOMPARE(bindings->windowsForBinding(config.logicBindings[3].id), b);
        auto* first = a.first(); auto* second = b.first();
        QSignalSpy firstTriggered(first, &ReplicaWindow::colorLogicTriggered);
        QSignalSpy secondTriggered(second, &ReplicaWindow::colorLogicTriggered);
        emit first->colorStatisticsChanged({10, 0, 0});
        QCOMPARE(firstTriggered.size(), 0); // Reverse is a state, not another active detector.
        emit first->colorStatisticsChanged({10, 2, 0});
        QCOMPARE(firstTriggered.size(), 1);
        QCOMPARE(firstTriggered.last()[0].toInt(), 31);
        QTest::mouseClick(first, Qt::MiddleButton);
        QCOMPARE(first->borderColor(), QColor(80, 160, 255));
        QCOMPARE(second->borderColor(), QColor(46, 204, 64));
        emit first->colorStatisticsChanged({10, 2, 0});
        QCOMPARE(firstTriggered.size(), 1); // Old conditions cannot execute after switching.
        emit second->colorStatisticsChanged({10, 2, 0});
        QCOMPARE(secondTriggered.size(), 1);
        QCOMPARE(secondTriggered.last()[0].toInt(), 31);
        emit first->colorStatisticsChanged({10, 0, 0});
        QCOMPARE(firstTriggered.size(), 2);
        QCOMPARE(firstTriggered.last()[0].toInt(), 32);
        QCOMPARE(first->borderColor(), QColor(46, 204, 64)); // Triggered jump shares the same state.
        emit first->colorStatisticsChanged({10, 2, 0});
        QCOMPARE(firstTriggered.size(), 3);
        QCOMPARE(firstTriggered.last()[0].toInt(), 31);
        QTest::mouseClick(first, Qt::MiddleButton);
        QTest::mouseClick(first, Qt::MiddleButton);
        emit first->colorStatisticsChanged({10, 0, 0});
        QCOMPARE(firstTriggered.size(), 3);
        emit first->colorStatisticsChanged({10, 2, 0});
        QCOMPARE(firstTriggered.size(), 4);
        QCOMPARE(firstTriggered.last()[0].toInt(), 31);
        manager.close();
    }

    void reverseWaitStopsFlashingAfterFeedback_data()
    {
        QTest::addColumn<int>("feedback");
        QTest::newRow("none") << static_cast<int>(TriggerFeedback::None);
        QTest::newRow("audio") << static_cast<int>(TriggerFeedback::Audio);
        QTest::newRow("popup") << static_cast<int>(TriggerFeedback::Popup);
    }

    void reverseWaitStopsFlashingAfterFeedback()
    {
        QFETCH(int, feedback);
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        auto* sample = manager.createReplica();
        auto* border = manager.createReplica();
        sample->selectSource({123, QStringLiteral("EVE - A")});
        border->selectSource({123, QStringLiteral("EVE - A")});
        sample->hide();
        QList<ControlledLogicAudioChannel*> channels;
        AudioFeedbackManager audio([&](QObject* parent) {
            auto* channel = new ControlledLogicAudioChannel(parent);
            channels.append(channel);
            return channel;
        });
        WindowColorLogicRuntime runtime(audio);
        ColorRule rule; rule.number = 71; rule.targetReplicaId = "sample";
        rule.postTriggerAction = PostTriggerAction::ReverseWait;
        rule.triggerFeedback = static_cast<TriggerFeedback>(feedback);
        rule.triggerFeedbackAudio = QStringLiteral("controlled-feedback.wav");
        rule.windowConditions = {{QStringLiteral("红色"), QStringLiteral(">=2"), -1, "sample"}};
        const QColor color(46, 204, 64);
        rule.windowSwitches = {{"border", color, 0}};
        runtime.configure({rule});
        runtime.attach("sample", sample); runtime.attach("border", border);
        bool publishedBeforeFeedback = false;
        connect(sample, &ReplicaWindow::colorLogicTriggered, &runtime, [&] {
            publishedBeforeFeedback = runtime.accountTriggers_.contains(123);
        });
        QSignalSpy triggered(sample, &ReplicaWindow::colorLogicTriggered);
        const auto completeFeedback = [&] {
            if (rule.triggerFeedback == TriggerFeedback::Audio) {
                emit channels.first()->playbackFinished(channels.first()->attempt);
            } else if (rule.triggerFeedback == TriggerFeedback::Popup) {
                QTest::mouseClick(sample, Qt::LeftButton);
            }
        };
        emit sample->colorStatisticsChanged({10, 2, 0});
        QCOMPARE(triggered.size(), 1);
        QVERIFY(publishedBeforeFeedback);
        QVERIFY(runtime.accountTriggers_.contains(123)); // Eligible while feedback is still pending.
        const auto remaining = runtime.recentAccountTriggers_.value(71).expiresAt - runtime.clock_.elapsed();
        QVERIFY(remaining > 9500 && remaining <= 10000);
        QCOMPARE(border->isFlashActive(), rule.triggerFeedback != TriggerFeedback::None);
        completeFeedback();
        QTRY_VERIFY(!border->isFlashActive());
        QVERIFY(runtime.accountTriggers_.contains(123));
        QCOMPARE(border->borderColor(), color);
        emit sample->colorStatisticsChanged({10, 2, 0});
        QCOMPARE(triggered.size(), 1); // Completion cannot rearm while the condition stays true.
        QVERIFY(!border->isFlashActive());
        emit sample->colorStatisticsChanged({10, 0, 0});
        emit sample->colorStatisticsChanged({10, 2, 0});
        QCOMPARE(triggered.size(), 2);
        QCOMPARE(border->isFlashActive(), rule.triggerFeedback != TriggerFeedback::None);
        completeFeedback();
        QTRY_VERIFY(!border->isFlashActive());
        QCOMPARE(border->borderColor(), color);
        runtime.stop();
        manager.close();
    }

    void sharedLogicBorderAndMiddleClickSwitchBetweenRules()
    {
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        auto* window = manager.createReplica();
        window->selectSource({123, QStringLiteral("EVE - A")});
        auto* runtime = manager.findChild<WindowColorLogicRuntime*>();

        ColorRule warning;
        warning.number = 31;
        warning.targetReplicaId = "test-A";
        warning.windowConditions = {{QStringLiteral("红色"), QStringLiteral(">=2"), -1, "test-A"}};
        warning.windowSwitches = {{"test-A", QColor(46, 204, 64), 32}};
        warning.postTriggerAction = PostTriggerAction::ReverseWait;
        ColorRule reverse = warning;
        reverse.number = 32;
        reverse.enabled = false;
        reverse.windowConditions[0].expression = QStringLiteral("<2");
        reverse.windowSwitches = {{"test-A", QColor(80, 160, 255), 0}};

        runtime->configure({warning, reverse}, {{"test-A", {31, 32}}});
        runtime->attach("test-A", window);
        QSignalSpy triggered(window, &ReplicaWindow::colorLogicTriggered);
        QCOMPARE(window->borderColor(), QColor(46, 204, 64));
        QVERIFY(!window->isFlashActive());
        emit window->colorStatisticsChanged({10, 0, 0});
        QCOMPARE(triggered.size(), 0);
        QCOMPARE(window->borderColor(), QColor(46, 204, 64));
        QVERIFY(!window->isFlashActive());

        emit window->colorStatisticsChanged({10, 2, 0});
        QCOMPARE(triggered.size(), 1);
        QCOMPARE(triggered.first()[0].toInt(), 31);
        QCOMPARE(window->borderColor(), QColor(46, 204, 64));
        QVERIFY(!window->isFlashActive());
        emit window->colorStatisticsChanged({10, 0, 0});
        QVERIFY(!window->isFlashActive());
        QCOMPARE(window->borderColor(), QColor(46, 204, 64));

        QTest::mouseClick(window, Qt::MiddleButton);
        QCOMPARE(window->borderColor(), QColor(80, 160, 255));
        QVERIFY(!window->isFlashActive());
        emit window->colorStatisticsChanged({10, 2, 0});
        QCOMPARE(triggered.size(), 1);
        emit window->colorStatisticsChanged({10, 0, 0});
        QCOMPARE(triggered.size(), 2);
        QCOMPARE(triggered.last()[0].toInt(), 32);
        QCOMPARE(window->borderColor(), QColor(80, 160, 255));
        QVERIFY(!window->isFlashActive());

        QTest::mouseClick(window, Qt::MiddleButton);
        QCOMPARE(window->borderColor(), QColor(46, 204, 64));
        QVERIFY(!window->isFlashActive());
        emit window->colorStatisticsChanged({10, 2, 0});
        QCOMPARE(triggered.size(), 3);
        QCOMPARE(triggered.last()[0].toInt(), 31);
        QCOMPARE(window->borderColor(), QColor(46, 204, 64));
        QVERIFY(!window->isFlashActive());
        manager.close();
    }


    void colorLogicChoicesUseRegionPresetsWithoutAnySmallWindows()
    {
        QTemporaryDir directory;
        JsonConfigStore store(directory.path());
        AppConfig config;
        const QRect region(10, 20, 100, 50);
        config.regionPresets = {{QStringLiteral("护盾"), region}, {QStringLiteral("本地"), region}};
        ColorRule rule; rule.number = 1;
        rule.targetReplicaId = logicRegionPresetId(QStringLiteral("本地"));
        rule.windowConditions = {{QStringLiteral("红色"), QStringLiteral(">=2"), -1,
            logicRegionPresetId(QStringLiteral("护盾"))}};
        config.colorRules = {rule};
        QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));
        NativeGateway gateway(functions());
        MainWindow manager(gateway, directory.path());
        QVERIFY(manager.replicaWindows().isEmpty());
        auto* table = manager.findChild<QTableWidget*>(QStringLiteral("colorRuleTable"));
        QVERIFY(table->item(0, 2)->text().startsWith(QStringLiteral("护盾：")));
        QCOMPARE(table->item(0, 4)->text(), QStringLiteral("无"));
        QTimer::singleShot(0, [] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            auto* condition = dialog->findChild<QComboBox*>(QStringLiteral("conditionWindow1"));
            QCOMPARE(condition->currentText(), QStringLiteral("护盾"));
            QCOMPARE(condition->count(), 3); // Both presets remain distinct even with equal rectangles.
            QCOMPARE(condition->itemText(2), QStringLiteral("本地"));
            QVERIFY(!dialog->findChild<QComboBox*>(QStringLiteral("logicTargetWindow")));
            auto* tabs = dialog->findChild<QTabWidget*>(QStringLiteral("colorRuleEditorTabs"));
            QVERIFY(tabs);
            QCOMPARE(tabs->tabText(1), QStringLiteral("状态切换设置"));
            dialog->grab().save(QCoreApplication::applicationDirPath() + QStringLiteral("/region-logic-dialog.png"));
            dialog->reject();
        });
        manager.findChild<ColorRuleController*>()->editColorRuleNumber(1);
    }

    void customWindowChoiceFallsBackToCoordinates()
    {
        ReplicaConfig replica; replica.capturedRegion = QRect(10, 20, 100, 50);
        const auto custom = colorLogicReplicaLabel(replica, 0, {});
        QVERIFY(custom.contains(QStringLiteral("自定义 (10,20) 100×50")));
        replica.capturedRegion = {};
        QVERIFY(colorLogicReplicaLabel(replica, 0, {}).contains(QStringLiteral("完整窗口")));
    }

    void windowConditionEditorAddsAndRemovesSlots()
    {
        ReplicaConfig a; a.id = "A";
        ReplicaConfig b; b.id = "B";
        ColorRule initial;
        initial.number = 7;
        initial.windowConditions = {{QStringLiteral("红色"), QStringLiteral(">=2"), -1, "A"},
            {QStringLiteral("白色"), QStringLiteral("<2"), -1, "B"}};
        initial.targetReplicaId = "A";
        a.capturedRegion = QRect(10, 20, 100, 50);
        b.capturedRegion = QRect(30, 40, 120, 60);
        QTimer::singleShot(0, [] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QVERIFY(!dialog->findChild<QComboBox*>(QStringLiteral("colorRulePriority")));
            QVERIFY(dialog->findChild<QComboBox*>(QStringLiteral("conditionWindow1"))->currentText().contains(QStringLiteral("护盾")));
            QVERIFY(dialog->findChild<QComboBox*>(QStringLiteral("conditionWindow2"))->currentText().contains(QStringLiteral("本地")));
            QVERIFY(!dialog->findChild<QComboBox*>(QStringLiteral("logicTargetWindow")));
            QCOMPARE(dialog->findChild<QComboBox*>(QStringLiteral("conditionWindow1"))->currentData().toString(), QStringLiteral("A"));
            QCOMPARE(dialog->findChild<QComboBox*>(QStringLiteral("conditionWindow2"))->currentData().toString(), QStringLiteral("B"));
            dialog->findChild<QPushButton*>(QStringLiteral("addWindowCondition"))->click();
            auto* third = dialog->findChild<QComboBox*>(QStringLiteral("conditionWindow3"));
            QVERIFY(third);
            const auto removeButtons = third->parentWidget()->findChildren<QPushButton*>();
            for (auto* button : removeButtons) {
                if (button->text() == QStringLiteral("移除条件")) { button->click(); break; }
            }
            auto* tabs = dialog->findChild<QTabWidget*>(QStringLiteral("colorRuleEditorTabs"));
            QCOMPARE(tabs->tabText(1), QStringLiteral("状态切换设置"));
            tabs->setCurrentIndex(1);
            dialog->findChild<QCheckBox*>(QStringLiteral("switchBorderEnabled1"))->setChecked(true);
            auto* middle = dialog->findChild<QComboBox*>(QStringLiteral("triggeredSwitchRule"));
            middle->setCurrentIndex(middle->findData(9));
            dialog->grab().save(QCoreApplication::applicationDirPath() + QStringLiteral("/window-logic-dialog.png"));
            dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
        });
        ColorRule alternate; alternate.number = 9; alternate.note = QStringLiteral("反预警");
        const auto edited = promptColorRule(nullptr, initial, {a, b}, {alternate},
            {{"A", QStringLiteral("护盾")}, {"B", QStringLiteral("本地")}});
        QVERIFY(edited);
        QCOMPARE(edited->windowConditions.size(), 2);
        QCOMPARE(edited->targetReplicaId, QStringLiteral("A"));
        QCOMPARE(edited->windowConditions, initial.windowConditions);
        QCOMPARE(edited->windowSwitches.size(), 2);
        QCOMPARE(edited->windowSwitches.first().replicaId, QStringLiteral("A"));
        QCOMPARE(edited->windowSwitches.first().borderColor, QColor(255, 80, 80));
        QCOMPARE(edited->windowSwitches.first().middleClickRule, 9);
        QCOMPARE(edited->windowSwitches.last().middleClickRule, 9);
        QCOMPARE(edited->jumpToRule, 9);
    }

    void centralColorToleranceSettingsPersistAndCancel()
    {
        const QString path = freshConfigPath();
        JsonConfigStore store(path);
        AppConfig initial;
        initial.colorTolerances = {{QStringLiteral("#123456"), 12}};
        initial.regionPresets = {{QStringLiteral("护盾"), QRect(0, 0, 20, 10)}};
        ReplicaConfig replica;
        replica.statisticsIntervalMs = 0;
        replica.capturedRegion = initial.regionPresets.first().region;
        initial.replicas = {replica, replica};
        initial.replicas[1].id = "second-window";
        QString error;
        QVERIFY2(store.save(initial, &error), qPrintable(error));
        NativeGateway gateway(functions());
        const QList<PixelColorTarget> expected{{QStringLiteral("#ffaa00"), 7}};
        {
            MainWindow manager(gateway, path);
            for (auto* row : manager.replicaRows()) {
                QCOMPARE(row->statisticsColors(), initial.colorTolerances);
                QVERIFY(!row->findChild<QPushButton*>(QStringLiteral("statisticsColors")));
            }
            auto* settings = manager.findChild<QPushButton*>(QStringLiteral("colorToleranceSettings"));
            QVERIFY(settings); QVERIFY(settings->isEnabled());
            for (bool save : {false, true}) {
                QTimer::singleShot(0, [save] {
                    auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                    QVERIFY(dialog);
                    auto* scope = dialog->findChild<QComboBox*>(QStringLiteral("colorToleranceScope"));
                    QCOMPARE(scope->count(), 2);
                    scope->setCurrentIndex(1);
                    auto* inherit = dialog->findChild<QCheckBox*>(QStringLiteral("inheritColorTolerances"));
                    QVERIFY(inherit->isChecked());
                    inherit->click();
                    QTimer::singleShot(0, [] {
                        auto* editorDialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                        QVERIFY(editorDialog);
                        auto* table = editorDialog->findChild<QTableWidget*>(QStringLiteral("statisticsColorsTable"));
                        QVERIFY(table); QCOMPARE(table->rowCount(), 1);
                        auto* editor = static_cast<PixelColorEditor*>(table->cellWidget(0, 0));
                        editor->combo()->setCurrentText(QStringLiteral("#ffaa00"));
                        editor->tolerance()->setValue(7);
                        editorDialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
                    });
                    dialog->findChild<QPushButton*>(QStringLiteral("editColorTolerances"))->click();
                    dialog->findChild<QDialogButtonBox*>()->button(save ? QDialogButtonBox::Save : QDialogButtonBox::Cancel)->click();
                });
                settings->click();
                for (auto* row : manager.replicaRows()) {
                    QCOMPARE(row->statisticsColors(), save ? expected : initial.colorTolerances);
                }
                for (auto* window : manager.replicaWindows()) {
                    QCOMPARE(window->statisticsColors(), save ? expected : initial.colorTolerances);
                }
            }
            manager.close();
        }
        AppConfig saved;
        QVERIFY2(store.load(saved, &error), qPrintable(error));
        QCOMPARE(saved.colorTolerances, initial.colorTolerances);
        QCOMPARE(saved.regionPresets.first().colorTolerances, expected);
        MainWindow restored(gateway, path);
        for (auto* row : restored.replicaRows()) { QCOMPARE(row->statisticsColors(), expected); }
        for (auto* window : restored.replicaWindows()) { QCOMPARE(window->statisticsColors(), expected); }
    }

    void conditionBoundsRelationRestoresAndSaves()
    {
        ReplicaConfig replica; replica.id = "A";
        ColorRule initial; initial.number = 1;
        initial.windowConditions = {{QStringLiteral("红色"), QStringLiteral(">100|<10"), -1, "A"}};
        for (auto relation : {TriggerConditionRelation::Or, TriggerConditionRelation::And}) {
            QTimer::singleShot(0, [relation] {
                auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()); QVERIFY(dialog);
                auto* bounds = dialog->findChild<QComboBox*>(QStringLiteral("triggerCondition1BoundsRelation"));
                QVERIFY(bounds); QVERIFY(bounds->isEnabled());
                QCOMPARE(bounds->currentData().toInt(), static_cast<int>(TriggerConditionRelation::Or));
                QCOMPARE(dialog->findChild<QSpinBox*>(QStringLiteral("triggerCondition1LowerValue"))->value(), 100);
                QCOMPARE(dialog->findChild<QSpinBox*>(QStringLiteral("triggerCondition1UpperValue"))->value(), 10);
                bounds->setCurrentIndex(bounds->findData(static_cast<int>(relation)));
                dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
            });
            const auto edited = promptColorRule(nullptr, initial, {replica}, {}, {{"A", QStringLiteral("护盾")}});
            QVERIFY(edited);
            QCOMPARE(edited->windowConditions.first().expression, relation == TriggerConditionRelation::Or
                ? QStringLiteral(">100|<10") : QStringLiteral(">100&<10"));
        }
    }

    void stateSwitchSettingsShareOneTargetForManualAndTriggeredTransitions()
    {
        ReplicaConfig replica; replica.id = "A";
        ColorRule initial; initial.number = 1;
        initial.windowConditions = {{QStringLiteral("红色"), QStringLiteral(">=2"), -1, replica.id}};
        ColorRule other; other.number = 9; other.note = QStringLiteral("反预警");
        for (const auto effect : {PostTriggerAction::Loop, PostTriggerAction::ReverseWait, PostTriggerAction::Jump}) {
            QTimer::singleShot(0, [effect] {
                auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()); QVERIFY(dialog);
                auto* tabs = dialog->findChild<QTabWidget*>(QStringLiteral("colorRuleEditorTabs"));
                QCOMPARE(tabs->tabText(1), QStringLiteral("状态切换设置"));
                auto* action = dialog->findChild<QComboBox*>(QStringLiteral("postTriggerAction"));
                auto* target = dialog->findChild<QComboBox*>(QStringLiteral("triggeredSwitchRule"));
                QVERIFY(action); QVERIFY(target);
                QVERIFY(tabs->widget(1)->isAncestorOf(action));
                QVERIFY(tabs->widget(1)->isAncestorOf(target));
                QVERIFY(!dialog->findChild<QComboBox*>(QStringLiteral("switchMiddleRule1")));
                QCOMPARE(action->count(), 3);
                action->setCurrentIndex(action->findData(static_cast<int>(effect)));
                QVERIFY(target->isEnabled());
                auto* save = dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save);
                if (effect == PostTriggerAction::Jump) {
                    save->click();
                    QCOMPARE(tabs->currentIndex(), 1);
                    QVERIFY(dialog->findChild<QLabel*>(QStringLiteral("stateSwitchError"))->isVisible());
                    QVERIFY(!qobject_cast<QMessageBox*>(QApplication::activeModalWidget()));
                }
                target->setCurrentIndex(target->findData(9));
                save->click();
            });
            const auto edited = promptColorRule(nullptr, initial, {replica}, {other}, {{"A", QStringLiteral("护盾")}});
            QVERIFY(edited);
            QCOMPARE(edited->postTriggerAction, effect);
            QCOMPARE(edited->jumpToRule, 9);
            QCOMPARE(edited->windowSwitches.size(), 1);
            QCOMPARE(edited->windowSwitches.first().middleClickRule, 9);
        }
        initial.windowSwitches = {{"A", QColor(46, 204, 64), 9}};
        QTimer::singleShot(0, [] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()); QVERIFY(dialog);
            auto* target = dialog->findChild<QComboBox*>(QStringLiteral("triggeredSwitchRule"));
            QCOMPARE(target->currentData().toInt(), 9);
            dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
        });
        const auto restored = promptColorRule(nullptr, initial, {replica}, {other}, {{"A", QStringLiteral("护盾")}});
        QVERIFY(restored);
        QCOMPARE(restored->windowSwitches, initial.windowSwitches);
    }

    void colorEditorSupportsExactCustomRgbAndRuleDialogPreservesIt()
    {
        PixelColorEditor editor({QStringLiteral("#abcdef"), 0}, false);
        QVERIFY(editor.target().has_value());
        QCOMPARE(editor.target()->tolerance, 0);
        QCOMPARE(editor.tolerance()->text(), QStringLiteral("0"));
        editor.combo()->setCurrentText(QStringLiteral("bad color"));
        QVERIFY(!editor.target().has_value());
        ColorRule initial;
        initial.number = 3;
        initial.triggerCondition1 = {QStringLiteral("#123456"), QStringLiteral(">=2"), 12};
        QTimer::singleShot(0, [] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            auto* color = dialog->findChild<QComboBox*>(QStringLiteral("triggerCondition1Pixel"));
            auto* tolerance = dialog->findChild<QSpinBox*>(QStringLiteral("triggerCondition1Tolerance"));
            QVERIFY(color);
            QVERIFY(tolerance);
            QCOMPARE(color->currentText(), QStringLiteral("#123456"));
            QCOMPARE(tolerance->value(), 12);
            QVERIFY(!tolerance->isVisible());
            color->setCurrentText(QStringLiteral("#00ff00"));
            tolerance->setValue(20);
            auto* buttons = dialog->findChild<QDialogButtonBox*>();
            buttons->button(QDialogButtonBox::Save)->click();
        });
        ReplicaConfig replica; replica.id = QStringLiteral("dialog-window");
        initial.targetReplicaId = replica.id;
        if (initial.triggerCondition1.isEmpty()) {
            initial.triggerCondition1 = {QStringLiteral("红色"), QStringLiteral(">=2")};
        }
        initial.triggerCondition1.replicaId = replica.id;
        const auto edited = promptColorRule(nullptr, initial, {replica}, {}, {{replica.id, QStringLiteral("测试区域")}});
        QVERIFY(edited.has_value());
        QCOMPARE(edited->windowConditions.first().pixel, QStringLiteral("#00ff00"));
        QCOMPARE(edited->windowConditions.first().tolerance, -1);
        QCOMPARE(edited->windowConditions.first().expression, QStringLiteral(">=2"));
    }

    void colorRuleDialogRestoresAndSavesConditionRelation_data()
    {
        QTest::addColumn<bool>("initialOr");
        QTest::newRow("and-to-or") << false;
        QTest::newRow("or-to-and") << true;
    }

    void colorRuleDialogRestoresAndSavesConditionRelation()
    {
        QFETCH(bool, initialOr);
        ColorRule initial;
        initial.triggerConditionRelation = initialOr ? TriggerConditionRelation::Or : TriggerConditionRelation::And;
        QTimer::singleShot(0, [initialOr] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            auto* relation = dialog->findChild<QComboBox*>(QStringLiteral("triggerConditionRelation"));
            QVERIFY(relation);
            QCOMPARE(relation->count(), 2);
            QCOMPARE(relation->currentData().toInt(), static_cast<int>(
                initialOr ? TriggerConditionRelation::Or : TriggerConditionRelation::And));
            relation->setCurrentIndex(relation->findData(static_cast<int>(
                initialOr ? TriggerConditionRelation::And : TriggerConditionRelation::Or)));
            dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
        });
        ReplicaConfig replica; replica.id = QStringLiteral("dialog-window");
        initial.targetReplicaId = replica.id;
        if (initial.triggerCondition1.isEmpty()) {
            initial.triggerCondition1 = {QStringLiteral("红色"), QStringLiteral(">=2")};
        }
        initial.triggerCondition1.replicaId = replica.id;
        const auto edited = promptColorRule(nullptr, initial, {replica}, {}, {{replica.id, QStringLiteral("测试区域")}});
        QVERIFY(edited);
        QCOMPARE(edited->triggerConditionRelation,
                 initialOr ? TriggerConditionRelation::And : TriggerConditionRelation::Or);
    }

    void replicaRowShowsThreePresetsAndCanDisplayCustomColorCounts()
    {
        ReplicaRow row(1);
        row.show();
        QVERIFY(row.statisticsColors() == defaultPixelColorTargets());
        auto* blue = row.findChild<QLabel*>(QStringLiteral("bluePixelPercentage"));
        QVERIFY(blue);
        row.setColorStatistics(PixelStatistics{10, 1, 2, 3});
        QCOMPARE(blue->text(), QStringLiteral("蓝色像素：3"));
        row.setStatisticsColors({{QStringLiteral("#123456"), 12}});
        QImage image(2, 1, QImage::Format_RGB32);
        image.fill(QColor(QStringLiteral("#123456")));
        row.setColorStatistics(calculatePixelStatistics(image, std::nullopt, row.statisticsColors()));
        auto* label = row.findChild<QLabel*>(QStringLiteral("customPixelCount0"));
        QVERIFY(label);
        QCOMPARE(label->text(), QStringLiteral("#123456像素：2"));
        row.clearPixelStatistics();
        QCOMPARE(label->text(), QStringLiteral("#123456像素：--"));
    }

    void copiedStateAndHiddenGeometryEditsPersistFromTheController()
    {
        QTemporaryDir directory;
        JsonConfigStore store(directory.path());
        AppConfig initial;
        initial.colorRules.append({1, QStringLiteral("rule"), QStringLiteral("红色"),
            QStringLiteral(">=2"), TriggerFeedback::None, {}, PostTriggerAction::ReverseWait});
        QString error;
        QVERIFY2(store.save(initial, &error), qPrintable(error));
        NativeGateway gateway(functions());
        MainWindow manager(gateway, directory.path());
        auto* original = manager.createReplica();
        original->selectSource({123, QStringLiteral("EVE - Alpha")});
        original->setStatisticsInterval(0);
        original->setCapturedRegion(QRect(100, 100, 80, 40));
        original->setColorLogicSlot(1, 1);
        original->setColorLogicEnabled(true);
        original->setColorLogicSlotColor(1, QColor(QStringLiteral("#123456")));
        original->moveTo(QPoint(70, 60));
        auto* copy = manager.duplicateReplica(original->replicaId());
        QVERIFY(copy != nullptr);
        QVERIFY(copy->pos() != original->pos());
        QVERIFY(!copy->colorLogicEnabled());
        QCoreApplication::processEvents();
        auto* row = manager.replicaRows().last();
        row->setVisibilityChecked(false);
        row->positionEditRequested(QPoint(111, 222));
        row->scalePercentageEdited(200.0);
        const auto acceptedSize = copy->size();
        QTest::qWait(650);
        AppConfig actual;
        QVERIFY2(store.load(actual, &error), qPrintable(error));
        QCOMPARE(actual.replicas.size(), 2);
        const auto copied = actual.replicas.last();
        QCOMPARE(copied.sourceTitle, QStringLiteral("EVE - Alpha"));
        QCOMPARE(copied.position, QPoint(111, 222));
        QCOMPARE(copied.size, acceptedSize);
        QCOMPARE(copied.capturedRegion, QRect(100, 100, 80, 40));
        QCOMPARE(copied.colorLogicSlotRule[0], 0);
        QVERIFY(!copied.colorLogicEnabled);
        QVERIFY(copied.id != actual.replicas.first().id);
        QVERIFY(!copied.visible);
    }
    void autosavingGeometryDoesNotCancelAnActivationRequest()
    {
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        // This test checks persistence, so real keyboard/mouse input must not gate activation.
        manager.findChild<QuickSwitchService*>()->stop();
        gateway.setAutomaticActivationMouseQuietPeriodMs(100);
        auto* replica = manager.createReplica();
        replica->selectSource({123, QStringLiteral("EVE - Alpha")});
        replica->setStatisticsInterval(0);
        const int before = mockActivationCount;
        gateway.queueAutomaticActivation(replica->replicaId(), 123, 0);
        replica->moveTo(QPoint(234, 345));
        QTest::qWait(650);
        gateway.flushAutomaticActivations();
        QCOMPARE(mockActivationCount, before + 1);
    }

    void closingAfterFailedLoadPreservesTheCorruptConfiguration()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("replicas.json"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray broken("{broken");
        QCOMPARE(file.write(broken), broken.size());
        file.close();
        NativeGateway gateway(functions());
        {
            MainWindow manager(gateway, directory.path());
            manager.close();
        }
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), broken);
    }

    void closingPreservesCorruptPetSettings()
    {
        QTemporaryDir directory;
        QFile file(directory.filePath(QStringLiteral("desktopPet.json")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray broken("{broken pet");
        QCOMPARE(file.write(broken), broken.size());
        file.close();
        NativeGateway gateway(functions());
        { MainWindow manager(gateway, directory.path()); }
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), broken);
    }

    void mainWindowOwnsOneStatisticsCoordinatorAndOneAudioManager()
    {
        NativeGateway gateway(functions());
        MainWindow window(gateway, freshConfigPath());
        static_cast<void>(window.createReplica());
        static_cast<void>(window.createReplica());
        static_cast<void>(window.createReplica());

        QCOMPARE(window.findChildren<StatisticsCoordinator*>(
                     QStringLiteral("statisticsCoordinator"),
                     Qt::FindDirectChildrenOnly).size(),
                 1);
        QCOMPARE(window.findChildren<AudioFeedbackManager*>(
                     QStringLiteral("audioFeedbackManager"),
                     Qt::FindDirectChildrenOnly).size(),
                 1);
        for (auto* replica : window.replicaWindows()) {
            QVERIFY(replica->findChildren<QMediaPlayer*>(
                QString{}, Qt::FindDirectChildrenOnly).isEmpty());
        }
    }

    void homePageOffersConfigurableAutomaticActivationCooldown()
    {
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        manager.show();
        QCoreApplication::processEvents();

        auto* optionsTitle = manager.findChild<QLabel*>(
            QStringLiteral("softwareOptionsTitle"));
        auto* optionsPanel = manager.findChild<QFrame*>(
            QStringLiteral("softwareOptionsPanel"));
        auto* presetTitle = manager.findChild<QLabel*>(
            QStringLiteral("presetSectionTitle"));
        QVERIFY(optionsTitle != nullptr);
        QVERIFY(optionsPanel != nullptr);
        QVERIFY(presetTitle != nullptr);
        QVERIFY(optionsTitle->geometry().top() < optionsPanel->geometry().top());
        QVERIFY(optionsPanel->geometry().bottom() < presetTitle->geometry().top());

        for (const auto& objectName : {
                 QStringLiteral("activateSourceOnColorLogicTrigger"),
                 QStringLiteral("raiseReplicaOnClick"),
                 QStringLiteral("restoreWindowsWithGame"),
                 QStringLiteral("keepInTray")}) {
            auto* option = manager.findChild<QCheckBox*>(objectName);
            QVERIFY(option != nullptr);
            QVERIFY(optionsPanel->isAncestorOf(option));
        }

        auto* cooldown = manager.findChild<QDoubleSpinBox*>(
            QStringLiteral("automaticActivationCooldown"));
        QVERIFY(cooldown != nullptr);
        QVERIFY(optionsPanel->isAncestorOf(cooldown));
        QCOMPARE(cooldown->minimum(), 1.0);
        QCOMPARE(cooldown->maximum(), 10.0);
        QCOMPARE(cooldown->singleStep(), 0.5);
        QCOMPARE(cooldown->value(), 5.0);

        auto* window = manager.createReplica();
        QCOMPARE(window->property("automaticActivationCooldownMs").toInt(), 5000);

        cooldown->setValue(7.5);
        QCoreApplication::processEvents();
        QCOMPARE(window->property("automaticActivationCooldownMs").toInt(), 7500);
    }

    void colorTriggerSourceActivationOptionPersistsAndUpdatesReplicas()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path();
        JsonConfigStore store(path);
        AppConfig config;
        config.activateSourceOnColorLogicTrigger = false;
        QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));

        NativeGateway gateway(functions());
        MainWindow manager(gateway, path);
        auto* activation = manager.findChild<QCheckBox*>(
            QStringLiteral("activateSourceOnColorLogicTrigger"));
        auto* cooldown = manager.findChild<QDoubleSpinBox*>(
            QStringLiteral("automaticActivationCooldown"));
        QVERIFY(activation != nullptr);
        QVERIFY(cooldown != nullptr);
        QVERIFY(!activation->isChecked());
        QVERIFY(!cooldown->isEnabled());

        auto* window = manager.createReplica();
        QVERIFY(!window->property("automaticActivationEnabled").toBool());

        activation->setChecked(true);
        QVERIFY(cooldown->isEnabled());
        QVERIFY(window->property("automaticActivationEnabled").toBool());

        AppConfig persisted;
        QVERIFY2(store.load(persisted, &error), qPrintable(error));
        QVERIFY(persisted.activateSourceOnColorLogicTrigger);
    }

    void replicaRowOffersIndependentVisibilitySwitch()
    {
        ReplicaRow row(1);
        auto* visibility = row.findChild<QCheckBox*>(QStringLiteral("replicaVisibility"));
        QVERIFY(visibility != nullptr);
        QVERIFY(visibility->isChecked());

        QSignalSpy visibilityChanged(&row, &ReplicaRow::replicaVisibilityChanged);
        visibility->setChecked(false);

        QCOMPARE(visibilityChanged.count(), 1);
        QCOMPARE(visibilityChanged.first().first().toBool(), false);
    }

    void replicaRowOffersDisabledOrAtLeastOneAndAHalfSecondStatisticsIntervals()
    {
        ReplicaRow row(1);
        auto* interval = row.findChild<QComboBox*>(QStringLiteral("statisticsInterval"));
        QVERIFY(interval != nullptr);
        QCOMPARE(interval->count(), 19);
        QCOMPARE(interval->itemText(0), QStringLiteral("不统计"));
        QCOMPARE(interval->itemData(0).toInt(), 0);
        QCOMPARE(interval->itemText(1), QStringLiteral("1.5 s"));
        QCOMPARE(interval->itemData(1).toInt(), 1500);
        QCOMPARE(interval->findData(500), -1);
        QCOMPARE(interval->findData(1000), -1);
        QCOMPARE(interval->itemText(18), QStringLiteral("10.0 s"));
        QCOMPARE(interval->itemData(18).toInt(), 10000);
        QCOMPARE(interval->currentData().toInt(), 1500);

        QSignalSpy intervalChanged(&row, &ReplicaRow::statisticsIntervalChanged);
        interval->setCurrentIndex(interval->findData(3500));

        QCOMPARE(intervalChanged.count(), 1);
        QCOMPARE(intervalChanged.first().first().toInt(), 3500);
    }

    void replicaRowColorLogicSlotsRejectDuplicateSelection()
    {
        ReplicaRow row(1);
        auto* slotA = row.findChild<QComboBox*>(QStringLiteral("colorLogicSlotA"));
        auto* slotB = row.findChild<QComboBox*>(QStringLiteral("colorLogicSlotB"));
        QVERIFY(slotA != nullptr);
        QVERIFY(slotB != nullptr);
        QCOMPARE(slotA->count(), 1);
        QCOMPARE(slotA->itemText(0), QStringLiteral("无"));

        QList<ColorRule> rules;
        rules.append({1, QStringLiteral("护盾"), {}, {}, TriggerFeedback::None, {},
                      PostTriggerAction::Loop});
        rules.append({2, QStringLiteral("锁定"), {}, {}, TriggerFeedback::None, {},
                      PostTriggerAction::Loop});
        row.setColorRules(rules);

        QCOMPARE(slotA->count(), 3);
        QCOMPARE(slotA->itemText(1), QStringLiteral("#1 护盾"));
        QCOMPARE(slotA->itemText(2), QStringLiteral("#2 锁定"));

        QSignalSpy slotChanged(&row, &ReplicaRow::colorLogicSlotChanged);
        slotA->setCurrentIndex(1);
        QCOMPARE(slotChanged.count(), 1);
        QCOMPARE(slotChanged.first()[0].toInt(), 1);
        QCOMPARE(slotChanged.first()[1].toInt(), 1);
        QCOMPARE(slotB->count(), 2);
        QCOMPARE(slotB->itemText(1), QStringLiteral("#2 锁定"));

        slotB->setCurrentIndex(1);
        QCOMPARE(slotChanged.count(), 2);
        QCOMPARE(slotA->count(), 2);
        QCOMPARE(slotA->itemText(1), QStringLiteral("#1 护盾"));

        row.setColorRules({rules[1]});
        QCOMPARE(slotA->currentData().toInt(), 0);
        QCOMPARE(slotA->count(), 1);
        QCOMPARE(slotA->itemText(0), QStringLiteral("无"));
        QCOMPARE(slotB->currentData().toInt(), 2);
        QCOMPARE(slotB->count(), 2);
        QCOMPARE(slotB->itemText(1), QStringLiteral("#2 锁定"));
    }

    void replicaRowOffersSeparateEnableSwitchAndSlotSelector()
    {
        ReplicaRow row(1);
        auto* enabled = row.findChild<QCheckBox*>(QStringLiteral("colorLogicEnabled"));
        auto* slot = row.findChild<QComboBox*>(QStringLiteral("colorLogicActiveSlot"));
        QVERIFY(enabled != nullptr);
        QVERIFY(slot != nullptr);
        QVERIFY(!enabled->isChecked());
        QCOMPARE(slot->count(), 2);
        QCOMPARE(slot->itemText(0), QStringLiteral("槽位1"));
        QCOMPARE(slot->itemData(0).toInt(), 1);
        QCOMPARE(slot->itemText(1), QStringLiteral("槽位2"));
        QCOMPARE(slot->itemData(1).toInt(), 2);
        QCOMPARE(slot->currentData().toInt(), 1);

        QSignalSpy enabledChanged(&row, &ReplicaRow::colorLogicEnabledChanged);
        enabled->setChecked(true);
        QCOMPARE(enabledChanged.count(), 1);
        QCOMPARE(enabledChanged.first().first().toBool(), true);

        QSignalSpy slotChanged(&row, &ReplicaRow::activeColorLogicSlotChanged);
        slot->setCurrentIndex(1);
        QCOMPARE(slotChanged.count(), 1);
        QCOMPARE(slotChanged.first().first().toInt(), 2);

        row.setColorLogicEnabled(false);
        QVERIFY(!enabled->isChecked());
        row.setActiveColorLogicSlot(1);
        QCOMPARE(slot->currentData().toInt(), 1);
    }

    void hidingReplicaKeepsItsGlobalLogicRunning()
    {
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        auto* window = manager.createReplica();
        window->selectSource({123, QStringLiteral("EVE - Alpha")});
        auto* row = manager.replicaRows().first();
        ColorRule rule{1, QStringLiteral("红色"), QStringLiteral("红色"), QStringLiteral(">=2"),
            TriggerFeedback::None, {}, PostTriggerAction::ReverseWait};
        bindGlobalRule(manager, window, rule);
        QVERIFY(row->findChild<QComboBox*>(QStringLiteral("colorLogicSlotA"))->isHidden());
        QVERIFY(row->findChild<QCheckBox*>(QStringLiteral("colorLogicEnabled"))->isHidden());
        QVERIFY(row->findChild<QLabel*>(QStringLiteral("colorLogicSummary")));
        QSignalSpy triggered(window, &ReplicaWindow::colorLogicTriggered);
        row->setVisibilityChecked(false);
        QVERIFY(!window->isVisible());
        QVERIFY(window->statisticsIntervalMs() > 0);
        emit window->colorStatisticsChanged({10, 2, 0});
        QCOMPARE(triggered.size(), 1);
    }

    void replicaRowOffersSlotColorButtonsWithDefaults()
    {
        ReplicaRow row(1);
        auto* buttonA = row.findChild<QPushButton*>(QStringLiteral("colorLogicSlotColorA"));
        auto* buttonB = row.findChild<QPushButton*>(QStringLiteral("colorLogicSlotColorB"));
        QVERIFY(buttonA != nullptr);
        QVERIFY(buttonB != nullptr);
        QVERIFY(row.colorLogicSlotColor(1) == QColor(46, 204, 64));
        QVERIFY(row.colorLogicSlotColor(2) == QColor(41, 121, 255));
    }

    void replicaRowEnablesPresetSaveOnlyAfterRegionCapture()
    {
        ReplicaRow row(3);
        auto* save = row.findChild<QPushButton*>(QStringLiteral("saveRegionPreset"));
        QVERIFY(save != nullptr);
        QVERIFY(!save->isEnabled());

        QSignalSpy requested(&row, &ReplicaRow::saveRegionPresetRequested);
        row.setCaptureRegion(QStringLiteral("300×180 @ (10,20)"));
        QVERIFY(save->isEnabled());
        QTest::mouseClick(save, Qt::LeftButton);
        QCOMPARE(requested.count(), 1);

        row.setCaptureRegion(QStringLiteral("未截取"));
        QVERIFY(!save->isEnabled());
    }

    void replicaRowShowsResetMessageWithoutEnablingPresetSave()
    {
        ReplicaRow row(1);
        auto* save = row.findChild<QPushButton*>(QStringLiteral("saveRegionPreset"));
        QVERIFY(save != nullptr);
        row.setCaptureRegion(QStringLiteral("未发现区域，已重置"));
        QVERIFY(!save->isEnabled());
        auto* regionEdit = row.findChild<QLineEdit*>(QStringLiteral("capturedRegion"));
        QVERIFY(regionEdit != nullptr);
        QCOMPARE(regionEdit->text(), QStringLiteral("未发现区域，已重置"));
    }

    void replicaRowRegionInputParsesAndEmitsRect()
    {
        ReplicaRow row(1);
        auto* regionEdit = row.findChild<QLineEdit*>(QStringLiteral("capturedRegion"));
        QVERIFY(regionEdit != nullptr);
        QSignalSpy edited(&row, &ReplicaRow::regionEditRequested);

        regionEdit->setText(QStringLiteral("231,236,488,366"));
        QTest::keyClick(regionEdit, Qt::Key_Return);
        QCOMPARE(edited.count(), 1);
        QCOMPARE(edited.first().first().toRect(), QRect(231, 236, 488, 366));

        regionEdit->setText(QStringLiteral("488×366 @ (231,236)"));
        QTest::keyClick(regionEdit, Qt::Key_Return);
        QCOMPARE(edited.count(), 2);
        QCOMPARE(edited.last().first().toRect(), QRect(231, 236, 488, 366));

        regionEdit->setText(QStringLiteral("abc"));
        QTest::keyClick(regionEdit, Qt::Key_Return);
        QCOMPARE(edited.count(), 2);
        QCOMPARE(regionEdit->text(), QStringLiteral("未截取"));
    }

    void replicaRowDisplaysPixelStatisticsAndUnavailableState()
    {
        ReplicaRow row(2);
        row.show();
        auto* red = row.findChild<QLabel*>(QStringLiteral("redPixelPercentage"));
        auto* white = row.findChild<QLabel*>(QStringLiteral("whitePixelPercentage"));
        QVERIFY(red != nullptr);
        QVERIFY(white != nullptr);
        QCOMPARE(red->text(), QStringLiteral("红色像素：--"));
        QCOMPARE(white->text(), QStringLiteral("白色像素：--"));

        row.setPixelStatistics(12, 5);
        QCOMPARE(red->text(), QStringLiteral("红色像素：12"));
        QCOMPARE(white->text(), QStringLiteral("白色像素：5"));

        row.clearPixelStatistics();
        QCOMPARE(red->text(), QStringLiteral("红色像素：--"));
        QCOMPARE(white->text(), QStringLiteral("白色像素：--"));
    }

    void hiddenStatisticsPageDefersLabelsAndShowsOnlyTheLatestResult()
    {
        QStackedWidget pages;
        auto* row = new ReplicaRow(1);
        pages.addWidget(row);
        pages.addWidget(new QWidget);
        pages.show();
        auto* red = row->findChild<QLabel*>(QStringLiteral("redPixelPercentage"));
        QVERIFY(red);
        QVERIFY(red->isVisible());
        row->setPixelStatistics(10, 5);
        QCOMPARE(red->text(), QStringLiteral("红色像素：10"));

        pages.setCurrentIndex(1);
        QVERIFY(!red->isVisible());
        for (int count = 11; count <= 40; ++count) { row->setPixelStatistics(count, 5); }
        QCOMPARE(red->text(), QStringLiteral("红色像素：10"));
        pages.setCurrentIndex(0);
        QCOMPARE(red->text(), QStringLiteral("红色像素：40"));
        row->setPixelStatistics(40, 5);
        QCOMPARE(red->text(), QStringLiteral("红色像素：40"));

        pages.setCurrentIndex(1);
        row->setPixelStatistics(99, 5);
        row->clearPixelStatistics();
        QCOMPARE(red->text(), QStringLiteral("红色像素：40"));
        pages.setCurrentIndex(0);
        QCOMPARE(red->text(), QStringLiteral("红色像素：--"));
    }

    void collapsedStatisticsResumeWithLatestResultAndUnavailableState()
    {
        ReplicaRow row(1);
        row.show();
        auto* red = row.findChild<QLabel*>(QStringLiteral("redPixelPercentage"));
        QVERIFY(red);
        auto* summary = row.findChild<QLabel*>(QStringLiteral("collapsedPixelStatistics"));
        QVERIFY(summary);
        QVERIFY(!summary->isVisible());
        row.setPixelStatistics(1, 2);
        row.setCollapsed(true);
        QVERIFY(!red->isVisible());
        QVERIFY(summary->isVisible());
        QCOMPARE(summary->text(), QStringLiteral("红色像素：1    白色像素：2    蓝色像素：0"));
        row.setColorStatistics(PixelStatistics{30, 8, 9, 13});
        QCOMPARE(summary->text(), QStringLiteral("红色像素：8    白色像素：9    蓝色像素：13"));
        QCOMPARE(red->text(), QStringLiteral("红色像素：1"));
        row.hide();
        row.setColorStatistics(PixelStatistics{40, 10, 12, 18});
        QCOMPARE(summary->text(), QStringLiteral("红色像素：8    白色像素：9    蓝色像素：13"));
        row.show();
        QCOMPARE(summary->text(), QStringLiteral("红色像素：10    白色像素：12    蓝色像素：18"));
        row.setCollapsed(false);
        QVERIFY(!summary->isVisible());
        QCOMPARE(red->text(), QStringLiteral("红色像素：10"));
        row.setCollapsed(true);
        row.clearPixelStatistics();
        QCOMPARE(summary->text(), QStringLiteral("红色像素：--    白色像素：--    蓝色像素：--"));
        row.setCollapsed(false);
        QCOMPARE(red->text(), QStringLiteral("红色像素：--"));
    }

    void changingHiddenStatisticsColorsInvalidatesOldDisplayCache()
    {
        ReplicaRow row(1);
        row.show();
        row.setPixelStatistics(12, 3);
        row.hide();
        const PixelColorTarget target{QStringLiteral("#123456"), 0};
        row.setStatisticsColors({target});
        auto* custom = row.findChild<QLabel*>(QStringLiteral("customPixelCount0"));
        QVERIFY(custom);
        QImage image(2, 1, QImage::Format_RGB32);
        image.fill(QColor(target.color));
        row.setColorStatistics(calculatePixelStatistics(image, std::nullopt, {target}));
        QCOMPARE(custom->text(), QStringLiteral("#123456像素：--"));
        row.show();
        QCOMPARE(custom->text(), QStringLiteral("#123456像素：2"));
        row.hide();
        row.clearPixelStatistics();
        row.setStatisticsColors(defaultPixelColorTargets());
        row.show();
        QCOMPARE(row.findChild<QLabel*>(QStringLiteral("redPixelPercentage"))->text(),
                 QStringLiteral("红色像素：--"));
    }

    void buttonCreatesIndependentReplicasAndRows()
    {
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        manager.show();
        auto* button = manager.findChild<QPushButton*>(QStringLiteral("createButton"));
        QVERIFY(button != nullptr);

        QTest::mouseClick(button, Qt::LeftButton);
        QTest::mouseClick(button, Qt::LeftButton);
        QTest::mouseClick(button, Qt::LeftButton);
        QCoreApplication::processEvents();

        QCOMPARE(manager.activeReplicaCount(), std::size_t{3});
        QCOMPARE(manager.replicaRows().size(), 3);
        QCOMPARE(manager.replicaWindows().size(), 3);
        QCOMPARE(manager.countLabel()->text(), QStringLiteral("3 个小窗"));
        for (auto* window : manager.replicaWindows()) {
            QVERIFY(window->isWindow());
            QVERIFY(window->isVisible());
        }
    }

    void visibilitySwitchHidesWithoutClosingReplica()
    {
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        manager.show();
        auto* window = manager.createReplica();
        auto* row = manager.replicaRows().first();
        auto* visibility = row->findChild<QCheckBox*>(QStringLiteral("replicaVisibility"));
        QVERIFY(visibility != nullptr);
        QVERIFY(window->isVisible());

        visibility->setChecked(false);
        QCoreApplication::processEvents();
        QVERIFY(!window->isVisible());
        QCOMPARE(manager.activeReplicaCount(), std::size_t{1});
        QCOMPARE(manager.replicaRows().size(), 1);

        visibility->setChecked(true);
        QCoreApplication::processEvents();
        QVERIFY(window->isVisible());
    }

    void closingOneReplicaLeavesTheOthersAlive()
    {
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        auto* first = manager.createReplica();
        auto* second = manager.createReplica();
        auto* third = manager.createReplica();
        Q_UNUSED(second);
        Q_UNUSED(third);

        first->close();
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();

        QCOMPARE(manager.activeReplicaCount(), std::size_t{2});
        QCOMPARE(manager.replicaRows().size(), 2);
        QCOMPARE(manager.replicaWindows().size(), 2);
        QCOMPARE(manager.countLabel()->text(), QStringLiteral("2 个小窗"));
    }

    void closingManagerDestroysEveryReplica()
    {
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        static_cast<void>(manager.createReplica());
        static_cast<void>(manager.createReplica());
        static_cast<void>(manager.createReplica());
        QList<QPointer<ReplicaWindow>> windows;
        for (auto* window : manager.replicaWindows()) {
            windows.append(window);
        }

        manager.close();
        QCoreApplication::processEvents();

        QCOMPARE(manager.activeReplicaCount(), std::size_t{0});
        for (const auto& window : windows) {
            QVERIFY(window.isNull());
        }
    }


    void homeHasNineTabsAndCreateButton()
    {
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        manager.show();

        auto* tabs = manager.findChild<QTabWidget*>(QStringLiteral("mainTabs"));
        QVERIFY(tabs != nullptr);
        QCOMPARE(tabs->count(), 9);
        QCOMPARE(tabs->tabText(0), QStringLiteral("首页"));
        QCOMPARE(tabs->tabText(1), QStringLiteral("小窗列表"));
        QCOMPARE(tabs->tabText(2), QStringLiteral("区域预设"));
        QCOMPARE(tabs->tabText(3), QStringLiteral("图色逻辑"));
        QCOMPARE(tabs->tabText(4), QStringLiteral("逻辑列表"));
        QCOMPARE(tabs->tabText(5), QStringLiteral("快捷切屏"));
        QCOMPARE(tabs->tabText(6), QStringLiteral("页面分区"));
        QCOMPARE(tabs->tabText(7), QStringLiteral("远征计时"));
        QCOMPARE(tabs->tabText(8), QStringLiteral("摸鱼桌宠"));

        auto* button = manager.findChild<QPushButton*>(QStringLiteral("createButton"));
        QVERIFY(button != nullptr);
        QVERIFY(button->isVisible());
    }

    void clickingExpeditionTimingTabAlwaysSynchronizesCurrentTime()
    {
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        manager.show();

        auto* tabs = manager.findChild<QTabWidget*>(QStringLiteral("mainTabs"));
        auto* startTime = manager.findChild<QDateTimeEdit*>(
            QStringLiteral("expeditionStartTime"));
        QVERIFY(tabs != nullptr);
        QVERIFY(startTime != nullptr);

        tabs->setCurrentIndex(7);
        QCoreApplication::processEvents();
        const QDateTime stale(QDate(2000, 1, 1), QTime(0, 0));
        startTime->setDateTime(stale);
        QTest::mouseClick(
            tabs->tabBar(), Qt::LeftButton, Qt::NoModifier,
            tabs->tabBar()->tabRect(7).center());
        QVERIFY(startTime->dateTime() != stale);

        startTime->setDateTime(stale);
        QTest::mouseClick(
            tabs->tabBar(), Qt::LeftButton, Qt::NoModifier,
            tabs->tabBar()->tabRect(7).center());
        QVERIFY(startTime->dateTime() != stale);
        QCOMPARE(startTime->dateTime().time().second(), 0);
    }

    void configurationPagesDisplayJsonDataAndExpectedColumns()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path();
        JsonConfigStore store(path);
        AppConfig config;
        config.regionPresets.append({QStringLiteral("扫描窗口"), QRect(10, 20, 300, 180)});
        config.colorRules.append({
            4, QStringLiteral("护盾"), QStringLiteral("红色"), QStringLiteral(">=20"),
            TriggerFeedback::Popup, QString{},
            PostTriggerAction::Loop});
        config.colorRules[0].triggerCondition2 = {
            QStringLiteral("白色"), QStringLiteral("<=5")};
        config.colorRules[0].triggerConditionRelation = TriggerConditionRelation::Or;
        QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));

        NativeGateway gateway(functions());
        MainWindow manager(gateway, path);

        auto* presets = manager.findChild<QTableWidget*>(QStringLiteral("regionPresetTable"));
        QVERIFY(presets != nullptr);
        QCOMPARE(presets->rowCount(), 1);
        QCOMPARE(presets->columnCount(), 7);
        QCOMPARE(presets->item(0, 0)->text(), QStringLiteral("扫描窗口"));
        QCOMPARE(presets->item(0, 1)->text(), QStringLiteral("10"));
        QCOMPARE(presets->item(0, 4)->text(), QStringLiteral("180"));
        auto* interval = qobject_cast<QComboBox*>(presets->cellWidget(0, 5));
        QVERIFY(interval != nullptr);
        QCOMPARE(interval->currentData().toInt(), 1500);
        QCOMPARE(interval->itemData(0).toInt(), 0);
        QCOMPARE(interval->itemData(1).toInt(), 1500);
        QCOMPARE(interval->findData(500), -1);
        QCOMPARE(interval->findData(1000), -1);
        auto* rowDelete = qobject_cast<QPushButton*>(presets->cellWidget(0, 6));
        QVERIFY(rowDelete != nullptr);
        QCOMPARE(rowDelete->text(), QStringLiteral("删除"));

        auto* rules = manager.findChild<QTableWidget*>(QStringLiteral("colorRuleTable"));
        QVERIFY(rules != nullptr);
        QCOMPARE(rules->rowCount(), 1);
        QCOMPARE(rules->columnCount(), 7);
        const QStringList headers{
            QStringLiteral("编号"), QStringLiteral("备注"), QStringLiteral("区域条件槽"),
            QStringLiteral("条件关系"), QStringLiteral("状态切换设置"),
            QStringLiteral("触发反馈"), QStringLiteral("触发后效果")};
        for (int column = 0; column < headers.size(); ++column) {
            QCOMPARE(rules->horizontalHeaderItem(column)->text(), headers[column]);
        }
        QCOMPARE(rules->item(0, 0)->text(), QStringLiteral("4"));
        QCOMPARE(rules->item(0, 1)->text(), QStringLiteral("护盾"));
        QVERIFY(rules->item(0, 2)->text().isEmpty());
        QCOMPARE(rules->item(0, 3)->text(), QStringLiteral("或（任一满足）"));
        QCOMPARE(rules->item(0, 4)->text(), QStringLiteral("无"));
        QCOMPARE(rules->item(0, 5)->text(), QStringLiteral("弹窗"));
        QCOMPARE(rules->item(0, 6)->text(), QStringLiteral("正向循环"));

        QVERIFY(manager.findChild<QPushButton*>(QStringLiteral("addColorRule")) != nullptr);
        QVERIFY(manager.findChild<QPushButton*>(QStringLiteral("editColorRule")) != nullptr);
        QVERIFY(manager.findChild<QPushButton*>(QStringLiteral("deleteColorRule")) != nullptr);
        QVERIFY(manager.findChild<QPushButton*>(QStringLiteral("renameRegionPreset")) == nullptr);
        QVERIFY(manager.findChild<QPushButton*>(QStringLiteral("deleteRegionPreset")) == nullptr);
        auto* saveConfiguration = manager.findChild<QPushButton*>(
            QStringLiteral("saveRegionPresetConfiguration"));
        QVERIFY(saveConfiguration != nullptr);
        QCOMPARE(saveConfiguration->text(), QStringLiteral("保存配置"));
    }

    void replicaRowSourceComboListsWindowsAndExposesState()
    {
        ReplicaRow row(1);
        auto* combo = row.findChild<QComboBox*>(QStringLiteral("sourceCombo"));
        QVERIFY(combo != nullptr);
        QCOMPARE(combo->itemText(0), QStringLiteral("尚未选择窗口"));
        QCOMPARE(combo->count(), 1);

        row.setSourceOptions({
            WindowInfo{100, QStringLiteral("EVE Online - Alpha")},
            WindowInfo{200, QStringLiteral("EVE - MysteriousmanA")},
            WindowInfo{300, QStringLiteral("星战前夜：晨曦[Infinity] - 噩梦级")},
        });
        QCOMPARE(combo->count(), 4);
        QCOMPARE(combo->itemText(1), QStringLiteral("Alpha"));
        QCOMPARE(combo->itemText(2), QStringLiteral("MysteriousmanA"));
        QCOMPARE(combo->itemText(3), QStringLiteral("噩梦级"));
        QCOMPARE(
            combo->itemData(2).value<WindowInfo>().title,
            QStringLiteral("EVE - MysteriousmanA"));

        row.setSourceTitle(QStringLiteral("EVE - MysteriousmanB"));
        QCOMPARE(combo->currentText(), QStringLiteral("MysteriousmanB"));
        row.setSourceOptions({});
        QCOMPARE(combo->currentText(), QStringLiteral("MysteriousmanB"));

        row.setSourceOptions({
            WindowInfo{300, QStringLiteral("MysteriousmanB")},
        });
        QCOMPARE(combo->currentText(), QStringLiteral("MysteriousmanB"));
        QCOMPARE(combo->currentData().value<WindowInfo>().hwnd, std::uintptr_t{0});

        QVERIFY(row.visibilityChecked());
        QCOMPARE(row.colorLogicSlotRule(1), 0);
        QCOMPARE(row.colorLogicSlotRule(2), 0);
    }

    void sourceComboSelectionBindsSourceToReplica()
    {
        mockWindowCount = 2;
        setMockWindow(0, 100, L"EVE Online - Alpha");
        setMockWindow(1, 200, L"EVE - MysteriousmanA");

        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        manager.show();
        auto* window = manager.createReplica();
        auto* row = manager.replicaRows().first();
        auto* combo = row->findChild<QComboBox*>(QStringLiteral("sourceCombo"));
        QVERIFY(combo != nullptr);
        QCOMPARE(combo->count(), 3);
        QVERIFY(!window->source().has_value());

        QMetaObject::invokeMethod(combo, "activated", Qt::DirectConnection, Q_ARG(int, 1));
        QCoreApplication::processEvents();

        QVERIFY(window->source().has_value());
        QCOMPARE(window->source()->hwnd, std::uintptr_t{100});
        QCOMPARE(window->source()->title, QStringLiteral("EVE Online - Alpha"));

        mockWindowCount = 0;
    }

    void replicaRowOffersCopyButton()
    {
        ReplicaRow row(1);
        auto* close = row.findChild<QPushButton*>(QStringLiteral("closeButton"));
        auto* copy = row.findChild<QPushButton*>(QStringLiteral("copyReplica"));
        QVERIFY(close != nullptr);
        QVERIFY(copy != nullptr);
        QCOMPARE(copy->text(), QStringLiteral("复制"));

        QSignalSpy copyRequested(&row, &ReplicaRow::copyRequested);
        QTest::mouseClick(copy, Qt::LeftButton);
        QCOMPARE(copyRequested.count(), 1);
    }

    void copyButtonDuplicatesReplicaWithConfiguration()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path();
        JsonConfigStore store(path);
        AppConfig config;
        config.colorRules.append({1, QStringLiteral("红"), QStringLiteral("红色"), QStringLiteral(">=2"),
                                  TriggerFeedback::None, QString{}, PostTriggerAction::Loop});
        QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));

        NativeGateway gateway(functions());
        MainWindow manager(gateway, path);
        manager.show();
        auto* original = manager.createReplica();
        auto* row = manager.replicaRows().first();
        auto* interval = row->findChild<QComboBox*>(QStringLiteral("statisticsInterval"));
        auto* slotA = row->findChild<QComboBox*>(QStringLiteral("colorLogicSlotA"));
        auto* enabled = row->findChild<QCheckBox*>(QStringLiteral("colorLogicEnabled"));
        QVERIFY(interval != nullptr && slotA != nullptr && enabled != nullptr);

        original->selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});
        interval->setCurrentIndex(interval->findData(1500));
        slotA->setCurrentIndex(slotA->findData(1));
        enabled->setChecked(true);

        auto* copyButton = row->findChild<QPushButton*>(QStringLiteral("copyReplica"));
        QVERIFY(copyButton != nullptr);
        QTest::mouseClick(copyButton, Qt::LeftButton);
        QCoreApplication::processEvents();

        QCOMPARE(manager.activeReplicaCount(), std::size_t{2});
        ReplicaWindow* copy = nullptr;
        for (auto* window : manager.replicaWindows()) {
            if (window->replicaId() != original->replicaId()) {
                copy = window;
            }
        }
        QVERIFY(copy != nullptr);
        QVERIFY(copy->source().has_value());
        QCOMPARE(copy->source()->hwnd, std::uintptr_t{123});
        QCOMPARE(copy->statisticsIntervalMs(), 1500);
        QVERIFY(!copy->colorLogicEnabled());

        ReplicaRow* copyRow = nullptr;
        for (auto* r : manager.replicaRows()) {
            if (r->replicaId() == copy->replicaId()) {
                copyRow = r;
            }
        }
        QVERIFY(copyRow != nullptr);
        QCOMPARE(copyRow->colorLogicSlotRule(1), 0);
        auto* copyCombo = copyRow->findChild<QComboBox*>(QStringLiteral("sourceCombo"));
        QVERIFY(copyCombo != nullptr);
        QCOMPARE(copyCombo->currentText(), QStringLiteral("Alpha"));
        auto* copyInterval = copyRow->findChild<QComboBox*>(QStringLiteral("statisticsInterval"));
        QVERIFY(copyInterval != nullptr);
        QCOMPARE(copyInterval->currentData().toInt(), 1500);
    }

    void borderFlashDimmsNonTriggeredReplicas()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path();
        JsonConfigStore store(path);
        AppConfig config;
        config.colorRules.append({1, QStringLiteral("红"), QStringLiteral("红色"), QStringLiteral(">=2"),
                                  TriggerFeedback::None, QString{}, PostTriggerAction::Loop});
        QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));

        mockCaptureWidth = 2;
        mockCaptureHeight = 1;
        mockCaptureColors = {qRgb(255, 0, 0), qRgb(255, 0, 0)};
        NativeGateway gateway(functions());
        MainWindow manager(gateway, path);
        manager.show();
        auto* first = manager.createReplica();
        auto* second = manager.createReplica();

        ReplicaRow* firstRow = nullptr;
        for (auto* row : manager.replicaRows()) {
            if (row->replicaId() == first->replicaId()) {
                firstRow = row;
            }
        }
        QVERIFY(firstRow != nullptr);
        auto* slotA = firstRow->findChild<QComboBox*>(QStringLiteral("colorLogicSlotA"));
        auto* enabled = firstRow->findChild<QCheckBox*>(QStringLiteral("colorLogicEnabled"));
        auto* interval = firstRow->findChild<QComboBox*>(QStringLiteral("statisticsInterval"));
        QVERIFY(slotA != nullptr && enabled != nullptr && interval != nullptr);
        slotA->setCurrentIndex(slotA->findData(1));
        enabled->setChecked(true);
        bindGlobalRule(manager, first, config.colorRules.first());
        interval->setCurrentIndex(interval->findData(1500));
        first->selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});
        second->selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});

        QTRY_VERIFY_WITH_TIMEOUT(first->isFlashActive(), 2000);
        QCOMPARE(second->grab().toImage().pixelColor(1, 1), QColor(0, 0, 0));

        mockCaptureColors = {qRgb(0, 0, 0), qRgb(0, 0, 0)};
        QTRY_VERIFY_WITH_TIMEOUT(!first->isFlashActive(), 2000);
        QCOMPARE(second->grab().toImage().pixelColor(1, 1), QColor(255, 255, 255));

        mockCaptureWidth = 0;
        mockCaptureHeight = 0;
        mockCaptureColors.clear();
    }

    void replicaRowShowsAndMovesWindowPosition()
    {
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        manager.show();
        auto* window = manager.createReplica();
        auto* row = manager.replicaRows().first();
        auto* positionEdit = row->findChild<QLineEdit*>(QStringLiteral("windowPosition"));
        QVERIFY(positionEdit != nullptr);
        QCOMPARE(positionEdit->text(), QStringLiteral("%1,%2").arg(window->pos().x()).arg(window->pos().y()));

        window->move(300, 200);
        QCoreApplication::processEvents();
        QCOMPARE(positionEdit->text(), QStringLiteral("300,200"));

        positionEdit->setText(QStringLiteral("100,50"));
        QTest::keyClick(positionEdit, Qt::Key_Return);
        QCoreApplication::processEvents();
        QCOMPARE(window->pos(), QPoint(100, 50));

        positionEdit->setText(QStringLiteral("abc"));
        QTest::keyClick(positionEdit, Qt::Key_Return);
        QCOMPARE(positionEdit->text(), QStringLiteral("100,50"));
    }

    void replicaRowShowsCurrentSizeAndScaleFromCapturedRegion()
    {
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        manager.show();
        auto* window = manager.createReplica();
        auto* row = manager.replicaRows().first();
        auto* sizeEdit = row->findChild<QLineEdit*>(QStringLiteral("windowSize"));
        auto* scaleEdit = row->findChild<QDoubleSpinBox*>(QStringLiteral("windowScale"));
        QVERIFY(sizeEdit != nullptr && scaleEdit != nullptr);
        QVERIFY(sizeEdit->isReadOnly());
        QVERIFY(!scaleEdit->isReadOnly());
        QVERIFY(!scaleEdit->isEnabled());
        QCOMPARE(sizeEdit->text(), QStringLiteral("360×240"));
        QCOMPARE(scaleEdit->value(), 0.0);

        window->selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});
        window->setCapturedRegion(QRect(0, 0, 300, 200));
        QCoreApplication::processEvents();
        QVERIFY(scaleEdit->isEnabled());
        QCOMPARE(sizeEdit->text(), QStringLiteral("360×240"));
        QCOMPARE(scaleEdit->value(), 120.0);

        window->resize(600, 400);
        QCoreApplication::processEvents();
        QCOMPARE(sizeEdit->text(), QStringLiteral("600×400"));
        QCOMPARE(scaleEdit->value(), 200.0);
    }

    void replicaRowsFollowDragOrderAndPersistIt()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path();
        NativeGateway gateway(functions());
        MainWindow manager(gateway, path);
        auto* firstWindow = manager.createReplica();
        auto* secondWindow = manager.createReplica();
        auto* thirdWindow = manager.createReplica();
        firstWindow->move(100, 10);
        secondWindow->move(200, 20);
        thirdWindow->move(300, 30);
        QCoreApplication::processEvents();

        const auto initialRows = manager.replicaRows();
        QCOMPARE(initialRows.size(), 3);
        QCOMPARE(initialRows[0]->replicaId(), firstWindow->replicaId());
        QCOMPARE(initialRows[1]->replicaId(), secondWindow->replicaId());
        QCOMPARE(initialRows[2]->replicaId(), thirdWindow->replicaId());

        initialRows[2]->reorderRequested(
            thirdWindow->replicaId(), firstWindow->replicaId(), false);
        QCoreApplication::processEvents();
        const auto reorderedRows = manager.replicaRows();
        QCOMPARE(reorderedRows[0]->replicaId(), thirdWindow->replicaId());
        QCOMPARE(reorderedRows[1]->replicaId(), firstWindow->replicaId());
        QCOMPARE(reorderedRows[2]->replicaId(), secondWindow->replicaId());

        QTest::qWait(650);
        JsonConfigStore store(path);
        AppConfig persisted;
        QString error;
        QVERIFY2(store.load(persisted, &error), qPrintable(error));
        QCOMPARE(persisted.replicas.size(), 3);
        QCOMPARE(persisted.replicas[0].position, QPoint(300, 30));
        QCOMPARE(persisted.replicas[1].position, QPoint(100, 10));
        QCOMPARE(persisted.replicas[2].position, QPoint(200, 20));
    }

    void replicasWithSameSourceFormCollapsibleGroup()
    {
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        manager.show();
        auto* first = manager.createReplica();
        auto* second = manager.createReplica();
        auto* third = manager.createReplica();
        auto* fourth = manager.createReplica();
        const WindowInfo alpha{123, QStringLiteral("EVE Online - Alpha")};
        const WindowInfo beta{456, QStringLiteral("EVE Online - Beta")};
        first->selectSource(alpha);
        second->selectSource(alpha);
        third->selectSource(beta);
        fourth->selectSource(alpha);
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();

        const auto rows = manager.replicaRows();
        QCOMPARE(rows.size(), 4);
        int visibleSourceCells = 0;
        for (int index = 0; index < 2; ++index) {
            auto* details = rows[index]->findChild<QWidget*>(
                QStringLiteral("sourceWindowDetails"));
            QVERIFY(details != nullptr);
            visibleSourceCells += !details->isHidden();
        }
        QCOMPARE(visibleSourceCells, 1);
        auto* groupCount = rows[1]->findChild<QLabel*>(QStringLiteral("sourceGroupCount"));
        QVERIFY(groupCount != nullptr);
        QCOMPARE(groupCount->text(), QStringLiteral("3 个小窗"));
        QVERIFY(!groupCount->isHidden());
        QCOMPARE(rows[1]->sourceCombo()->currentText(), QStringLiteral("Alpha"));
        auto* expandedSourceCell = rows[1]->findChild<QWidget*>(
            QStringLiteral("sourceWindowCell"));
        QVERIFY(expandedSourceCell != nullptr);
        QCOMPARE(expandedSourceCell->minimumWidth(), 190);
        QCOMPARE(expandedSourceCell->maximumWidth(), 190);
        QCOMPARE(
            manager.findChildren<QFrame*>(QStringLiteral("sourceGroupBar")).size(),
            1);

        auto* collapseButton = rows[1]->findChild<QPushButton*>(
            QStringLiteral("collapseReplica"));
        QVERIFY(collapseButton != nullptr);
        collapseButton->click();
        QCoreApplication::processEvents();
        QVERIFY(rows[0]->collapsed());
        QVERIFY(rows[1]->collapsed());
        QCOMPARE(static_cast<int>(!rows[0]->isHidden()) + static_cast<int>(!rows[1]->isHidden()), 1);

        auto* visibleRow = rows[0]->isHidden() ? rows[1] : rows[0];
        auto* collapsedSummary = visibleRow->findChild<QWidget*>(
            QStringLiteral("sourceWindowDetails"));
        QVERIFY(collapsedSummary != nullptr);
        QCOMPARE(collapsedSummary->minimumWidth(), 380);
        QCOMPARE(collapsedSummary->maximumWidth(), 380);
        QCOMPARE(visibleRow->sourceCombo()->currentText(), QStringLiteral("Alpha"));
        auto* expandButton = visibleRow->findChild<QPushButton*>(
            QStringLiteral("collapseReplica"));
        expandButton->click();
        QCoreApplication::processEvents();
        QVERIFY(!rows[0]->collapsed());
        QVERIFY(!rows[1]->collapsed());
        QVERIFY(!rows[0]->isHidden() && !rows[1]->isHidden());
        QCOMPARE(rows[1]->sourceCombo()->currentText(), QStringLiteral("Alpha"));
        rows[1]->setSourceOptions({});
        QCOMPARE(rows[1]->sourceCombo()->currentText(), QStringLiteral("Alpha"));
        auto* expandedSummary = rows[1]->findChild<QWidget*>(
            QStringLiteral("sourceWindowDetails"));
        QVERIFY(expandedSummary->maximumWidth() > 380);

        rows[3]->reorderRequested(fourth->replicaId(), third->replicaId(), false);
        QCoreApplication::processEvents();
        const auto regrouped = manager.replicaRows();
        QCOMPARE(regrouped[0]->replicaId(), first->replicaId());
        QCOMPARE(regrouped[1]->replicaId(), second->replicaId());
        QCOMPARE(regrouped[2]->replicaId(), fourth->replicaId());
        int sourceCellsInThreeMemberGroup = 0;
        for (int index = 0; index < 3; ++index) {
            auto* details = regrouped[index]->findChild<QWidget*>(
                QStringLiteral("sourceWindowDetails"));
            sourceCellsInThreeMemberGroup += !details->isHidden();
        }
        QCOMPARE(sourceCellsInThreeMemberGroup, 1);
        auto* threeCount = regrouped[1]->findChild<QLabel*>(
            QStringLiteral("sourceGroupCount"));
        QCOMPARE(threeCount->text(), QStringLiteral("3 个小窗"));
        QVERIFY(!threeCount->isHidden());
    }

    void replicaRowRenamesSwitchesAndListsPresets()
    {
        ReplicaRow row(1);
        auto* save = row.findChild<QPushButton*>(QStringLiteral("saveRegionPreset"));
        QVERIFY(save != nullptr);
        QCOMPARE(save->text(), QStringLiteral("保存为新区域预设"));
        auto* presetCombo = row.findChild<QComboBox*>(QStringLiteral("currentRegionPreset"));
        QVERIFY(presetCombo != nullptr);
        QCOMPARE(presetCombo->itemText(0), QStringLiteral("自定义"));
        QCOMPARE(presetCombo->count(), 1);

        row.setRegionPresets({{QStringLiteral("总览"), QRect(120, 80, 640, 480), 1500}});
        QCOMPARE(presetCombo->count(), 2);
        QCOMPARE(presetCombo->itemText(1), QStringLiteral("总览"));

        row.setCurrentRegion(QRect(120, 80, 640, 480));
        QCOMPARE(presetCombo->currentIndex(), 1);
        row.setCurrentRegion(QRect(1, 1, 10, 10));
        QCOMPARE(presetCombo->currentIndex(), 0);
    }

    void regionPresetComboToleratesTinySizeDifference()
    {
        ReplicaRow row(1);
        auto* presetCombo = row.findChild<QComboBox*>(QStringLiteral("currentRegionPreset"));
        QVERIFY(presetCombo != nullptr);
        row.setRegionPresets({{QStringLiteral("总览"), QRect(120, 80, 640, 480), 1500}});

        row.setCurrentRegion(QRect(120, 80, 639, 480));
        QCOMPARE(presetCombo->currentIndex(), 1);
        row.setCurrentRegion(QRect(121, 80, 640, 479));
        QCOMPARE(presetCombo->currentIndex(), 1);
        row.setCurrentRegion(QRect(120, 80, 635, 480));
        QCOMPARE(presetCombo->currentIndex(), 0);
    }

    void regionPresetComboAppliesSelectedPresetToReplica()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path();
        JsonConfigStore store(path);
        AppConfig config;
        config.regionPresets.append({QStringLiteral("总览"), QRect(120, 80, 640, 480), 1500});
        QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));

        NativeGateway gateway(functions());
        MainWindow manager(gateway, path);
        manager.show();
        auto* window = manager.createReplica();
        auto* row = manager.replicaRows().first();
        auto* combo = row->findChild<QComboBox*>(QStringLiteral("currentRegionPreset"));
        QVERIFY(combo != nullptr);
        QCOMPARE(combo->count(), 2);
        QCOMPARE(combo->itemText(1), QStringLiteral("总览"));

        window->selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});
        QMetaObject::invokeMethod(combo, "activated", Qt::DirectConnection, Q_ARG(int, 1));
        QCoreApplication::processEvents();

        QVERIFY(window->capturedRegion().has_value());
        QCOMPARE(*window->capturedRegion(), QRect(120, 80, 640, 480));
        QCOMPARE(window->statisticsIntervalMs(), 1500);
        QCOMPARE(combo->currentIndex(), 1);
    }

    void cachedReplicasRestoreOnStartup()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path();
        JsonConfigStore store(path);
        AppConfig config;
        config.colorRules.append({
            1, QStringLiteral("红"), QStringLiteral("红色"), QStringLiteral(">=2"),
            TriggerFeedback::None, QString{}, PostTriggerAction::Loop});
        config.colorRules.append({
            2, QStringLiteral("蓝"), QStringLiteral("白色"), QStringLiteral(">=1"),
            TriggerFeedback::None, QString{}, PostTriggerAction::Loop});
        config.replicas.append({
            QStringLiteral("EVE Online - Alpha"),
            QRect(120, 80, 640, 480),
            1500,
            {1, 2},
            true,
            2,
            true,
            false,
            QPoint(300, 200),
            QSize(720, 480),
            QColor(255, 0, 0),
            QColor(0, 0, 255),
        });
        config.replicas.append({
            QStringLiteral("EVE - MysteriousmanA"),
            QRect{},
            500,
            {2, 0},
            true,
            1,
            false,
            false,
            QPoint(100, 100),
            QSize(360, 240),
            QColor(46, 204, 64),
            QColor(41, 121, 255),
        });
        QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));

        NativeGateway gateway(functions());
        MainWindow manager(gateway, path);
        manager.show();
        QCoreApplication::processEvents();

        QCOMPARE(manager.activeReplicaCount(), std::size_t{2}); // Migrated logic reuses the cached windows.
        QCOMPARE(manager.replicaRows().size(), 2);

        ReplicaWindow* first = nullptr;
        ReplicaWindow* second = nullptr;
        ReplicaRow* firstRow = nullptr;
        ReplicaRow* secondRow = nullptr;
        for (auto* window : manager.replicaWindows()) {
            if (window->replicaId() == 1) {
                first = window;
            } else if (window->replicaId() == 2) {
                second = window;
            }
        }
        for (auto* row : manager.replicaRows()) {
            if (row->replicaId() == 1) {
                firstRow = row;
            } else if (row->replicaId() == 2) {
                secondRow = row;
            }
        }
        QVERIFY(first != nullptr && second != nullptr);
        QVERIFY(firstRow != nullptr && secondRow != nullptr);

        QCOMPARE(first->statisticsIntervalMs(), 1500);
        QVERIFY(!first->colorLogicEnabled());
        QCOMPARE(first->activeColorLogicSlot(), 1);
        QVERIFY(firstRow->visibilityChecked());
        QCOMPARE(first->pos(), QPoint(300, 200));
        QCOMPARE(first->size(), QSize(720, 480));
        QCOMPARE(firstRow->colorLogicSlotRule(1), 0);
        QCOMPARE(firstRow->colorLogicSlotRule(2), 0);
        QCOMPARE(firstRow->findChild<QLabel*>(QStringLiteral("colorLogicSummary"))->text(),
            QString{}); // Definitions now refer to presets, not ordinary windows.

        QCOMPARE(second->statisticsIntervalMs(), 1500);
        QVERIFY(!secondRow->visibilityChecked());
        QCOMPARE(second->pos(), QPoint(100, 100));
        QCOMPARE(second->size(), QSize(360, 240));
        QCOMPARE(secondRow->colorLogicSlotRule(1), 0);
        QCOMPARE(secondRow->colorLogicSlotRule(2), 0);
    }

    void createdReplicasPersistAndRestore()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path();
        {
            NativeGateway gateway(functions());
            MainWindow manager(gateway, path);
            manager.show();
            auto* window = manager.createReplica();
            auto* row = manager.replicaRows().first();
            auto* interval = row->findChild<QComboBox*>(QStringLiteral("statisticsInterval"));
            QVERIFY(interval != nullptr);
            interval->setCurrentIndex(interval->findData(1500));
            window->resize(720, 480);
            window->move(300, 200);
            QCoreApplication::processEvents();
            manager.close();
            QCoreApplication::processEvents();
        }
        {
            NativeGateway gateway(functions());
            MainWindow manager(gateway, path);
            manager.show();
            QCOMPARE(manager.activeReplicaCount(), std::size_t{1});
            auto* window = manager.replicaWindows().first();
            QCOMPARE(window->statisticsIntervalMs(), 1500);
            auto* restoredInterval = manager.replicaRows().first()->findChild<QComboBox*>(
                QStringLiteral("statisticsInterval"));
            QVERIFY(restoredInterval != nullptr);
            QCOMPARE(restoredInterval->currentData().toInt(), 1500);
            QCOMPARE(window->pos(), QPoint(300, 200));
            QCOMPARE(window->size(), QSize(720, 480));
        }
    }

    void replicaListScrollsWithoutCompressingRows()
    {
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        manager.show();
        auto* scroll = manager.findChild<QScrollArea*>(QStringLiteral("replicaListScroll"));
        QVERIFY(scroll != nullptr);

        for (int index = 0; index < 10; ++index) {
            static_cast<void>(manager.createReplica());
        }
        QCoreApplication::processEvents();
        manager.resize(manager.width(), 500);
        QCoreApplication::processEvents();

        QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
        for (auto* row : manager.replicaRows()) {
            QVERIFY(row->height() >= row->sizeHint().height());
        }
    }

    void replicaListWheelOverComboScrollsPageWithoutChangingSelection()
    {
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        manager.show();
        auto* tabs = manager.findChild<QTabWidget*>(QStringLiteral("mainTabs"));
        QVERIFY(tabs != nullptr);
        tabs->setCurrentIndex(1);
        for (int index = 0; index < 10; ++index) {
            static_cast<void>(manager.createReplica());
        }
        QCoreApplication::processEvents();

        auto* scroll = manager.findChild<QScrollArea*>(QStringLiteral("replicaListScroll"));
        QVERIFY(scroll != nullptr);
        QVERIFY(scroll->verticalScrollBar()->maximum() > 0);

        auto* row = manager.replicaRows().first();
        row->setSourceOptions({
            WindowInfo{1, QStringLiteral("EVE - Alpha")},
            WindowInfo{2, QStringLiteral("EVE - Beta")},
        });
        row->setRegionPresets({
            RegionPreset{QStringLiteral("区域 1"), QRect(0, 0, 10, 10), 1000},
            RegionPreset{QStringLiteral("区域 2"), QRect(10, 10, 20, 20), 1500},
        });
        row->setColorRules({
            ColorRule{1, QStringLiteral("红"), {}, {}, TriggerFeedback::None, {},
                      PostTriggerAction::Loop},
            ColorRule{2, QStringLiteral("蓝"), {}, {}, TriggerFeedback::None, {},
                      PostTriggerAction::Loop},
        });
        QCoreApplication::processEvents();

        const QList<QComboBox*> combos{
            row->findChild<QComboBox*>(QStringLiteral("sourceCombo")),
            row->findChild<QComboBox*>(QStringLiteral("currentRegionPreset")),
            row->findChild<QComboBox*>(QStringLiteral("statisticsInterval")),
        };
        for (auto* combo : combos) {
            QVERIFY(combo != nullptr);
            QVERIFY(combo->count() > 1);
            combo->setCurrentIndex(1);
        }

        scroll->verticalScrollBar()->setValue(0);
        for (auto* combo : combos) {
            const int selectedIndex = combo->currentIndex();
            const QPoint localPosition = combo->rect().center();
            QTest::wheelEvent(
                manager.windowHandle(),
                combo->mapTo(&manager, localPosition),
                QPoint(0, -120));

            QCOMPARE(combo->currentIndex(), selectedIndex);
        }
        QVERIFY(scroll->verticalScrollBar()->value() > 0);
    }

    void popupRevealsHiddenWindowWhileOthersDim()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path();
        JsonConfigStore store(path);
        AppConfig config;
        config.colorRules.append({1, QStringLiteral("红"), QStringLiteral("红色"), QStringLiteral(">=2"),
                                  TriggerFeedback::Popup, QString{}, PostTriggerAction::Loop});
        QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));

        mockCaptureWidth = 2;
        mockCaptureHeight = 1;
        mockCaptureColors = {qRgb(255, 0, 0), qRgb(255, 0, 0)};
        NativeGateway gateway(functions());
        MainWindow manager(gateway, path);
        manager.show();
        auto* triggered = manager.createReplica();
        auto* other = manager.createReplica();

        ReplicaRow* triggeredRow = nullptr;
        for (auto* row : manager.replicaRows()) {
            if (row->replicaId() == triggered->replicaId()) {
                triggeredRow = row;
            }
        }
        QVERIFY(triggeredRow != nullptr);
        auto* slotA = triggeredRow->findChild<QComboBox*>(QStringLiteral("colorLogicSlotA"));
        auto* enabled = triggeredRow->findChild<QCheckBox*>(QStringLiteral("colorLogicEnabled"));
        auto* interval = triggeredRow->findChild<QComboBox*>(QStringLiteral("statisticsInterval"));
        auto* visibility = triggeredRow->findChild<QCheckBox*>(QStringLiteral("replicaVisibility"));
        QVERIFY(slotA != nullptr && enabled != nullptr && interval != nullptr && visibility != nullptr);
        slotA->setCurrentIndex(slotA->findData(1));
        enabled->setChecked(true);
        bindGlobalRule(manager, triggered, config.colorRules.first());
        interval->setCurrentIndex(interval->findData(1500));
        triggered->selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});
        other->selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});
        visibility->setChecked(false);
        QVERIFY(!triggered->isVisible());

        QTRY_VERIFY_WITH_TIMEOUT(triggered->isVisible(), 2000);
        QCOMPARE(other->grab().toImage().pixelColor(1, 1), QColor(0, 0, 0));

        mockCaptureWidth = 0;
        mockCaptureHeight = 0;
        mockCaptureColors.clear();
    }

    void flashDimmingScopesToSameRegionPresetGroup()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path();
        JsonConfigStore store(path);
        AppConfig config;
        config.colorRules.append({1, QStringLiteral("红"), QStringLiteral("红色"), QStringLiteral(">=2"),
                                  TriggerFeedback::None, QString{}, PostTriggerAction::Loop});
        config.regionPresets.append({QStringLiteral("总览"), QRect(0, 0, 2, 1), 1000});
        QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));

        mockCaptureWidth = 2;
        mockCaptureHeight = 1;
        mockCaptureColors = {qRgb(255, 0, 0), qRgb(255, 0, 0)};
        NativeGateway gateway(functions());
        MainWindow manager(gateway, path);
        manager.show();
        auto* a1 = manager.createReplica();
        auto* a2 = manager.createReplica();
        auto* b = manager.createReplica();

        ReplicaRow* a1Row = nullptr;
        ReplicaRow* a2Row = nullptr;
        for (auto* row : manager.replicaRows()) {
            if (row->replicaId() == a1->replicaId()) {
                a1Row = row;
            }
            if (row->replicaId() == a2->replicaId()) {
                a2Row = row;
            }
        }
        QVERIFY(a1Row != nullptr && a2Row != nullptr);

        const WindowInfo source{123, QStringLiteral("EVE Online - Alpha")};
        a1->selectSource(source);
        a2->selectSource(source);
        b->selectSource(source);
        a1->applyRegionPreset(config.regionPresets.first());
        a2->applyRegionPreset(config.regionPresets.first());
        b->setCapturedRegion(QRect(3, 0, 1, 1));
        QCOMPARE(a1Row->currentRegionPresetName(), QStringLiteral("总览"));
        QCOMPARE(a2Row->currentRegionPresetName(), QStringLiteral("总览"));

        auto* slotA = a1Row->findChild<QComboBox*>(QStringLiteral("colorLogicSlotA"));
        auto* enabled = a1Row->findChild<QCheckBox*>(QStringLiteral("colorLogicEnabled"));
        auto* interval = a1Row->findChild<QComboBox*>(QStringLiteral("statisticsInterval"));
        QVERIFY(slotA != nullptr && enabled != nullptr && interval != nullptr);
        slotA->setCurrentIndex(slotA->findData(1));
        enabled->setChecked(true);
        bindGlobalRule(manager, a1, config.colorRules.first());
        interval->setCurrentIndex(interval->findData(1500));

        QTRY_VERIFY_WITH_TIMEOUT(a1->isFlashActive(), 2000);
        QCOMPARE(a2->grab().toImage().pixelColor(1, 1), QColor(0, 0, 0));
        QCOMPARE(b->grab().toImage().pixelColor(1, 1), QColor(255, 255, 255));

        mockCaptureWidth = 0;
        mockCaptureHeight = 0;
        mockCaptureColors.clear();
    }

    void flashDimmingGroupsIdenticalCustomRegions()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path();
        JsonConfigStore store(path);
        AppConfig config;
        config.colorRules.append({1, QStringLiteral("红"), QStringLiteral("红色"), QStringLiteral(">=2"),
                                  TriggerFeedback::None, QString{}, PostTriggerAction::Loop});
        QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));

        mockCaptureWidth = 2;
        mockCaptureHeight = 1;
        mockCaptureColors = {qRgb(255, 0, 0), qRgb(255, 0, 0)};
        NativeGateway gateway(functions());
        MainWindow manager(gateway, path);
        manager.show();
        auto* first = manager.createReplica();
        auto* second = manager.createReplica();
        auto* other = manager.createReplica();

        ReplicaRow* firstRow = nullptr;
        for (auto* row : manager.replicaRows()) {
            if (row->replicaId() == first->replicaId()) {
                firstRow = row;
            }
        }
        QVERIFY(firstRow != nullptr);

        const WindowInfo source{123, QStringLiteral("EVE Online - Alpha")};
        first->selectSource(source);
        second->selectSource(source);
        other->selectSource(source);
        first->setCapturedRegion(QRect(0, 0, 2, 1));
        second->setCapturedRegion(QRect(0, 0, 2, 1));
        other->setCapturedRegion(QRect(0, 0, 1, 1));
        QVERIFY(firstRow->currentRegionPresetName().isEmpty());

        auto* slotA = firstRow->findChild<QComboBox*>(QStringLiteral("colorLogicSlotA"));
        auto* enabled = firstRow->findChild<QCheckBox*>(QStringLiteral("colorLogicEnabled"));
        auto* interval = firstRow->findChild<QComboBox*>(QStringLiteral("statisticsInterval"));
        QVERIFY(slotA != nullptr && enabled != nullptr && interval != nullptr);
        slotA->setCurrentIndex(slotA->findData(1));
        enabled->setChecked(true);
        bindGlobalRule(manager, first, config.colorRules.first());
        interval->setCurrentIndex(interval->findData(1500));

        QTRY_VERIFY_WITH_TIMEOUT(first->isFlashActive(), 2000);
        QCOMPARE(second->grab().toImage().pixelColor(1, 1), QColor(0, 0, 0));
        QCOMPARE(other->grab().toImage().pixelColor(1, 1), QColor(255, 255, 255));

        mockCaptureWidth = 0;
        mockCaptureHeight = 0;
        mockCaptureColors.clear();
    }

    void homePageOffersThreePresetBars()
    {
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        manager.show();
        for (int slot = 1; slot <= 3; ++slot) {
            QVERIFY(manager.findChild<QRadioButton*>(
                        QStringLiteral("presetDefault%1").arg(slot)) != nullptr);
            QVERIFY(manager.findChild<QPushButton*>(
                        QStringLiteral("renamePreset%1").arg(slot)) != nullptr);
            QVERIFY(manager.findChild<QPushButton*>(
                        QStringLiteral("savePreset%1").arg(slot)) != nullptr);
            QVERIFY(manager.findChild<QPushButton*>(
                        QStringLiteral("loadPreset%1").arg(slot)) != nullptr);
            QVERIFY(manager.findChild<QPushButton*>(
                        QStringLiteral("deletePreset%1").arg(slot)) != nullptr);
        }
    }

    void closeAllButtonClosesEveryReplica()
    {
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        manager.show();
        static_cast<void>(manager.createReplica());
        static_cast<void>(manager.createReplica());
        static_cast<void>(manager.createReplica());
        auto* closeAll = manager.findChild<QPushButton*>(QStringLiteral("closeAllButton"));
        QVERIFY(closeAll != nullptr);
        QCOMPARE(manager.activeReplicaCount(), std::size_t{3});

        QTimer::singleShot(0, &manager, [] {
            if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                if (auto* yes = box->button(QMessageBox::Yes)) {
                    yes->click();
                }
            }
        });
        QTest::mouseClick(closeAll, Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(manager.activeReplicaCount(), std::size_t{0}, 2000);
    }

    void desktopPetModesHideAndRestoreWindowsAndPartitionLines()
    {
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        auto* page = manager.findChild<DesktopPetPage*>();
        auto* partition = manager.findChild<PagePartitionPage*>();
        QVERIFY(page && partition);
        PagePartitionConfig lines;
        lines.lines = {{QStringLiteral("mode-test"), QPoint(50, 50), QPoint(150, 50)}};
        partition->setPartitionConfig(lines);
        const auto visibleLines = [] {
            int count = 0;
            for (auto* widget : QApplication::topLevelWidgets()) {
                if (widget->objectName() == QStringLiteral("pagePartitionOverlay") && widget->isVisible()) { ++count; }
            }
            return count;
        };
        auto* visible = manager.createReplica();
        auto* hidden = manager.createReplica();
        manager.replicaRows().last()->findChild<QCheckBox*>(QStringLiteral("replicaVisibility"))->setChecked(false);
        QVERIFY(visible->isVisible());
        QVERIFY(!hidden->isVisible());
        QVERIFY(visibleLines() > 0);
        page->setMode(DesktopPetMode::Fish);
        QVERIFY(!visible->isVisible());
        QCOMPARE(visibleLines(), 0);
        QVERIFY(partition->partitionConfig().visible);
        auto* createdWhileHidden = manager.createReplica();
        QVERIFY(!createdWhileHidden->isVisible());
        page->setMode(DesktopPetMode::DoNotDisturb);
        QVERIFY(!visible->isVisible());
        QCOMPARE(visibleLines(), 0);
        page->setMode(DesktopPetMode::Resident);
        QVERIFY(visible->isVisible());
        QVERIFY(createdWhileHidden->isVisible());
        QVERIFY(!hidden->isVisible());
        QVERIFY(visibleLines() > 0);
        QVERIFY(manager.replicaRows().first()->visibilityChecked());
    }

    void persistedDoNotDisturbModeAppliesAtStartup()
    {
        QTemporaryDir directory;
        DesktopPetSettingsStore petStore(directory.path());
        QVERIFY(petStore.save(QJsonObject{{QStringLiteral("mode"), QStringLiteral("doNotDisturb")}}));
        NativeGateway gateway(functions());
        MainWindow manager(gateway, directory.path());
        auto* page = manager.findChild<DesktopPetPage*>();
        QCOMPARE(page->mode(), DesktopPetMode::DoNotDisturb);
        auto* window = manager.createReplica();
        QVERIFY(!window->isVisible());
        page->setMode(DesktopPetMode::Resident);
        QVERIFY(window->isVisible());
    }

    void petStopSoftwareClosesEveryReplicaDespiteTray()
    {
        QTemporaryDir directory;
        JsonConfigStore store(directory.path());
        AppConfig config;
        config.keepInTray = true;
        QVERIFY(store.save(config));
        NativeGateway gateway(functions());
        MainWindow manager(gateway, directory.path());
        manager.show();
        static_cast<void>(manager.createReplica());
        auto* page = manager.findChild<DesktopPetPage*>();
        page->setMode(DesktopPetMode::DoNotDisturb);
        FloatingPetWindow* pet = nullptr;
        for (auto* widget : QApplication::topLevelWidgets()) {
            if (auto* candidate = qobject_cast<FloatingPetWindow*>(widget)) { pet = candidate; }
        }
        QVERIFY(pet);
        std::unique_ptr<QMenu> menu(pet->buildContextMenu());
        menu->findChild<QAction*>(QStringLiteral("desktopPetStopSoftware"))->trigger();
        QTRY_COMPARE(manager.activeReplicaCount(), std::size_t{0});
        QVERIFY(!manager.isVisible());
        const auto saved = DesktopPetSettingsStore(directory.path()).load();
        QCOMPARE(saved.settings.value(QStringLiteral("mode")).toString(), QStringLiteral("doNotDisturb"));
    }

    void forceQuitButtonClosesEveryReplicaDespiteTray()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path();
        JsonConfigStore store(path);
        AppConfig config;
        config.keepInTray = true;
        QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));

        NativeGateway gateway(functions());
        MainWindow manager(gateway, path);
        manager.show();
        auto* forceQuit = manager.findChild<QPushButton*>(QStringLiteral("forceQuitButton"));
        QVERIFY(forceQuit != nullptr);
        QVERIFY(forceQuit->isVisible());
        auto* tabs = manager.findChild<QTabWidget*>(QStringLiteral("mainTabs"));
        QVERIFY(tabs != nullptr);
        QVERIFY(!tabs->isAncestorOf(forceQuit));

        manager.resize(manager.minimumSize());
        QCoreApplication::processEvents();
        QVERIFY(forceQuit->isVisible());
        QVERIFY(forceQuit->height() > 0);

        static_cast<void>(manager.createReplica());
        static_cast<void>(manager.createReplica());
        QCOMPARE(manager.activeReplicaCount(), std::size_t{2});
        QTest::mouseClick(forceQuit, Qt::LeftButton);
        QCoreApplication::processEvents();
        QCOMPARE(manager.activeReplicaCount(), std::size_t{0});
    }

    void presetSaveAndLoadRestoresReplicas()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path();
        JsonConfigStore store(path);
        AppConfig config;
        QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));

        NativeGateway gateway(functions());
        MainWindow manager(gateway, path);
        manager.show();
        auto* window = manager.createReplica();
        auto* row = manager.replicaRows().first();
        auto* interval = row->findChild<QComboBox*>(QStringLiteral("statisticsInterval"));
        QVERIFY(interval != nullptr);
        interval->setCurrentIndex(interval->findData(1500));

        auto* saveButton = manager.findChild<QPushButton*>(QStringLiteral("savePreset1"));
        QVERIFY(saveButton != nullptr);
        QTest::mouseClick(saveButton, Qt::LeftButton);
        QCoreApplication::processEvents();

        window->close();
        QCoreApplication::processEvents();
        QCOMPARE(manager.activeReplicaCount(), std::size_t{0});

        auto* loadButton = manager.findChild<QPushButton*>(QStringLiteral("loadPreset1"));
        QVERIFY(loadButton != nullptr);
        QTest::mouseClick(loadButton, Qt::LeftButton);
        QCoreApplication::processEvents();

        QCOMPARE(manager.activeReplicaCount(), std::size_t{1});
        QCOMPARE(manager.replicaWindows().first()->statisticsIntervalMs(), 1500);
    }

    void startupPresetOverridesCachedGeometry_data()
    {
        QTest::addColumn<int>("defaultSlot");
        QTest::addColumn<bool>("hasFirstPreset");
        QTest::addColumn<int>("expectedSlot");
        QTest::newRow("unset-uses-preset-1") << 0 << true << 1;
        QTest::newRow("explicit-preset-1") << 1 << true << 1;
        QTest::newRow("explicit-preset-2") << 2 << true << 2;
        QTest::newRow("no-preset-1-keeps-cache") << 0 << false << 0;
        QTest::newRow("empty-default-keeps-cache") << 3 << true << 0;
    }

    void startupPresetOverridesCachedGeometry()
    {
        QFETCH(int, defaultSlot);
        QFETCH(bool, hasFirstPreset);
        QFETCH(int, expectedSlot);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        JsonConfigStore store(directory.path());
        AppConfig config;
        ReplicaConfig cached;
        cached.position = QPoint(30, 40);
        cached.size = QSize(410, 270);
        cached.statisticsIntervalMs = 3000;
        config.replicas = {cached};
        auto first = cached;
        first.position = QPoint(120, 140);
        first.size = QSize(520, 340);
        first.statisticsIntervalMs = 1500;
        config.presets[0].hasConfig = hasFirstPreset;
        config.presets[0].replicas = {first};
        auto second = cached;
        second.position = QPoint(240, 260);
        second.size = QSize(630, 410);
        second.statisticsIntervalMs = 2500;
        config.presets[1].hasConfig = true;
        config.presets[1].replicas = {second};
        config.defaultPresetSlot = defaultSlot;
        QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));

        NativeGateway gateway(functions());
        MainWindow manager(gateway, directory.path());
        manager.show();
        QCoreApplication::processEvents();
        QCOMPARE(manager.activeReplicaCount(), std::size_t{1});
        const auto expected = expectedSlot == 1 ? first : expectedSlot == 2 ? second : cached;
        const auto verify = [&] {
            auto* window = manager.replicaWindows().first();
            QCOMPARE(window->pos(), expected.position);
            QCOMPARE(window->size(), expected.size);
            QCOMPARE(window->statisticsIntervalMs(), expected.statisticsIntervalMs);
        };
        verify();
        if (expectedSlot > 0) {
            auto* load = manager.findChild<QPushButton*>(QStringLiteral("loadPreset%1").arg(expectedSlot));
            QVERIFY(load);
            QTest::mouseClick(load, Qt::LeftButton);
            QCoreApplication::processEvents();
            QCOMPARE(manager.activeReplicaCount(), std::size_t{1});
            verify();
        }
    }

    void defaultPresetLoadsOnStartup()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path();
        JsonConfigStore store(path);
        AppConfig config;
        config.presets[1].name = QStringLiteral("默认预设");
        config.presets[1].hasConfig = true;
        config.regionPresets.append({QStringLiteral("全局区域"), QRect(5, 6, 70, 80), 2500});
        config.colorRules.append({
            7, QStringLiteral("全局规则"), QStringLiteral("红色"), QStringLiteral(">=20"),
            TriggerFeedback::None, QString{}, PostTriggerAction::Loop});
        config.presets[1].replicas.append({
            QStringLiteral("EVE - Alpha"),
            QRect{},
            1500,
            {0, 0},
            false,
            1,
            true,
            false,
            QPoint(10, 10),
            QSize(360, 240),
            QColor(46, 204, 64),
            QColor(41, 121, 255),
        });
        config.defaultPresetSlot = 2;
        QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));

        NativeGateway gateway(functions());
        MainWindow manager(gateway, path);
        manager.show();
        QCoreApplication::processEvents();

        QCOMPARE(manager.activeReplicaCount(), std::size_t{1});
        QCOMPARE(manager.replicaWindows().first()->statisticsIntervalMs(), 1500);
        auto* regionTable = manager.findChild<QTableWidget*>(QStringLiteral("regionPresetTable"));
        auto* ruleTable = manager.findChild<QTableWidget*>(QStringLiteral("colorRuleTable"));
        QVERIFY(regionTable != nullptr);
        QVERIFY(ruleTable != nullptr);
        QCOMPARE(regionTable->rowCount(), 1);
        QCOMPARE(regionTable->item(0, 0)->text(), QStringLiteral("全局区域"));
        QCOMPARE(ruleTable->rowCount(), 1);
        QCOMPARE(ruleTable->item(0, 1)->text(), QStringLiteral("全局规则"));
        auto* radio = manager.findChild<QRadioButton*>(QStringLiteral("presetDefault2"));
        QVERIFY(radio != nullptr);
        QVERIFY(radio->isChecked());
    }

    void copyingReplicaScrollsToNewRow()
    {
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        manager.show();
        for (int index = 0; index < 10; ++index) {
            static_cast<void>(manager.createReplica());
        }
        QCoreApplication::processEvents();
        auto* scroll = manager.findChild<QScrollArea*>(QStringLiteral("replicaListScroll"));
        QVERIFY(scroll != nullptr);
        QVERIFY(scroll->verticalScrollBar()->maximum() > 0);

        auto* firstRow = manager.replicaRows().first();
        auto* copyButton = firstRow->findChild<QPushButton*>(QStringLiteral("copyReplica"));
        QVERIFY(copyButton != nullptr);
        QTest::mouseClick(copyButton, Qt::LeftButton);
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();

        ReplicaRow* newRow = nullptr;
        for (auto* row : manager.replicaRows()) {
            if (row->replicaId() == static_cast<ReplicaId>(manager.activeReplicaCount())) {
                newRow = row;
            }
        }
        QVERIFY(newRow != nullptr);
        QVERIFY(scroll->verticalScrollBar()->value() > 0);
        QVERIFY(newRow->geometry().bottom()
            <= scroll->verticalScrollBar()->value() + scroll->viewport()->height() + 1);
    }

    void replicaRowCollapseTogglesContent()
    {
        ReplicaRow row(1);
        auto* collapseButton = row.findChild<QPushButton*>(QStringLiteral("collapseReplica"));
        QVERIFY(collapseButton != nullptr);
        auto* sourceCombo = row.findChild<QComboBox*>(QStringLiteral("sourceCombo"));
        auto* closeButton = row.findChild<QPushButton*>(QStringLiteral("closeButton"));
        QVERIFY(sourceCombo != nullptr && closeButton != nullptr);
        QVERIFY(!row.collapsed());
        QVERIFY(sourceCombo->isVisibleTo(&row));
        QVERIFY(closeButton->isVisibleTo(&row));

        QTest::mouseClick(collapseButton, Qt::LeftButton);
        QVERIFY(row.collapsed());
        QCOMPARE(collapseButton->text(), QStringLiteral("展开"));
        QVERIFY(sourceCombo->isVisibleTo(&row));
        QVERIFY(!closeButton->isVisibleTo(&row));

        QTest::mouseClick(collapseButton, Qt::LeftButton);
        QVERIFY(!row.collapsed());
        QCOMPARE(collapseButton->text(), QStringLiteral("折叠"));
        QVERIFY(sourceCombo->isVisibleTo(&row));
    }

    void collapsedReplicaRestoresCollapsed()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path();
        JsonConfigStore store(path);
        AppConfig config;
        config.replicas.append({
            QStringLiteral("EVE - Alpha"),
            QRect{},
            1000,
            {0, 0},
            false,
            1,
            true,
            true,
            QPoint(10, 10),
            QSize(360, 240),
            QColor(46, 204, 64),
            QColor(41, 121, 255),
        });
        QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));

        NativeGateway gateway(functions());
        MainWindow manager(gateway, path);
        manager.show();
        QCoreApplication::processEvents();

        QCOMPARE(manager.activeReplicaCount(), std::size_t{1});
        QVERIFY(manager.replicaRows().first()->collapsed());
    }

    void keepInTrayOptionPersistsAndRestores()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path();
        JsonConfigStore store(path);
        AppConfig config;
        config.keepInTray = true;
        QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));

        NativeGateway gateway(functions());
        MainWindow manager(gateway, path);
        manager.show();
        auto* checkBox = manager.findChild<QCheckBox*>(QStringLiteral("keepInTray"));
        QVERIFY(checkBox != nullptr);
        QVERIFY(checkBox->isChecked());
        QVERIFY(manager.isVisible());
    }

    void clickRaiseOptionIsGlobalAndPersists()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path();
        JsonConfigStore store(path);
        AppConfig config;
        config.raiseReplicaOnClick = false;
        QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));

        NativeGateway gateway(functions());
        MainWindow manager(gateway, path);
        manager.show();
        auto* checkBox = manager.findChild<QCheckBox*>(
            QStringLiteral("raiseReplicaOnClick"));
        QVERIFY(checkBox != nullptr);
        QVERIFY(!checkBox->isChecked());

        auto* first = manager.createReplica();
        auto* second = manager.createReplica();
        QVERIFY(!first->raiseOnClickEnabled());
        QVERIFY(!second->raiseOnClickEnabled());

        checkBox->setChecked(true);
        QVERIFY(first->raiseOnClickEnabled());
        QVERIFY(second->raiseOnClickEnabled());

        AppConfig persisted;
        QVERIFY2(store.load(persisted, &error), qPrintable(error));
        QVERIFY(persisted.raiseReplicaOnClick);
    }

    void hiddenManagerCanBeRestoredAndActivated()
    {
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        manager.show();
        QCoreApplication::processEvents();
        manager.hide();
        QVERIFY(manager.isHidden());

        manager.showAndActivate();
        QCoreApplication::processEvents();
        QVERIFY(manager.isVisible());
        QVERIFY(!manager.isMinimized());
    }

    void gameRestoreOptionRebindsMissingPresetWindowAtSavedGeometry()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path();
        JsonConfigStore store(path);
        AppConfig config;
        config.restoreWindowsWithGame = true;
        config.defaultPresetSlot = 1;
        config.presets[0].hasConfig = true;
        config.presets[0].replicas.append({
            QStringLiteral("EVE Online - Alpha"),
            QRect(1200, 80, 640, 480),
            1500,
            {0, 0},
            false,
            1,
            true,
            false,
            QPoint(321, 234),
            QSize(700, 460),
            QColor(46, 204, 64),
            QColor(41, 121, 255),
        });
        QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));

        mockWindowCount = 0;
        NativeGateway gateway(functions());
        MainWindow manager(gateway, path);
        manager.show();
        QCoreApplication::processEvents();
        QCOMPARE(manager.activeReplicaCount(), std::size_t{0});

        QTest::qWait(650);
        AppConfig persisted;
        QVERIFY2(store.load(persisted, &error), qPrintable(error));
        QCOMPARE(
            persisted.presets[0].replicas.first().sourceTitle,
            QStringLiteral("EVE Online - Alpha"));

        setMockWindow(0, 777, L"EVE Online - Alpha");
        mockSourceWidth = 800;
        mockSourceHeight = 600;
        mockWindowCount = 1;
        QTRY_COMPARE_WITH_TIMEOUT(manager.activeReplicaCount(), std::size_t{1}, 6500);
        auto* replica = manager.replicaWindows().first();
        QVERIFY(replica->source().has_value());
        QCOMPARE(replica->source()->hwnd, std::uintptr_t{777});
        QVERIFY(!replica->capturedRegion().has_value());
        mockSourceWidth = 1920;
        mockSourceHeight = 1080;
        QTRY_COMPARE_WITH_TIMEOUT(
            replica->capturedRegion(),
            std::optional<QRect>(QRect(1200, 80, 640, 480)),
            2500);
        QCOMPARE(replica->pos(), QPoint(321, 234));
        QCOMPARE(replica->width(), 700);

        auto* restoreCheck = manager.findChild<QCheckBox*>(
            QStringLiteral("restoreWindowsWithGame"));
        auto* trayCheck = manager.findChild<QCheckBox*>(QStringLiteral("keepInTray"));
        QVERIFY(restoreCheck != nullptr && trayCheck != nullptr);
        QVERIFY(restoreCheck->isChecked());
        QVERIFY(restoreCheck->geometry().left() < trayCheck->geometry().left());

        mockWindowCount = 0;
        mockUpdateStatus = -9;
        QTRY_COMPARE_WITH_TIMEOUT(manager.activeReplicaCount(), std::size_t{0}, 3000);

        mockUpdateStatus = 0;
        setMockWindow(0, 888, L"EVE Online - Alpha");
        mockWindowCount = 1;
        QTRY_COMPARE_WITH_TIMEOUT(manager.activeReplicaCount(), std::size_t{1}, 6500);
        auto* restored = manager.replicaWindows().first();
        QVERIFY(restored->source().has_value());
        QCOMPARE(restored->source()->hwnd, std::uintptr_t{888});
        QCOMPARE(
            restored->capturedRegion(),
            std::optional<QRect>(QRect(1200, 80, 640, 480)));
        QCOMPARE(restored->pos(), QPoint(321, 234));

        mockWindowCount = 0;
        mockUpdateStatus = 0;
        mockSourceWidth = 1920;
        mockSourceHeight = 1080;
    }

    void closingHiddenManagerStillRunsCloseLogic()
    {
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        manager.show();
        static_cast<void>(manager.createReplica());
        QCoreApplication::processEvents();

        manager.hide();
        QCoreApplication::processEvents();
        QVERIFY(!manager.isVisible());

        manager.close();
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();

        QCOMPARE(manager.activeReplicaCount(), std::size_t{0});
    }
};

QTEST_MAIN(MainWindowTest)

#include "main_window_test.moc"
