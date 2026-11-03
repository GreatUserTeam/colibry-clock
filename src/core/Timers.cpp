#include "Timers.h"

#include <algorithm>

namespace colibry::clock {

QString formatDuration(qint64 ms, bool showCentiseconds, bool forceHours)
{
    if (ms < 0)
        ms = 0;
    const qint64 totalSeconds = ms / 1000;
    const int centi = static_cast<int>((ms % 1000) / 10);
    const qint64 h = totalSeconds / 3600;
    const int m = static_cast<int>((totalSeconds / 60) % 60);
    const int s = static_cast<int>(totalSeconds % 60);

    QString text;
    if (h > 0 || forceHours)
        text = QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
    else
        text = QStringLiteral("%1:%2").arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
    if (showCentiseconds)
        text += QStringLiteral(".%1").arg(centi, 2, 10, QLatin1Char('0'));
    return text;
}

// --- Stopwatch --------------------------------------------------------------

void Stopwatch::start(qint64 nowMs)
{
    if (m_running)
        return;
    m_running = true;
    m_startedAt = nowMs;
}

void Stopwatch::stop(qint64 nowMs)
{
    if (!m_running)
        return;
    m_accumulated += std::max<qint64>(0, nowMs - m_startedAt);
    m_running = false;
}

void Stopwatch::reset()
{
    m_running = false;
    m_accumulated = 0;
    m_startedAt = 0;
    m_lastLapTotal = 0;
    m_laps.clear();
}

qint64 Stopwatch::elapsed(qint64 nowMs) const
{
    return m_accumulated + (m_running ? std::max<qint64>(0, nowMs - m_startedAt) : 0);
}

bool Stopwatch::lap(qint64 nowMs)
{
    if (!m_running)
        return false;
    const qint64 total = elapsed(nowMs);
    Lap l;
    l.number = static_cast<int>(m_laps.size()) + 1;
    l.total = total;
    l.split = total - m_lastLapTotal;
    m_lastLapTotal = total;
    m_laps.append(l);
    return true;
}

// --- CountdownTimer ---------------------------------------------------------

bool CountdownTimer::setDuration(qint64 ms)
{
    if (m_running)
        return false;
    m_duration = std::clamp(ms, kMinMs, kMaxMs);
    m_used = 0;
    return true;
}

void CountdownTimer::start(qint64 nowMs)
{
    if (m_running || m_used >= m_duration)
        return;
    m_running = true;
    m_startedAt = nowMs;
}

void CountdownTimer::pause(qint64 nowMs)
{
    if (!m_running)
        return;
    m_used = std::min(m_duration, m_used + std::max<qint64>(0, nowMs - m_startedAt));
    m_running = false;
}

void CountdownTimer::reset()
{
    m_running = false;
    m_used = 0;
}

qint64 CountdownTimer::remaining(qint64 nowMs) const
{
    const qint64 used = m_used + (m_running ? std::max<qint64>(0, nowMs - m_startedAt) : 0);
    return std::max<qint64>(0, m_duration - used);
}

bool CountdownTimer::isFinished(qint64 nowMs) const
{
    return remaining(nowMs) == 0;
}

} // namespace colibry::clock
