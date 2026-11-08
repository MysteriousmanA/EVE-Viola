#pragma once
#include <optional>
#include <QHash>
#include "config/config_types.h"
class QWidget;
std::optional<ColorRule> promptColorRule(QWidget *parent, const ColorRule &initial,
    const QList<ReplicaConfig>& replicas = {}, const QList<ColorRule>& rules = {},
    const QHash<QString, QString>& regionPresetNames = {});
