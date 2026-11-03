#include "ClockWindow.h"
#include "colibry/Keys.h"
#include "colibry/SettingsStore.h"

#include <QTabWidget>
#include <QTemporaryDir>
#include <QtTest>

using namespace colibry;
using namespace colibry::clock;

namespace {

class FakeTime : public TimeSource
{
public:
    QDateTime now = QDateTime(QDate(2026, 3, 10), QTime(6, 59, 50), QTimeZone::utc());
    qint64 mono = 1000;
    QDateTime nowUtc() const override { return now; }
    qint64 monotonicMs() const override { return mono; }
    void advance(qint64 ms)
    {
        now = now.addMSecs(ms);
        mono += ms;
    }
};

Alarm alarm(int h, int m, int days = 0, const QString &label = {})
{
    Alarm a;
    a.time = QTime(h, m);
    a.days = days;
    a.label = label;
    return a;
}

} // namespace

class ClockUiTest : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QVERIFY(m_tmp.isValid());
        QDir(m_tmp.path()).removeRecursively();
        QDir().mkpath(m_tmp.path());
        m_settings = std::make_unique<SettingsStore>(m_tmp.path() + QStringLiteral("/s.ini"));
        m_settings->setValue(Keys::TimeZone, QStringLiteral("UTC")); // a known "home" zone
        m_settings->setValue(Keys::Clock24, true);
        m_time = std::make_unique<FakeTime>();
        m_store = std::make_unique<ClockStore>(m_tmp.path() + QStringLiteral("/clock.json"));
    }

    void showsHomeTimeInConfiguredZoneAndFormat()
    {
        ClockWindow w(*m_time, *m_store, *m_settings);
        QCOMPARE(w.localTimeText(), QStringLiteral("06:59:50"));
        w.setTwentyFourHour(false);
        QCOMPARE(w.localTimeText(), QStringLiteral("6:59:50 AM"));
        m_settings->setValue(Keys::TimeZone, QStringLiteral("UTC+03:00"));
        w.setTwentyFourHour(true);
        QCOMPARE(w.localTimeText(), QStringLiteral("09:59:50"));
    }

    void fallsBackWhenConfiguredZoneIsUnknown()
    {
        m_settings->setValue(Keys::TimeZone, QStringLiteral("Mars/Olympus"));
        ClockWindow w(*m_time, *m_store, *m_settings);
        QVERIFY(!w.localTimeText().isEmpty()); // no crash, system zone used
    }

    void worldClockAddRemoveAndPersist()
    {
        {
            ClockWindow w(*m_time, *m_store, *m_settings);
            QVERIFY(w.addZone(QStringLiteral("UTC+09:00")));
            QVERIFY(!w.addZone(QStringLiteral("UTC+09:00")));
            QVERIFY(!w.addZone(QStringLiteral("nonsense")));
            QCOMPARE(w.worldClockLines().size(), 1);
            QVERIFY(w.worldClockLines().first().contains(QStringLiteral("15:59")));
        }
        ClockStore again(m_tmp.path() + QStringLiteral("/clock.json"));
        again.load();
        QCOMPARE(again.zones(), QStringList{QStringLiteral("UTC+09:00")});
        ClockWindow w2(*m_time, again, *m_settings);
        QVERIFY(w2.removeZone(QStringLiteral("UTC+09:00")));
        QVERIFY(w2.worldClockLines().isEmpty());
    }

    void alarmRingsAtTheRightSecond()
    {
        ClockWindow w(*m_time, *m_store, *m_settings);
        w.addAlarm(alarm(7, 0, 0, QStringLiteral("Wake up")));
        w.tick();
        QVERIFY(w.ringing().isEmpty());
        m_time->advance(9000); // 06:59:59
        w.tick();
        QVERIFY(w.ringing().isEmpty());
        m_time->advance(1000); // 07:00:00
        w.tick();
        QCOMPARE(w.ringing(), QStringList{QStringLiteral("Wake up")});
        // one-time alarm switched itself off, and that was saved
        QVERIFY(!m_store->alarms().all().first().enabled);
        ClockStore again(m_tmp.path() + QStringLiteral("/clock.json"));
        again.load();
        QVERIFY(!again.alarms().all().first().enabled);
        m_time->advance(1000);
        w.tick();
        QCOMPARE(w.ringing().size(), 1); // did not ring again
        w.dismissRinging();
        QVERIFY(w.ringing().isEmpty());
    }

    void unnamedAlarmShowsItsTime()
    {
        ClockWindow w(*m_time, *m_store, *m_settings);
        w.addAlarm(alarm(7, 0));
        m_time->advance(10'000);
        w.tick();
        QCOMPARE(w.ringing().size(), 1);
        QVERIFY(w.ringing().first().contains(QStringLiteral("07:00")));
    }

    void alarmEditing()
    {
        ClockWindow w(*m_time, *m_store, *m_settings);
        const QString id = w.addAlarm(alarm(8, 0, Weekdays));
        QVERIFY(!id.isEmpty());
        QCOMPARE(w.alarmLines().size(), 1);
        QVERIFY(w.setAlarmEnabled(id, false));
        QVERIFY(w.alarmLines().first().contains(QStringLiteral("off")));
        QVERIFY(w.nextAlarmText().contains(QStringLiteral("No alarms")));
        QVERIFY(w.setAlarmEnabled(id, true));
        QVERIFY(w.nextAlarmText().contains(QStringLiteral("Next alarm")));
        QVERIFY(w.removeAlarm(id));
        QVERIFY(!w.removeAlarm(id));
        QVERIFY(w.addAlarm(Alarm()).isEmpty()); // invalid
    }

    void hidingTheWindowDoesNotCauseCatchUpRings()
    {
        ClockWindow w(*m_time, *m_store, *m_settings);
        w.addAlarm(alarm(7, 0, Every));
        w.show();
        m_time->advance(3600 * 1000); // an hour passes without ticks (e.g. suspended)
        w.tick();
        QVERIFY(w.ringing().isEmpty()); // 07:00 was 59 minutes ago: too old to ring
    }

    void stopwatchBasics()
    {
        ClockWindow w(*m_time, *m_store, *m_settings);
        QCOMPARE(w.stopwatchText(), QStringLiteral("00:00.00"));
        w.stopwatchStartStop();
        QVERIFY(w.stopwatchRunning());
        m_time->advance(1230);
        QCOMPARE(w.stopwatchText(), QStringLiteral("00:01.23"));
        w.stopwatchLap();
        QCOMPARE(w.lapCount(), 1);
        m_time->advance(1000);
        w.stopwatchStartStop();
        QVERIFY(!w.stopwatchRunning());
        m_time->advance(5000);
        QCOMPARE(w.stopwatchText(), QStringLiteral("00:02.23"));
        w.stopwatchLap(); // not running: ignored
        QCOMPARE(w.lapCount(), 1);
        w.stopwatchReset();
        QCOMPARE(w.stopwatchText(), QStringLiteral("00:00.00"));
        QCOMPARE(w.lapCount(), 0);
    }

    void timerCountsDownRingsAndStopsAtZero()
    {
        ClockWindow w(*m_time, *m_store, *m_settings);
        QVERIFY(w.timerSetDuration(3000));
        QCOMPARE(w.timerText(), QStringLiteral("00:03"));
        w.timerStartPause();
        QVERIFY(w.timerRunning());
        m_time->advance(400);
        w.tick();
        QCOMPARE(w.timerText(), QStringLiteral("00:03")); // 2.6 s left shows 3
        m_time->advance(1000);
        QCOMPARE(w.timerText(), QStringLiteral("00:02"));
        QVERIFY(w.ringing().isEmpty());
        m_time->advance(1700);
        w.tick();
        QVERIFY(!w.timerRunning());
        QCOMPARE(w.timerText(), QStringLiteral("00:00"));
        QCOMPARE(w.ringing(), QStringList{QStringLiteral("Timer finished")});
        w.tick();
        QCOMPARE(w.ringing().size(), 1); // rings once
    }

    void timerCanRunAgainAfterFinishing()
    {
        ClockWindow w(*m_time, *m_store, *m_settings);
        w.timerSetDuration(2000);
        w.timerStartPause();
        m_time->advance(2500);
        w.tick();
        QVERIFY(!w.timerRunning());
        w.dismissRinging();
        w.timerStartPause(); // starts over from the full time
        QVERIFY(w.timerRunning());
        QCOMPARE(w.timerText(), QStringLiteral("00:02"));
        m_time->advance(2500);
        w.tick();
        QCOMPARE(w.ringing().size(), 1);
    }

    void timerPauseKeepsRemaining()
    {
        ClockWindow w(*m_time, *m_store, *m_settings);
        w.timerSetDuration(10'000);
        w.timerStartPause();
        m_time->advance(4000);
        w.timerStartPause();
        QVERIFY(!w.timerRunning());
        m_time->advance(60'000);
        QCOMPARE(w.timerText(), QStringLiteral("00:06"));
        QVERIFY(w.timerSetDuration(20'000)); // can be changed while not running
        w.timerStartPause();
        QVERIFY(!w.timerSetDuration(5000));  // but not while running
    }

    void geometryIsRemembered()
    {
        {
            ClockWindow w(*m_time, *m_store, *m_settings);
            w.show();
            w.close();
        }
        QVERIFY(!m_settings->value(Keys::ClockGeometry).toByteArray().isEmpty());
    }

    void hasFourTabs()
    {
        ClockWindow w(*m_time, *m_store, *m_settings);
        QCOMPARE(w.tabs()->count(), 4);
    }

    void showsLoadWarning()
    {
        QFile f(m_tmp.path() + QStringLiteral("/clock.json"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("garbage");
        f.close();
        m_store->load();
        QVERIFY(!m_store->loadWarning().isEmpty());
        ClockWindow w(*m_time, *m_store, *m_settings);
        QVERIFY(w.alarmLines().isEmpty());
    }

private:
    QTemporaryDir m_tmp;
    std::unique_ptr<SettingsStore> m_settings;
    std::unique_ptr<FakeTime> m_time;
    std::unique_ptr<ClockStore> m_store;
};

QTEST_MAIN(ClockUiTest)
#include "test_ui.moc"
