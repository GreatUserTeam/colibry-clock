#pragma once

#include <QDateTime>
#include <QElapsedTimer>

namespace colibry::clock {

/// Where "now" comes from. Tests use a fake one, so alarms and timers can be checked without waiting.
class TimeSource
{
public:
    virtual ~TimeSource() = default;
    virtual QDateTime nowUtc() const = 0;
    /// Milliseconds on a clock that never jumps (unaffected by a changed system time).
    virtual qint64 monotonicMs() const = 0;
};

class SystemTimeSource : public TimeSource
{
public:
    SystemTimeSource() { m_timer.start(); }
    QDateTime nowUtc() const override { return QDateTime::currentDateTimeUtc(); }
    qint64 monotonicMs() const override { return m_timer.elapsed(); }

private:
    QElapsedTimer m_timer;
};

} // namespace colibry::clock
