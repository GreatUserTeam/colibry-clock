#include "core/Alarms.h"
#include "core/ClockStore.h"
#include "core/Timers.h"
#include "core/WorldClock.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace colibry::clock;

namespace {
QDateTime at(int y, int mo, int d, int h, int mi, int s = 0)
{
    return QDateTime(QDate(y, mo, d), QTime(h, mi, s));
}
Alarm alarm(int h, int m, int days = 0, const QString &label = {})
{
    Alarm a;
    a.time = QTime(h, m);
    a.days = days;
    a.label = label;
    return a;
}
} // namespace

class ClockCoreTest : public QObject
{
    Q_OBJECT

private slots:
    // --- formatting -----------------------------------------------------------
    void formatsDurations()
    {
        QCOMPARE(formatDuration(0, false), QStringLiteral("00:00"));
        QCOMPARE(formatDuration(0, true), QStringLiteral("00:00.00"));
        QCOMPARE(formatDuration(65'432, true), QStringLiteral("01:05.43"));
        QCOMPARE(formatDuration(3'600'000, false), QStringLiteral("1:00:00"));
        QCOMPARE(formatDuration(3'723'450, true), QStringLiteral("1:02:03.45"));
        QCOMPARE(formatDuration(5'000, false, true), QStringLiteral("0:00:05"));
        QCOMPARE(formatDuration(-5, true), QStringLiteral("00:00.00"));
        QCOMPARE(formatDuration(100LL * 3600 * 1000, false), QStringLiteral("100:00:00"));
    }

    // --- stopwatch ------------------------------------------------------------
    void stopwatchCountsOnlyWhileRunning()
    {
        Stopwatch s;
        QCOMPARE(s.elapsed(1000), 0);
        s.start(1000);
        QCOMPARE(s.elapsed(3500), 2500);
        s.stop(4000);
        QCOMPARE(s.elapsed(9000), 3000);
        s.start(10000);
        QCOMPARE(s.elapsed(10500), 3500);
        s.reset();
        QCOMPARE(s.elapsed(20000), 0);
        QVERIFY(!s.isRunning());
    }

    void stopwatchIgnoresDoubleStartAndStop()
    {
        Stopwatch s;
        s.start(0);
        s.start(500); // must not restart
        QCOMPARE(s.elapsed(1000), 1000);
        s.stop(1000);
        s.stop(5000); // must not add more
        QCOMPARE(s.elapsed(9000), 1000);
    }

    void stopwatchLaps()
    {
        Stopwatch s;
        QVERIFY(!s.lap(100)); // not running
        s.start(0);
        QVERIFY(s.lap(1000));
        QVERIFY(s.lap(2500));
        s.stop(3000);
        s.start(4000);
        QVERIFY(s.lap(4500)); // total 3500
        const auto laps = s.laps();
        QCOMPARE(laps.size(), 3);
        QCOMPARE(laps.at(0).split, 1000);
        QCOMPARE(laps.at(1).split, 1500);
        QCOMPARE(laps.at(1).total, 2500);
        QCOMPARE(laps.at(2).split, 1000);
        QCOMPARE(laps.at(2).total, 3500);
        QCOMPARE(laps.at(2).number, 3);
        s.reset();
        QVERIFY(s.laps().isEmpty());
    }

    void stopwatchSurvivesClockGoingBackwards()
    {
        Stopwatch s;
        s.start(5000);
        QCOMPARE(s.elapsed(4000), 0); // never negative
        s.stop(3000);
        QCOMPARE(s.elapsed(9000), 0);
    }

    // --- countdown ------------------------------------------------------------
    void timerCountsDownAndFinishes()
    {
        CountdownTimer t;
        QVERIFY(t.setDuration(10'000));
        QCOMPARE(t.remaining(0), 10'000);
        t.start(1000);
        QCOMPARE(t.remaining(4000), 7000);
        QVERIFY(!t.isFinished(10'999));
        QVERIFY(t.isFinished(11'000));
        QCOMPARE(t.remaining(99'999), 0);
    }

    void timerPauseResume()
    {
        CountdownTimer t;
        t.setDuration(10'000);
        t.start(0);
        t.pause(3000);
        QCOMPARE(t.remaining(50'000), 7000);
        t.start(60'000);
        QCOMPARE(t.remaining(62'000), 5000);
        t.reset();
        QCOMPARE(t.remaining(70'000), 10'000);
        QVERIFY(!t.isRunning());
    }

    void timerDurationIsLimitedAndLockedWhileRunning()
    {
        CountdownTimer t;
        t.setDuration(0);
        QCOMPARE(t.duration(), CountdownTimer::kMinMs);
        t.setDuration(Q_INT64_C(1) << 50);
        QCOMPARE(t.duration(), CountdownTimer::kMaxMs);
        t.setDuration(5000);
        t.start(0);
        QVERIFY(!t.setDuration(9000));
        QCOMPARE(t.duration(), 5000);
    }

    void finishedTimerDoesNotStartAgainUntilReset()
    {
        CountdownTimer t;
        t.setDuration(2000);
        t.start(0);
        t.pause(5000); // overshoot is capped
        QCOMPARE(t.remaining(5000), 0);
        t.start(6000);
        QVERIFY(!t.isRunning());
        t.reset();
        t.start(6000);
        QVERIFY(t.isRunning());
    }

    // --- alarms ---------------------------------------------------------------
    void nextTriggerForOneTimeAlarm()
    {
        const Alarm a = alarm(7, 30);
        QCOMPARE(a.nextTrigger(at(2026, 3, 10, 7, 0)), at(2026, 3, 10, 7, 30));
        QCOMPARE(a.nextTrigger(at(2026, 3, 10, 7, 30)), at(2026, 3, 11, 7, 30)); // strictly after
        QCOMPARE(a.nextTrigger(at(2026, 3, 10, 7, 29, 59)), at(2026, 3, 10, 7, 30));
        QCOMPARE(a.nextTrigger(at(2026, 12, 31, 23, 0)), at(2027, 1, 1, 7, 30));
    }

    void nextTriggerRespectsWeekdays()
    {
        const Alarm a = alarm(8, 0, Weekdays);
        // 2026-03-14 is a Saturday
        QCOMPARE(QDate(2026, 3, 14).dayOfWeek(), 6);
        QCOMPARE(a.nextTrigger(at(2026, 3, 14, 9, 0)), at(2026, 3, 16, 8, 0)); // Monday
        QCOMPARE(a.nextTrigger(at(2026, 3, 13, 7, 0)), at(2026, 3, 13, 8, 0)); // Friday morning
        const Alarm sunday = alarm(8, 0, Sun);
        QCOMPARE(sunday.nextTrigger(at(2026, 3, 14, 9, 0)), at(2026, 3, 15, 8, 0));
        QCOMPARE(sunday.nextTrigger(at(2026, 3, 15, 8, 0)), at(2026, 3, 22, 8, 0));
    }

    void invalidAlarmsAreRejected()
    {
        Alarm a;
        QVERIFY(!a.isValid());
        QVERIFY(!a.nextTrigger(at(2026, 1, 1, 0, 0)).isValid());
        a.time = QTime(1, 0);
        a.days = 999;
        QVERIFY(!a.isValid());
        AlarmList list;
        QVERIFY(list.add(a).isEmpty());
        QVERIFY(list.all().isEmpty());
    }

    void oneTimeAlarmRingsOnceThenSwitchesOff()
    {
        AlarmList list;
        const QString id = list.add(alarm(7, 0));
        QVERIFY(list.collectDue(at(2026, 3, 10, 6, 59, 58), at(2026, 3, 10, 6, 59, 59)).isEmpty());
        const auto due = list.collectDue(at(2026, 3, 10, 6, 59, 59), at(2026, 3, 10, 7, 0, 0));
        QCOMPARE(due.size(), 1);
        QCOMPARE(due.first().id, id);
        QVERIFY(!list.get(id).enabled);
        QVERIFY(list.collectDue(at(2026, 3, 10, 7, 0, 0), at(2026, 3, 10, 7, 0, 1)).isEmpty());
    }

    void repeatingAlarmStaysOnAndRingsNextDay()
    {
        AlarmList list;
        const QString id = list.add(alarm(7, 0, Every));
        QCOMPARE(list.collectDue(at(2026, 3, 10, 6, 59, 59), at(2026, 3, 10, 7, 0, 0)).size(), 1);
        QVERIFY(list.get(id).enabled);
        QCOMPARE(list.collectDue(at(2026, 3, 11, 6, 59, 59), at(2026, 3, 11, 7, 0, 0)).size(), 1);
    }

    void alarmDoesNotRingTwiceInTheSameMinute()
    {
        AlarmList list;
        list.add(alarm(7, 0, Every));
        int rings = 0;
        QDateTime t = at(2026, 3, 10, 6, 59, 55);
        for (int i = 0; i < 130; ++i) { // 130 one-second steps = 7:02:05
            const QDateTime next = t.addSecs(1);
            rings += static_cast<int>(list.collectDue(t, next).size());
            t = next;
        }
        QCOMPARE(rings, 1);
    }

    void disabledAlarmDoesNotRing()
    {
        AlarmList list;
        const QString id = list.add(alarm(7, 0, Every));
        list.setEnabled(id, false);
        QVERIFY(list.collectDue(at(2026, 3, 10, 6, 59, 59), at(2026, 3, 10, 7, 0, 0)).isEmpty());
    }

    void longSleepOnlyRingsTheLastMinute()
    {
        AlarmList list;
        list.add(alarm(3, 0, Every, QStringLiteral("night")));
        list.add(alarm(8, 0, Every, QStringLiteral("morning")));
        // Computer slept from 22:00, wakes at 08:00:20: only the alarm of the last minute rings.
        const auto due = list.collectDue(at(2026, 3, 10, 22, 0), at(2026, 3, 11, 8, 0, 20));
        QCOMPARE(due.size(), 1);
        QCOMPARE(due.first().label, QStringLiteral("morning"));
    }

    void clockGoingBackwardsRingsNothing()
    {
        AlarmList list;
        list.add(alarm(7, 0, Every));
        QVERIFY(list.collectDue(at(2026, 3, 10, 7, 0, 5), at(2026, 3, 10, 6, 59, 59)).isEmpty());
        QVERIFY(list.collectDue(at(2026, 3, 10, 7, 0, 5), at(2026, 3, 10, 7, 0, 5)).isEmpty());
        QVERIFY(list.collectDue(QDateTime(), at(2026, 3, 10, 7, 0, 5)).isEmpty());
    }

    void severalAlarmsAtOnce()
    {
        AlarmList list;
        list.add(alarm(7, 0));
        list.add(alarm(7, 0, Every));
        QCOMPARE(list.collectDue(at(2026, 3, 10, 6, 59, 59), at(2026, 3, 10, 7, 0, 0)).size(), 2);
    }

    void nextAlarmPicksTheEarliestEnabled()
    {
        AlarmList list;
        const QString late = list.add(alarm(22, 0));
        const QString early = list.add(alarm(6, 0));
        const QString off = list.add(alarm(5, 0));
        list.setEnabled(off, false);
        auto n = list.nextAlarm(at(2026, 3, 10, 12, 0));
        QCOMPARE(n.first.id, late);
        QCOMPARE(n.second, at(2026, 3, 10, 22, 0));
        n = list.nextAlarm(at(2026, 3, 10, 23, 0));
        QCOMPARE(n.first.id, early);
        QCOMPARE(n.second, at(2026, 3, 11, 6, 0));
        list.setEnabled(late, false);
        list.setEnabled(early, false);
        QVERIFY(!list.nextAlarm(at(2026, 3, 10, 12, 0)).second.isValid());
    }

    void listEditing()
    {
        AlarmList list;
        const QString id = list.add(alarm(7, 0, Mon, QStringLiteral("a")));
        Alarm a = list.get(id);
        a.time = QTime(9, 15, 42); // seconds are dropped
        a.label = QStringLiteral("b");
        QVERIFY(list.update(a));
        QCOMPARE(list.get(id).time, QTime(9, 15));
        a.id = QStringLiteral("missing");
        QVERIFY(!list.update(a));
        QVERIFY(!list.remove(QStringLiteral("missing")));
        QVERIFY(list.remove(id));
        QVERIFY(list.all().isEmpty());
        QVERIFY(!list.setEnabled(id, true));
    }

    void daysText()
    {
        QCOMPARE(daysToText(0), QStringLiteral("Once"));
        QCOMPARE(daysToText(Every), QStringLiteral("Every day"));
        QCOMPARE(daysToText(Weekdays), QStringLiteral("Weekdays"));
        QCOMPARE(daysToText(Weekend), QStringLiteral("Weekends"));
        QVERIFY(daysToText(Mon | Wed).contains(QLatin1Char(',')));
    }

    // --- world clock ----------------------------------------------------------
    void describesZones()
    {
        const QDateTime now(QDate(2026, 3, 10), QTime(22, 30), QTimeZone::utc());
        // local = UTC+2 -> local date is Mar 11 00:30
        const int local = 2 * 3600;
        ZoneInfo tokyo = describeZone(QStringLiteral("UTC+09:00"), now, true, local);
        QVERIFY(tokyo.valid);
        QCOMPARE(tokyo.time, QStringLiteral("07:30"));
        QCOMPARE(tokyo.offset, QStringLiteral("UTC+09:00"));
        QCOMPARE(tokyo.dayShift, 0); // Mar 11 there, Mar 11 here
        ZoneInfo la = describeZone(QStringLiteral("UTC-08:00"), now, false, local);
        QCOMPARE(la.time, QStringLiteral("2:30 PM"));
        QCOMPARE(la.offset, QStringLiteral("UTC-08:00"));
        QCOMPARE(la.dayShift, -1);
        QCOMPARE(la.dayText, QStringLiteral("Yesterday"));
        const ZoneInfo ahead = describeZone(QStringLiteral("UTC+12:00"), now, true, 0);
        QCOMPARE(ahead.dayShift, 1);
        QCOMPARE(ahead.dayText, QStringLiteral("Tomorrow"));
    }

    void invalidZonesAreNotValid()
    {
        const QDateTime now = QDateTime::currentDateTimeUtc();
        QVERIFY(!describeZone(QStringLiteral("Mars/Olympus"), now, true, 0).valid);
        QVERIFY(!describeZone(QString(), now, true, 0).valid);
        QVERIFY(!describeZone(QStringLiteral("UTC"), QDateTime(), true, 0).valid);
    }

    void cityAndOffsetText()
    {
        QCOMPARE(cityName(QStringLiteral("America/New_York")), QStringLiteral("New York"));
        QCOMPARE(cityName(QStringLiteral("UTC")), QStringLiteral("UTC"));
        QCOMPARE(offsetText(0), QStringLiteral("UTC+00:00"));
        QCOMPARE(offsetText(19800), QStringLiteral("UTC+05:30"));
        QCOMPARE(offsetText(-12600), QStringLiteral("UTC-03:30"));
    }

    // --- store ----------------------------------------------------------------
    void storeRoundTrip()
    {
        QTemporaryDir tmp;
        const QString file = tmp.path() + QStringLiteral("/sub/clock.json");
        QString id;
        {
            ClockStore s(file);
            id = s.alarms().add(alarm(6, 45, Weekdays, QStringLiteral("Работа 😀")));
            QVERIFY(s.addZone(QStringLiteral("UTC+09:00")));
            QVERIFY(!s.addZone(QStringLiteral("UTC+09:00"))); // duplicate
            QVERIFY(!s.addZone(QStringLiteral("Mars/Olympus")));
            QVERIFY(s.save());
        }
        ClockStore s(file);
        s.load();
        QVERIFY(s.loadWarning().isEmpty());
        QCOMPARE(s.zones(), QStringList{QStringLiteral("UTC+09:00")});
        const Alarm a = s.alarms().get(id);
        QCOMPARE(a.time, QTime(6, 45));
        QCOMPARE(a.days, static_cast<int>(Weekdays));
        QCOMPARE(a.label, QStringLiteral("Работа 😀"));
        QVERIFY(s.removeZone(QStringLiteral("UTC+09:00")));
        QVERIFY(!s.removeZone(QStringLiteral("UTC+09:00")));
    }

    void storeHandlesMissingCorruptAndPartlyBrokenFiles()
    {
        QTemporaryDir tmp;
        const QString file = tmp.path() + QStringLiteral("/clock.json");
        {
            ClockStore s(file);
            s.load();
            QVERIFY(s.loadWarning().isEmpty());
        }
        {
            QFile f(file);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("not json {");
        }
        {
            ClockStore s(file);
            s.load();
            QVERIFY(!s.loadWarning().isEmpty());
            QVERIFY(QFileInfo::exists(file + QStringLiteral(".corrupt")));
            QVERIFY(s.alarms().all().isEmpty());
        }
        {
            QFile f(file);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(R"({"alarms":[
              {"id":"a","time":"07:00","days":0},
              {"id":"b","time":"25:99"},
              {"id":"c","time":"07:00","days":500},
              {"time":"07:00"},
              5,
              {"id":"a","time":"08:00"}],
              "zones":["UTC","Mars/Olympus","UTC","UTC+01:00",3]})");
        }
        ClockStore s(file);
        s.load();
        QCOMPARE(s.alarms().all().size(), 1);
        QVERIFY(s.loadWarning().contains(QStringLiteral("5")));
        QCOMPARE(s.zones(), (QStringList{QStringLiteral("UTC"), QStringLiteral("UTC+01:00")}));
    }

    void storeReportsUnwritableLocation()
    {
        QTemporaryDir tmp;
        QFile blocker(tmp.path() + QStringLiteral("/x"));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        ClockStore s(tmp.path() + QStringLiteral("/x/clock.json"));
        QVERIFY(!s.save());
        QVERIFY(!s.lastError().isEmpty());
    }
};

QTEST_MAIN(ClockCoreTest)
#include "test_core.moc"
