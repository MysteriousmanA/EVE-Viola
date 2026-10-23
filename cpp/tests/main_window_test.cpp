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
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>

#include "config/app_config.h"
#include "ui/audio_feedback_manager.h"
#include "features/replica/statistics_coordinator.h"
#include "ui/main_window.h"
#include "ui/desktop_pet_page.h"
#include "ui/desktop_pet/floating_pet_window.h"
#include "ui/page_partition_page.h"
#include "ui/replica_row.h"
#include "ui/replica_window.h"
#include "ui/pages/advanced_logic/advanced_logic_page.h"
#include "ui/pixel_color_editor.h"
#include "ui/pages/color_rule/color_rule_dialog.h"

namespace {

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
    void statisticsColorDialogEditsPersistAndRestoreWithTheReplica()
    {
        const QString path = freshConfigPath();
        JsonConfigStore store(path);
        AppConfig initial;
        ReplicaConfig replica;
        replica.statisticsIntervalMs = 0;
        replica.statisticsColors = {{QStringLiteral("#123456"), 12}};
        initial.replicas.append(replica);
        QString error;
        QVERIFY2(store.save(initial, &error), qPrintable(error));
        NativeGateway gateway(functions());
        const QList<PixelColorTarget> expected{{QStringLiteral("#ffaa00"), 7}};
        {
            MainWindow manager(gateway, path);
            auto* row = manager.replicaRows().first();
            QVERIFY(row->statisticsColors() == replica.statisticsColors);
            QTimer::singleShot(0, [] {
                auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                QVERIFY(dialog);
                auto* table = dialog->findChild<QTableWidget*>(QStringLiteral("statisticsColorsTable"));
                QVERIFY(table);
                QCOMPARE(table->rowCount(), 1);
                auto* editor = static_cast<PixelColorEditor*>(table->cellWidget(0, 0));
                editor->combo()->setCurrentText(QStringLiteral("#ffaa00"));
                editor->tolerance()->setValue(7);
                dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
            });
            row->findChild<QPushButton*>(QStringLiteral("statisticsColors"))->click();
            QVERIFY(row->statisticsColors() == expected);
            QVERIFY(manager.replicaWindows().first()->statisticsColors() == expected);
            manager.close();
        }
        AppConfig saved;
        QVERIFY2(store.load(saved, &error), qPrintable(error));
        QVERIFY(saved.replicas.first().statisticsColors == expected);
        MainWindow restored(gateway, path);
        QVERIFY(restored.replicaRows().first()->statisticsColors() == expected);
        QVERIFY(restored.replicaWindows().first()->statisticsColors() == expected);
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
            color->setCurrentText(QStringLiteral("#00ff00"));
            tolerance->setValue(20);
            auto* buttons = dialog->findChild<QDialogButtonBox*>();
            buttons->button(QDialogButtonBox::Save)->click();
        });
        const auto edited = promptColorRule(nullptr, initial);
        QVERIFY(edited.has_value());
        QCOMPARE(edited->triggerCondition1.pixel, QStringLiteral("#00ff00"));
        QCOMPARE(edited->triggerCondition1.tolerance, 20);
        QCOMPARE(edited->triggerCondition1.expression, QStringLiteral(">=2"));
    }

    void replicaRowShowsThreePresetsAndCanDisplayCustomColorCounts()
    {
        ReplicaRow row(1);
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
        original->setSourceNameVisible(true);
        original->moveTo(QPoint(70, 60));
        auto* copy = manager.duplicateReplica(original->replicaId());
        QVERIFY(copy != nullptr);
        QVERIFY(copy->pos() != original->pos());
        QCOMPARE(copy->colorLogicSlotColor(1), original->colorLogicSlotColor(1));
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
        QCOMPARE(copied.slotColor1, QColor(QStringLiteral("#123456")));
        QCOMPARE(copied.colorLogicSlotRule[0], 1);
        QVERIFY(copied.colorLogicEnabled);
        QVERIFY(copied.sourceNameVisible);
        QVERIFY(!copied.visible);
    }
    void autosavingGeometryDoesNotCancelAnActivationRequest()
    {
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
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

    void replicaRowOffersSourceNameOverlaySwitchDisabledByDefault()
    {
        ReplicaRow row(1);
        auto* sourceNameVisibility =
            row.findChild<QCheckBox*>(QStringLiteral("sourceNameVisibility"));
        QVERIFY(sourceNameVisibility != nullptr);
        QVERIFY(!sourceNameVisibility->isChecked());

        QSignalSpy visibilityChanged(&row, &ReplicaRow::sourceNameVisibilityChanged);
        QVERIFY(visibilityChanged.isValid());
        sourceNameVisibility->setChecked(true);

        QCOMPARE(visibilityChanged.count(), 1);
        QCOMPARE(visibilityChanged.first().first().toBool(), true);
    }

    void replicaRowOffersZeroToTenSecondStatisticsIntervals()
    {
        ReplicaRow row(1);
        auto* interval = row.findChild<QComboBox*>(QStringLiteral("statisticsInterval"));
        QVERIFY(interval != nullptr);
        QCOMPARE(interval->count(), 21);
        QCOMPARE(interval->itemText(0), QStringLiteral("不统计"));
        QCOMPARE(interval->itemData(0).toInt(), 0);
        QCOMPARE(interval->itemText(1), QStringLiteral("0.5 s"));
        QCOMPARE(interval->itemData(1).toInt(), 500);
        QCOMPARE(interval->itemText(20), QStringLiteral("10.0 s"));
        QCOMPARE(interval->itemData(20).toInt(), 10000);
        QCOMPARE(interval->currentData().toInt(), 1000);

        QSignalSpy intervalChanged(&row, &ReplicaRow::statisticsIntervalChanged);
        interval->setCurrentIndex(7);

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

    void hidingReplicaLocksSlotTwoAndResetsEnableStateToOne()
    {
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        static_cast<void>(manager.createReplica());
        ReplicaRow& row = *manager.replicaRows().first();
        auto* visibility = row.findChild<QCheckBox*>(QStringLiteral("replicaVisibility"));
        auto* enabled = row.findChild<QCheckBox*>(QStringLiteral("colorLogicEnabled"));
        auto* slot = row.findChild<QComboBox*>(QStringLiteral("colorLogicActiveSlot"));
        auto* slotA = row.findChild<QComboBox*>(QStringLiteral("colorLogicSlotA"));
        auto* slotB = row.findChild<QComboBox*>(QStringLiteral("colorLogicSlotB"));
        QVERIFY(slotA->isEnabled());
        QVERIFY(slotB->isEnabled());
        QList<ColorRule> rules;
        rules.append({1, QStringLiteral("红"), {}, {}, TriggerFeedback::None, {},
                      PostTriggerAction::Loop});
        rules.append({2, QStringLiteral("蓝"), {}, {}, TriggerFeedback::None, {},
                      PostTriggerAction::Loop});
        row.setColorRules(rules);
        slotB->setCurrentIndex(1);
        slot->setCurrentIndex(1);

        QSignalSpy enabledChanged(&row, &ReplicaRow::colorLogicEnabledChanged);
        QSignalSpy slotChanged(&row, &ReplicaRow::activeColorLogicSlotChanged);
        visibility->setChecked(false);
        QVERIFY(slotA->isEnabled());
        QVERIFY(!slotB->isEnabled());
        QCOMPARE(slotB->currentData().toInt(), 0);
        QCOMPARE(slot->currentData().toInt(), 1);
        QCOMPARE(slot->itemData(1, Qt::UserRole - 1).toBool(), false);
        QVERIFY(enabled->isChecked());
        QCOMPARE(enabledChanged.last().first().toBool(), true);
        QCOMPARE(slotChanged.last().first().toInt(), 1);

        visibility->setChecked(true);
        QVERIFY(slotB->isEnabled());
        QCOMPARE(slot->itemData(1, Qt::UserRole - 1).toBool(), true);
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

    void sourceNameSwitchHidesThumbnailAndShowsExtractedOrangeNameAtReplicaCenter()
    {
        NativeGateway gateway(functions());
        MainWindow manager(gateway, freshConfigPath());
        manager.show();
        auto* window = manager.createReplica();
        auto* row = manager.replicaRows().first();
        lastThumbnailVisible = false;
        window->selectSource(WindowInfo{123, QStringLiteral("EVE - MysteriousmanA")});

        auto* sourceNameVisibility =
            row->findChild<QCheckBox*>(QStringLiteral("sourceNameVisibility"));
        auto* overlay = window->findChild<QLabel*>(QStringLiteral("sourceNameOverlay"));
        QVERIFY(sourceNameVisibility != nullptr);
        QVERIFY(overlay != nullptr);
        QCOMPARE(overlay->parentWidget(), window);
        QVERIFY(!overlay->isVisible());
        QVERIFY(lastThumbnailVisible);

        sourceNameVisibility->setChecked(true);
        QCoreApplication::processEvents();

        QVERIFY(window->source().has_value());
        QVERIFY(window->isVisible());
        QVERIFY(!overlay->isHidden());
        QVERIFY(overlay->isVisible());
        QVERIFY(!lastThumbnailVisible);
        QCOMPARE(overlay->text(), QStringLiteral("MysteriousmanA"));
        QCOMPARE(overlay->palette().color(QPalette::WindowText), QColor(255, 140, 0));
        const QPoint offset = overlay->geometry().center() - window->rect().center();
        QVERIFY(qAbs(offset.x()) <= 1);
        QVERIFY(qAbs(offset.y()) <= 1);

        sourceNameVisibility->setChecked(false);
        QCoreApplication::processEvents();
        QVERIFY(!overlay->isVisible());
        QVERIFY(lastThumbnailVisible);

        window->selectSource(WindowInfo{
            456,
            QStringLiteral("星战前夜：晨曦[Infinity] - 噩梦级"),
        });
        sourceNameVisibility->setChecked(true);
        QCoreApplication::processEvents();
        QCOMPARE(overlay->text(), QStringLiteral("噩梦级"));
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

    void advancedLogicPagePersistsWhitelistAndPriorityChanges()
    {
        QTemporaryDir directory;
        JsonConfigStore store(directory.path());
        AppConfig config;
        ColorRule warning;
        warning.number = 1;
        ColorRule acceleration;
        acceleration.number = 2;
        config.colorRules = {warning, acceleration};
        WhitelistSequence sequence;
        sequence.id = QStringLiteral("evade");
        sequence.name = QStringLiteral("规避");
        sequence.nodes = {{1, WhitelistNodeEffect::Keep, {}}, {2, WhitelistNodeEffect::Suppress, {}}};
        config.advancedLogic = {true, {sequence}};
        QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));
        NativeGateway gateway(functions());
        MainWindow manager(gateway, directory.path());
        auto* page = manager.findChild<AdvancedLogicPage*>();
        QVERIFY(page);
        QCOMPARE(page->configuration().sequences.size(), 1);
        auto* enabled = page->findChild<QCheckBox*>(QStringLiteral("advancedLogicEnabled"));
        QVERIFY(enabled->isChecked());
        enabled->setChecked(false);
        auto* priorities = page->findChild<QTableWidget*>(QStringLiteral("colorRulePriorityTable"));
        QVERIFY(priorities);
        qobject_cast<QComboBox*>(priorities->cellWidget(0, 1))->setCurrentIndex(2);
        AppConfig saved;
        QVERIFY2(store.load(saved, &error), qPrintable(error));
        QVERIFY(!saved.advancedLogic.enabled);
        QCOMPARE(saved.advancedLogic.sequences, config.advancedLogic.sequences);
        QCOMPARE(saved.colorRules.first().priority, ColorRulePriority::Immediate);
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
        QCOMPARE(tabs->tabText(4), QStringLiteral("高级逻辑"));
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
        QCOMPARE(interval->currentData().toInt(), 1000);
        auto* rowDelete = qobject_cast<QPushButton*>(presets->cellWidget(0, 6));
        QVERIFY(rowDelete != nullptr);
        QCOMPARE(rowDelete->text(), QStringLiteral("删除"));

        auto* rules = manager.findChild<QTableWidget*>(QStringLiteral("colorRuleTable"));
        QVERIFY(rules != nullptr);
        QCOMPARE(rules->rowCount(), 1);
        QCOMPARE(rules->columnCount(), 6);
        const QStringList headers{
            QStringLiteral("编号"), QStringLiteral("备注"), QStringLiteral("触发条件 1"),
            QStringLiteral("触发条件 2"), QStringLiteral("触发反馈"), QStringLiteral("触发后操作")};
        for (int column = 0; column < headers.size(); ++column) {
            QCOMPARE(rules->horizontalHeaderItem(column)->text(), headers[column]);
        }
        QCOMPARE(rules->item(0, 0)->text(), QStringLiteral("4"));
        QCOMPARE(rules->item(0, 1)->text(), QStringLiteral("护盾"));
        QCOMPARE(rules->item(0, 2)->text(), QStringLiteral("红色 >=20"));
        QCOMPARE(rules->item(0, 3)->text(), QStringLiteral("白色 <=5"));
        QCOMPARE(rules->item(0, 4)->text(), QStringLiteral("弹窗"));
        QCOMPARE(rules->item(0, 5)->text(), QStringLiteral("正向循环"));

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

        auto* sourceName = row.findChild<QCheckBox*>(QStringLiteral("sourceNameVisibility"));
        QVERIFY(sourceName != nullptr);
        sourceName->setChecked(true);
        QVERIFY(row.sourceNameVisible());
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
        auto* sourceName = row->findChild<QCheckBox*>(QStringLiteral("sourceNameVisibility"));
        auto* enabled = row->findChild<QCheckBox*>(QStringLiteral("colorLogicEnabled"));
        QVERIFY(interval != nullptr && slotA != nullptr && sourceName != nullptr && enabled != nullptr);

        original->selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});
        interval->setCurrentIndex(interval->findData(1500));
        slotA->setCurrentIndex(slotA->findData(1));
        sourceName->setChecked(true);
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
        QVERIFY(copy->colorLogicEnabled());

        ReplicaRow* copyRow = nullptr;
        for (auto* r : manager.replicaRows()) {
            if (r->replicaId() == copy->replicaId()) {
                copyRow = r;
            }
        }
        QVERIFY(copyRow != nullptr);
        QCOMPARE(copyRow->colorLogicSlotRule(1), 1);
        QVERIFY(copyRow->sourceNameVisible());
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
        interval->setCurrentIndex(interval->findData(500));
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
        auto* sourceName = row.findChild<QCheckBox*>(QStringLiteral("sourceNameVisibility"));
        QVERIFY(sourceName != nullptr);
        QCOMPARE(sourceName->text(), QStringLiteral("显示角色名(不显示画面)"));
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

        QCOMPARE(manager.activeReplicaCount(), std::size_t{2});
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
        QVERIFY(first->colorLogicEnabled());
        QCOMPARE(first->activeColorLogicSlot(), 2);
        QVERIFY(firstRow->sourceNameVisible());
        QVERIFY(firstRow->visibilityChecked());
        QCOMPARE(first->pos(), QPoint(300, 200));
        QCOMPARE(first->size(), QSize(720, 480));
        QCOMPARE(firstRow->colorLogicSlotRule(1), 1);
        QCOMPARE(firstRow->colorLogicSlotRule(2), 2);
        QVERIFY(firstRow->colorLogicSlotColor(1) == QColor(255, 0, 0));
        QVERIFY(firstRow->colorLogicSlotColor(2) == QColor(0, 0, 255));

        QCOMPARE(second->statisticsIntervalMs(), 500);
        QVERIFY(!secondRow->visibilityChecked());
        QCOMPARE(second->pos(), QPoint(100, 100));
        QCOMPARE(second->size(), QSize(360, 240));
        QCOMPARE(secondRow->colorLogicSlotRule(1), 2);
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
            row->findChild<QComboBox*>(QStringLiteral("colorLogicSlotA")),
            row->findChild<QComboBox*>(QStringLiteral("colorLogicSlotB")),
            row->findChild<QComboBox*>(QStringLiteral("colorLogicActiveSlot")),
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
        interval->setCurrentIndex(interval->findData(500));
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
        interval->setCurrentIndex(interval->findData(500));

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
        interval->setCurrentIndex(interval->findData(500));

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
            false,
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
            false,
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
            false,
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
