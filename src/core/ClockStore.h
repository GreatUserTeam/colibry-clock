#pragma once

#include "Alarms.h"

#include <QStringList>

namespace colibry::clock {

/// Alarms and the list of world-clock cities, kept in one JSON file (written atomically).
/// A damaged file is renamed to "<name>.corrupt" and the app starts with defaults; single bad
/// entries are skipped.
class ClockStore
{
public:
    explicit ClockStore(QString fileName);
    static QString defaultFileName();

    void load();
    bool save();
    QString loadWarning() const { return m_warning; }
    QString lastError() const { return m_error; }

    AlarmList &alarms() { return m_alarms; }
    const AlarmList &alarms() const { return m_alarms; }

    /// Time zone ids (IANA names or "UTC±hh:mm").
    QStringList zones() const { return m_zones; }
    bool addZone(const QString &id);
    bool removeZone(const QString &id);
    static bool isValidZone(const QString &id);

private:
    QString m_file;
    AlarmList m_alarms;
    QStringList m_zones;
    QString m_warning;
    QString m_error;
};

} // namespace colibry::clock
