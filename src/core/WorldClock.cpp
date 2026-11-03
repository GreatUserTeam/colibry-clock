#include "WorldClock.h"

#include <QCoreApplication>
#include <QTimeZone>

namespace colibry::clock {

QString offsetText(int seconds)
{
    const QChar sign = seconds < 0 ? QLatin1Char('-') : QLatin1Char('+');
    const int a = qAbs(seconds);
    return QStringLiteral("UTC%1%2:%3").arg(sign).arg(a / 3600, 2, 10, QLatin1Char('0')).arg((a / 60) % 60, 2, 10, QLatin1Char('0'));
}

QString cityName(const QString &zoneId)
{
    QString name = zoneId.mid(zoneId.lastIndexOf(QLatin1Char('/')) + 1);
    name.replace(QLatin1Char('_'), QLatin1Char(' '));
    return name;
}

ZoneInfo describeZone(const QString &id, const QDateTime &nowUtc, bool clock24, int localOffsetSeconds)
{
    ZoneInfo info;
    const QTimeZone zone(id.toUtf8());
    if (!zone.isValid() || !nowUtc.isValid())
        return info;

    const QDateTime there = nowUtc.toTimeZone(zone);
    info.valid = true;
    info.id = id;
    info.city = cityName(id);
    info.time = there.time().toString(clock24 ? QStringLiteral("HH:mm") : QStringLiteral("h:mm AP"));
    info.offset = offsetText(there.offsetFromUtc());

    const QDate localDate = nowUtc.addSecs(localOffsetSeconds).date();
    info.dayShift = static_cast<int>(localDate.daysTo(there.date()));
    if (info.dayShift > 0)
        info.dayText = QCoreApplication::translate("colibry::clock", "Tomorrow");
    else if (info.dayShift < 0)
        info.dayText = QCoreApplication::translate("colibry::clock", "Yesterday");
    else
        info.dayText = QCoreApplication::translate("colibry::clock", "Today");
    return info;
}

} // namespace colibry::clock
