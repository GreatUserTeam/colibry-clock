#include "Alarms.h"

#include <QCoreApplication>
#include <QLocale>
#include <QRandomGenerator>

#include <algorithm>

namespace colibry::clock {

namespace {
int bitFor(const QDate &d)
{
    return 1 << (d.dayOfWeek() - 1); // Qt: Monday = 1
}

QString newId()
{
    return QStringLiteral("%1%2")
        .arg(QDateTime::currentMSecsSinceEpoch(), 0, 36)
        .arg(QRandomGenerator::global()->bounded(0x100000), 5, 36, QLatin1Char('0'));
}
} // namespace

bool Alarm::isValid() const
{
    return time.isValid() && days >= 0 && days <= Every;
}

QDateTime Alarm::nextTrigger(const QDateTime &from) const
{
    if (!isValid() || !from.isValid())
        return {};
    const QTime t(time.hour(), time.minute(), 0);

    for (int i = 0; i <= 7; ++i) {
        const QDate day = from.date().addDays(i);
        if (days != 0 && !(days & bitFor(day)))
            continue;
        const QDateTime candidate(day, t);
        if (candidate.isValid() && candidate > from)
            return candidate;
    }
    return {};
}

QString AlarmList::add(Alarm alarm)
{
    if (!alarm.isValid())
        return {};
    alarm.id = newId();
    while (std::any_of(m_alarms.begin(), m_alarms.end(), [&](const Alarm &a) { return a.id == alarm.id; }))
        alarm.id = newId();
    alarm.time = QTime(alarm.time.hour(), alarm.time.minute());
    m_alarms.append(alarm);
    return alarm.id;
}

bool AlarmList::update(const Alarm &alarm)
{
    if (!alarm.isValid())
        return false;
    for (Alarm &a : m_alarms) {
        if (a.id == alarm.id) {
            a = alarm;
            a.time = QTime(a.time.hour(), a.time.minute());
            return true;
        }
    }
    return false;
}

bool AlarmList::remove(const QString &id)
{
    for (qsizetype i = 0; i < m_alarms.size(); ++i) {
        if (m_alarms.at(i).id == id) {
            m_alarms.removeAt(i);
            return true;
        }
    }
    return false;
}

bool AlarmList::setEnabled(const QString &id, bool enabled)
{
    for (Alarm &a : m_alarms) {
        if (a.id == id) {
            a.enabled = enabled;
            return true;
        }
    }
    return false;
}

Alarm AlarmList::get(const QString &id) const
{
    for (const Alarm &a : m_alarms)
        if (a.id == id)
            return a;
    return {};
}

QList<Alarm> AlarmList::collectDue(const QDateTime &previous, const QDateTime &now)
{
    QList<Alarm> due;
    if (!previous.isValid() || !now.isValid() || now <= previous)
        return due;

    QDateTime from = previous;
    if (previous.secsTo(now) > 120)
        from = now.addSecs(-60);

    for (Alarm &a : m_alarms) {
        if (!a.enabled)
            continue;
        const QDateTime t = a.nextTrigger(from);
        if (t.isValid() && t <= now) {
            due.append(a);
            if (a.days == 0)
                a.enabled = false;
        }
    }
    return due;
}

QPair<Alarm, QDateTime> AlarmList::nextAlarm(const QDateTime &from) const
{
    QPair<Alarm, QDateTime> best;
    for (const Alarm &a : m_alarms) {
        if (!a.enabled)
            continue;
        const QDateTime t = a.nextTrigger(from);
        if (t.isValid() && (!best.second.isValid() || t < best.second))
            best = {a, t};
    }
    return best;
}

QString daysToText(int days)
{
    auto tr = [](const char *s) { return QCoreApplication::translate("colibry::clock", s); };
    if (days == 0)
        return tr("Once");
    if (days == Every)
        return tr("Every day");
    if (days == Weekdays)
        return tr("Weekdays");
    if (days == Weekend)
        return tr("Weekends");

    const QLocale locale;
    QStringList names;
    for (int d = 1; d <= 7; ++d)
        if (days & (1 << (d - 1)))
            names << locale.dayName(d, QLocale::ShortFormat);
    return names.join(QStringLiteral(", "));
}

} // namespace colibry::clock
