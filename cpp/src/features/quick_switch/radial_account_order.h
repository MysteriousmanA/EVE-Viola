#pragma once

#include "config/config_types.h"

[[nodiscard]] QList<QString>
orderedRadialAccountTitles(const QStringList& liveWindowTitles,
                           const QList<RadialAccountConfig>& accountConfigs);
