#include "features/expedition_timing/expedition_timing_service.h"
ExpeditionDuration ExpeditionTimingService::defaultDuration(const QString &type)
{
    return {type == QStringLiteral("三无畏") || type == QStringLiteral("护盾母星") ? 2 : 0, 23, 59};
}
QString ExpeditionTimingService::expiryText(const QDateTime &start, ExpeditionDuration duration,
                                            QString faction, QString type)
{
    const auto expiry = start.addDays(duration.days)
                            .addSecs(static_cast<qint64>(duration.hours) * 3600 +
                                     static_cast<qint64>(duration.minutes) * 60);
    if (faction.isEmpty()) {
        faction = QStringLiteral("兔子");
    }
    if (type.isEmpty()) {
        type = QStringLiteral("1-10");
    }
    return QStringLiteral("%1%2 %3月%4日%5时%6分过期")
        .arg(faction, type)
        .arg(expiry.date().month())
        .arg(expiry.date().day())
        .arg(expiry.time().hour())
        .arg(expiry.time().minute());
}
