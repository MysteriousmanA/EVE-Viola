#pragma once

#include <array>
#include <optional>
#include <utility>

#include <QColor>
#include <QHash>
#include <QList>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QUuid>

#include "features/replica/pixel_statistics_calculator.h"

inline constexpr int kStatisticsIntervalDisabledMs = 0;
inline constexpr int kStatisticsIntervalMinimumMs = 1500;
inline constexpr int kStatisticsIntervalMaximumMs = 10000;
inline constexpr int kStatisticsIntervalStepMs = 500;
inline constexpr int kDefaultStatisticsIntervalMs = 1500;

inline constexpr int kAutomaticActivationCooldownMinimumMs = 1000;
inline constexpr int kAutomaticActivationCooldownMaximumMs = 10000;
inline constexpr int kAutomaticActivationCooldownStepMs = 500;
inline constexpr int kDefaultAutomaticActivationCooldownMs = 5000;

inline constexpr int kAutomaticActivationMouseQuietPeriodMinimumMs = 100;
inline constexpr int kAutomaticActivationMouseQuietPeriodMaximumMs = 10000;
inline constexpr int kAutomaticActivationMouseQuietPeriodStepMs = 100;
inline constexpr int kDefaultAutomaticActivationMouseQuietPeriodMs = 1500;

inline const QString kDefaultPreviousClientKey{QStringLiteral("PageUp")};
inline const QString kDefaultNextClientKey{QStringLiteral("PageDown")};
inline const QString kDefaultRadialSwitchKey{QStringLiteral("KeyboardVK:20:0")};

struct QuickSwitchKeyboardInput {
    int virtualKey{};
    bool extended{};
};

struct RadialAccountConfig {
    QString windowTitle;
    QString nickname;
    int position{1};

    bool operator==(const RadialAccountConfig&) const = default;
};

inline constexpr int kColorRulePixelValueMinimum = 0;
inline constexpr int kColorRulePixelValueMaximum = 5000;

inline constexpr int kPresetCount = 3;

struct RegionPreset {
    QString name;
    QRect region;
    int statisticsIntervalMs{kDefaultStatisticsIntervalMs};

    // Empty inherits the application's common color matching settings.
    QList<PixelColorTarget> colorTolerances;

    bool operator==(const RegionPreset&) const = default;
};

inline constexpr int kPagePartitionLineWidthMinimum = 1;
inline constexpr int kPagePartitionLineWidthMaximum = 20;

struct PagePartitionLine {
    QString id;
    QPoint start;
    QPoint end;
    QColor color{QColor(QStringLiteral("#0088ff"))};
    int width{2};
    QString note;

    [[nodiscard]] bool isHorizontal() const noexcept { return start.y() == end.y(); }

    bool operator==(const PagePartitionLine&) const = default;
};

struct PagePartitionConfig {
    QList<PagePartitionLine> lines;
    QColor defaultColor{QColor(QStringLiteral("#0088ff"))};
    QColor selectedColor{QColor(QStringLiteral("#ff0000"))};
    int defaultWidth{2};
    bool visible{true};
    bool onlyShowWhenEveDetected{false};

    bool operator==(const PagePartitionConfig&) const = default;
};

enum class TriggerFeedback {
    None,
    Popup,
    Audio,
};

enum class PostTriggerAction {
    Loop,
    Jump,
    ReverseWait,
};

enum class TriggerConditionRelation { And, Or };

struct TriggerCondition {
    QString lowerOperator;
    double lowerValue{0.0};
    QString upperOperator;
    double upperValue{0.0};
    TriggerConditionRelation relation{TriggerConditionRelation::And};

    [[nodiscard]] bool isEmpty() const noexcept;
    [[nodiscard]] QString toString() const;

    bool operator==(const TriggerCondition&) const = default;
};

struct ColorRuleCondition {
    QString pixel;
    QString expression;
    int tolerance{-1};
    QString replicaId; // Definition: region-preset:<name>; runtime: per-instance sampling window ID.

    [[nodiscard]] bool isEmpty() const noexcept
    {
        return pixel.trimmed().isEmpty() || expression.trimmed().isEmpty();
    }

    [[nodiscard]] QString displayText() const
    {
        return isEmpty() ? QString{}
                         : QStringLiteral("%1%2 %3").arg(
                               pixel.trimmed(), tolerance < 0 ? QString{} :
                                   QStringLiteral("（容差 %1）").arg(tolerance), expression.trimmed());
    }

    bool operator==(const ColorRuleCondition&) const = default;
};

struct ColorRuleWindowSwitch {
    QString replicaId; // Region preset in a definition; binding-specific window at runtime.
    QColor borderColor; // Invalid means the rule does not change this window's border.
    int middleClickRule{}; // Zero disables middle-click switching.

    bool operator==(const ColorRuleWindowSwitch&) const = default;
};

