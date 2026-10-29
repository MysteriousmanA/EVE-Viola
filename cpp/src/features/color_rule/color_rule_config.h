#pragma once

#include "config/config_types.h"

[[nodiscard]] QString triggerConditionRelationName(TriggerConditionRelation relation);
[[nodiscard]] QString triggerFeedbackName(TriggerFeedback feedback);
[[nodiscard]] std::optional<TriggerFeedback> triggerFeedbackFromName(const QString& name);
[[nodiscard]] QString postTriggerActionName(PostTriggerAction action);
[[nodiscard]] std::optional<PostTriggerAction> postTriggerActionFromName(const QString& name);
[[nodiscard]] int addColorRule(AppConfig& config, ColorRule rule);
[[nodiscard]] bool updateColorRule(AppConfig& config, int number, ColorRule rule);
[[nodiscard]] bool removeColorRule(AppConfig& config, int number);
