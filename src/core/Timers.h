#pragma once

#include <QList>
#include <QString>

namespace colibry::clock {

/// "1:02:03.45" style text. Hours are shown only when needed (or when forceHours).
QString formatDuration(qint64 ms, bool showCentiseconds, bool forceHours = false);

struct Lap
{
    int number = 0;
    qint64 split = 0; ///< time of this lap alone
    qint64 total = 0; ///< time since the start
};

/// Stopwatch driven by a monotonic millisecond clock that the caller passes in.
class Stopwatch
{
public:
    void start(qint64 nowMs);
    void stop(qint64 nowMs);
    void reset();
    /// Records a lap while running. Returns false when stopped.
    bool lap(qint64 nowMs);

    bool isRunning() const { return m_running; }
    qint64 elapsed(qint64 nowMs) const;
    const QList<Lap> &laps() const { return m_laps; }

private:
    bool m_running = false;
    qint64 m_startedAt = 0;
    qint64 m_accumulated = 0;
    qint64 m_lastLapTotal = 0;
    QList<Lap> m_laps;
};

/// Countdown timer.
class CountdownTimer
{
public:
    static constexpr qint64 kMinMs = 1000;
    static constexpr qint64 kMaxMs = 99LL * 3600 * 1000 + 59 * 60 * 1000 + 59 * 1000;

    /// Sets the length. Only possible while it is not running. Values are limited to 1 s … 99:59:59.
    bool setDuration(qint64 ms);
    qint64 duration() const { return m_duration; }

    void start(qint64 nowMs);
    void pause(qint64 nowMs);
    /// Back to the full duration (also clears "finished").
    void reset();

    bool isRunning() const { return m_running; }
    qint64 remaining(qint64 nowMs) const;
    /// True once the countdown has reached zero, until reset().
    bool isFinished(qint64 nowMs) const;

private:
    qint64 m_duration = 5 * 60 * 1000;
    bool m_running = false;
    qint64 m_startedAt = 0;
    qint64 m_used = 0; ///< time already counted before the last start
};

} // namespace colibry::clock
