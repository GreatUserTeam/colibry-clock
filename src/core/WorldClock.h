#pragma once

#include <QDateTime>
#include <QString>

namespace colibry::clock {

struct ZoneInfo
{
    bool valid = false;
    QString id;
    QString city;      ///< "Europe/Kyiv" -> "Kyiv"
    QString time;      ///< "14:05" or "2:05 PM"
    QString offset;    ///< "UTC+02:00"
    int dayShift = 0;  ///< -1 yesterday, 0 same day, +1 tomorrow, compared with the local day
    QString dayText;   ///< "Today", "Tomorrow", "Yesterday"
};

/// Describes what a clock in the given zone shows at `nowUtc`. `localOffsetSeconds` is the
/// offset of the person's own zone, used for the day comparison.
ZoneInfo describeZone(const QString &id, const QDateTime &nowUtc, bool clock24, int localOffsetSeconds);

QString cityName(const QString &zoneId);
QString offsetText(int seconds);

} // namespace colibry::clock
