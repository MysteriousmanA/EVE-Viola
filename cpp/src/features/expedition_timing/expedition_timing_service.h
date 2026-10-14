#pragma once
#include <QDateTime>
#include <QString>
struct ExpeditionDuration {
    int days;
    int hours;
    int minutes;
};
class ExpeditionTimingService
{
  public:
    static ExpeditionDuration defaultDuration(const QString &type);
    static QString expiryText(const QDateTime &start, ExpeditionDuration duration, QString faction,
                              QString type);
};
