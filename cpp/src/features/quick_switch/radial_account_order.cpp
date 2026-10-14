#include "features/quick_switch/radial_account_order.h"
#include <algorithm>

QList<QString> orderedRadialAccountTitles(const QStringList& liveWindowTitles,
                                          const QList<RadialAccountConfig>& accountConfigs)
{
    QList<RadialAccountConfig> sortedConfigs = accountConfigs;
    std::stable_sort(
        sortedConfigs.begin(), sortedConfigs.end(),
        [](const auto& left, const auto& right) { return left.position < right.position; });

    QStringList uniqueLiveTitles;
    for (const QString& title : liveWindowTitles) {
        if (!title.trimmed().isEmpty() && !uniqueLiveTitles.contains(title)) {
            uniqueLiveTitles.append(title);
        }
    }

    QList<QString> ordered;
    for (const auto& account : sortedConfigs) {
        if (uniqueLiveTitles.contains(account.windowTitle) &&
            !ordered.contains(account.windowTitle)) {
            ordered.append(account.windowTitle);
        }
    }
    for (const QString& title : uniqueLiveTitles) {
        if (!ordered.contains(title)) {
            ordered.append(title);
        }
    }
    return ordered;
}
