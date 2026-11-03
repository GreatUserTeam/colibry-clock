#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include <QTime>

namespace colibry::clock {

/// Weekday bits: Monday = 1 … Sunday = 64. No bit set means "once".
enum Day : int {
    Mon = 1, Tue = 2, Wed = 4, Thu = 8, Fri = 16, Sat = 32, Sun = 64,
    Weekdays = Mon | Tue | Wed | Thu | Fri,
    Weekend = Sat | Sun,
    Every = Weekdays | Weekend,
};

struct Alarm
{
    QString id;
    QTime time;
    QString label;
    bool enabled = true;
    int days = 0; ///< see Day; 0 = rings once, then switches itself off

    bool isValid() const;
    /// The first moment strictly after `from` (local time) at which the alarm rings.
    /// Invalid when the alarm is invalid.
    QDateTime nextTrigger(const QDateTime &from) const;
};

/// The list of alarms and the logic that decides which of them ring.
class AlarmList
{
public:
    QString add(Alarm alarm); ///< returns the id, empty when invalid
    bool update(const Alarm &alarm);
    bool remove(const QString &id);
    bool setEnabled(const QString &id, bool enabled);
    Alarm get(const QString &id) const;
    const QList<Alarm> &all() const { return m_alarms; }
    void setAll(QList<Alarm> alarms) { m_alarms = std::move(alarms); }

    /// Alarms that rang after `previous` up to and including `now` (local times).
    ///
    /// Alarms that ring once are switched off here. If more than two minutes passed (the computer
    /// was asleep) only the last minute is considered, so a long sleep does not make every alarm
    /// of the night go off together. If the clock went backwards nothing rings.
    QList<Alarm> collectDue(const QDateTime &previous, const QDateTime &now);

    /// The alarm that rings next, with its time; invalid id when none is enabled.
    QPair<Alarm, QDateTime> nextAlarm(const QDateTime &from) const;

private:
    QList<Alarm> m_alarms;
};

QString daysToText(int days);

} // namespace colibry::clock
