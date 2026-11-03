#pragma once

#include "core/Alarms.h"
#include "core/ClockStore.h"
#include "core/TimeSource.h"
#include "core/Timers.h"

#include <QMainWindow>

class QCheckBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QTimeEdit;
class QTimer;
class QComboBox;
class QLineEdit;

namespace colibry {
class SettingsStore;
}

namespace colibry::clock {

/// Clock window with four tabs: world clock, alarms, stopwatch and timer.
/// All timing goes through a TimeSource, and tick() does one update step, so the whole
/// behavior can be tested with a fake clock.
class ClockWindow : public QMainWindow
{
    Q_OBJECT

public:
    ClockWindow(TimeSource &time, ClockStore &store, SettingsStore &settings, QWidget *parent = nullptr);

    /// One update step (the window calls it by itself every 100 ms while visible).
    void tick();

    // World clock
    QString localTimeText() const;
    QStringList worldClockLines() const;
    bool addZone(const QString &id);
    bool removeZone(const QString &id);

    // Alarms
    QString addAlarm(const Alarm &alarm);
    bool removeAlarm(const QString &id);
    bool setAlarmEnabled(const QString &id, bool enabled);
    QStringList alarmLines() const;
    QString nextAlarmText() const;
    /// What is ringing right now (alarm labels / "Timer finished").
    QStringList ringing() const { return m_ringing; }
    void dismissRinging();

    // Stopwatch
    void stopwatchStartStop();
    void stopwatchLap();
    void stopwatchReset();
    QString stopwatchText() const;
    bool stopwatchRunning() const { return m_stopwatch.isRunning(); }
    int lapCount() const { return static_cast<int>(m_stopwatch.laps().size()); }

    // Timer
    bool timerSetDuration(qint64 ms);
    void timerStartPause();
    void timerReset();
    QString timerText() const;
    bool timerRunning() const { return m_timer.isRunning(); }

    QTabWidget *tabs() const { return m_tabs; }
    void setTwentyFourHour(bool on);

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void buildUi();
    QWidget *buildWorldTab();
    QWidget *buildAlarmTab();
    QWidget *buildStopwatchTab();
    QWidget *buildTimerTab();
    void refreshWorld();
    void refreshAlarms();
    void refreshStopwatch();
    void refreshTimer();
    void refreshRinging();
    void ring(const QString &what);
    QDateTime wallClockNow() const; ///< "what the wall clock shows" in the home zone, as a naive time
    QTimeZone homeZone() const;
    bool clock24() const;

    TimeSource &m_time;
    ClockStore &m_store;
    SettingsStore &m_settings;

    Stopwatch m_stopwatch;
    CountdownTimer m_timer;
    QDateTime m_lastWall;
    bool m_timerWasFinished = false;
    QStringList m_ringing;
    bool m_forced24 = false;
    bool m_force24Value = false;

    QTimer *m_tick = nullptr;
    QTabWidget *m_tabs = nullptr;
    QLabel *m_big = nullptr;
    QLabel *m_banner = nullptr;
    QPushButton *m_dismiss = nullptr;

    QListWidget *m_world = nullptr;
    QComboBox *m_zoneBox = nullptr;

    QListWidget *m_alarmList = nullptr;
    QLabel *m_nextAlarm = nullptr;
    QTimeEdit *m_alarmTime = nullptr;
    QLineEdit *m_alarmLabel = nullptr;
    QList<QCheckBox *> m_dayBoxes;

    QLabel *m_swLabel = nullptr;
    QPushButton *m_swStart = nullptr;
    QPushButton *m_swLap = nullptr;
    QListWidget *m_swLaps = nullptr;

    QLabel *m_tmLabel = nullptr;
    QPushButton *m_tmStart = nullptr;
    QSpinBox *m_tmH = nullptr;
    QSpinBox *m_tmM = nullptr;
    QSpinBox *m_tmS = nullptr;
};

} // namespace colibry::clock