struct ColorRule {
    int number{};
    QString note;
    ColorRuleCondition triggerCondition1;
    ColorRuleCondition triggerCondition2;
    TriggerConditionRelation triggerConditionRelation{TriggerConditionRelation::And};
    TriggerFeedback triggerFeedback{TriggerFeedback::None};
    QString triggerFeedbackAudio;
    PostTriggerAction postTriggerAction{PostTriggerAction::Loop};
    QList<ColorRuleCondition> windowConditions;
    QList<ColorRuleWindowSwitch> windowSwitches;
    QString targetReplicaId;
    bool enabled{true};
    int jumpToRule{};
    int definitionNumber{}; // Runtime instance -> reusable rule number; never serialized.

    ColorRule() = default;

    ColorRule(int ruleNumber, QString ruleNote, QString legacyTriggerPixel,
              QString legacyTriggerCondition, TriggerFeedback feedback, QString feedbackAudio,
              PostTriggerAction action)
        : number(ruleNumber), note(std::move(ruleNote)),
          triggerCondition1{std::move(legacyTriggerPixel), std::move(legacyTriggerCondition)},
          triggerFeedback(feedback), triggerFeedbackAudio(std::move(feedbackAudio)),
          postTriggerAction(action)
    {
    }

    bool operator==(const ColorRule&) const = default;
};

struct ReplicaConfig {
    QString sourceTitle;
    QRect capturedRegion;
    int statisticsIntervalMs{kDefaultStatisticsIntervalMs};
    int colorLogicSlotRule[2]{0, 0};
    bool colorLogicEnabled{false};
    int activeColorLogicSlot{1};
    bool visible{true};
    bool collapsed{false};
    QPoint position;
    QSize size{QSize(360, 240)};
    QColor slotColor1{QColor(46, 204, 64)};
    QColor slotColor2{QColor(41, 121, 255)};
    QList<PixelColorTarget> statisticsColors{defaultPixelColorTargets()};
    QString id{QUuid::createUuid().toString(QUuid::WithoutBraces)};

    bool operator==(const ReplicaConfig& other) const
    {
        return id == other.id && sourceTitle == other.sourceTitle && capturedRegion == other.capturedRegion &&
               statisticsIntervalMs == other.statisticsIntervalMs &&
               colorLogicSlotRule[0] == other.colorLogicSlotRule[0] &&
               colorLogicSlotRule[1] == other.colorLogicSlotRule[1] &&
               colorLogicEnabled == other.colorLogicEnabled &&
               activeColorLogicSlot == other.activeColorLogicSlot &&
               visible == other.visible &&
               collapsed == other.collapsed && position == other.position && size == other.size &&
               slotColor1 == other.slotColor1 && slotColor2 == other.slotColor2 &&
               statisticsColors == other.statisticsColors;
    }
};

struct PresetConfig {
    QString name;
    bool hasConfig{false};
    QList<ReplicaConfig> replicas;

    bool operator==(const PresetConfig& other) const
    {
        return name == other.name && hasConfig == other.hasConfig && replicas == other.replicas;
    }
};

struct LogicBindingConfig {
    QString id{QUuid::createUuid().toString(QUuid::WithoutBraces)};
    int ruleNumber{};
    QString sourceTitle;
    QString sourceProcessName{QStringLiteral("exefile.exe")};
    bool enabled{true};
    QHash<QString, QString> windowIds; // Region template ID -> existing small-window ID.
    bool operator==(const LogicBindingConfig&) const = default;
};

struct AppConfig {
    QList<PixelColorTarget> colorTolerances{defaultPixelColorTargets()};
    QList<RegionPreset> regionPresets;
    QList<ColorRule> colorRules;
    QList<LogicBindingConfig> logicBindings;
    PagePartitionConfig pagePartition;
    QList<ReplicaConfig> replicas;
    int nextColorRuleNumber{1};
    std::array<PresetConfig, kPresetCount> presets;
    int defaultPresetSlot{0};
    bool restoreWindowsWithGame{false};
    bool keepInTray{false};
    bool raiseReplicaOnClick{true};
    bool lockReplicaSize{false};
    bool activateSourceOnColorLogicTrigger{true};
    int automaticActivationCooldownMs{kDefaultAutomaticActivationCooldownMs};
    int automaticActivationMouseQuietPeriodMs{kDefaultAutomaticActivationMouseQuietPeriodMs};
    QStringList previousClientKeys{QStringLiteral("PageUp")};
    QStringList nextClientKeys{QStringLiteral("PageDown")};
    QString radialSwitchKey{kDefaultRadialSwitchKey};
    int radialSwitchRecentSeconds{10};
    QList<RadialAccountConfig> radialAccounts;
};

inline QList<PixelColorTarget> regionColorTolerances(const AppConfig& config, const QString& name)
{
    for (const auto& preset : config.regionPresets) {
        if (preset.name == name && !preset.colorTolerances.isEmpty()) {
            return preset.colorTolerances;
        }
    }
    return config.colorTolerances;
}

inline int colorToleranceFor(const QList<PixelColorTarget>& settings, const QString& color)
{
    for (const auto& target : settings) {
        if (target.color == color) { return target.tolerance; }
    }
    return normalizedPixelColorTargets({{color, -1}}).first().tolerance;
}
