#pragma once

#include "config/app_config.h"
#include "platform/native_gateway.h"
#include "features/replica/replica_controller.h"

class PresetService final {
public:
    struct RestorePlan {
        QList<ReplicaConfig> active;
        QList<ReplicaConfig> dormant;
    };
    [[nodiscard]] static const WindowInfo* findSource(const QList<WindowInfo>& windows, const QString& title);
    [[nodiscard]] static RestorePlan planRestore(const QList<ReplicaConfig>& replicas,
        const QList<WindowInfo>& windows, bool restoreWithGame);
    [[nodiscard]] static QList<ReplicaConfig> takeRestorable(ReplicaController& controller,
        const QList<WindowInfo>& windows);
    static bool save(AppConfig& config, int slot, const QList<ReplicaConfig>& replicas);
    static bool load(AppConfig& config, int slot);
    static bool remove(AppConfig& config, int slot);
};
