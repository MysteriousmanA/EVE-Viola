#pragma once
#include <QHash>
#include "config/config_types.h"

inline QString colorLogicReplicaLabel(const ReplicaConfig& replica, int index,
    const QHash<QString, QString>& presetNames)
{
    QString region = presetNames.value(replica.id);
    if (replica.id.startsWith(QStringLiteral("region-preset:")) && !region.isEmpty()) { return region; }
    if (region.isEmpty()) {
        const auto& r = replica.capturedRegion;
        region = r.isValid() && !r.isEmpty()
            ? QStringLiteral("自定义 (%1,%2) %3×%4").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height())
            : QStringLiteral("完整窗口");
    }
    if (replica.sourceTitle.isEmpty()) {
        return QStringLiteral("无主小窗 %1 · %2").arg(index + 1).arg(region);
    }
    return QStringLiteral("小窗 %1 · %2 · %3").arg(index + 1)
        .arg(replica.sourceTitle.isEmpty() ? QStringLiteral("未选择来源") : replica.sourceTitle, region);
}
