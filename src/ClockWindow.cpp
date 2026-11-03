#include "ClockWindow.h"

#include "core/WorldClock.h"
#include "colibry/Keys.h"
#include "colibry/SettingsStore.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimeEdit>
#include <QTimer>
#include <QTimeZone>
#include <QVBoxLayout>

namespace colibry::clock {

namespace {
constexpr int kIdRole = Qt::UserRole + 1;

/// "Tokyo  17:23  (Today, UTC+09:00)". A zone that is only an offset has no city, so the offset is not repeated.
QString zoneLine(const ZoneInfo &z)
{
    const bool onlyOffset = z.city.startsWith(QLatin1String("UTC"));
    return onlyOffset ? QStringLiteral("%1   %2   (%3)").arg(z.city, z.time, z.dayText)
                      : QStringLiteral("%1   %2   (%3, %4)").arg(z.city, z.time, z.dayText, z.offset);
}

QFont bigFont(QFont f, double scale)
{
    f.setPointSizeF(f.pointSizeF() * scale);
    f.setBold(true);
    f.setStyleHint(QFont::Monospace);
    return f;
}
} // namespace

ClockWindow::ClockWindow(TimeSource &time, ClockStore &store, SettingsStore &settings, QWidget *parent)
    : QMainWindow(parent)
    , m_time(time)
    , m_store(store)
    , m_settings(settings)
{
    setWindowTitle(tr("Clock"));
    setWindowIcon(QIcon::fromTheme(QStringLiteral("preferences-system-time")));
    resize(560, 560);
    setMinimumSize(420, 420);
    buildUi();

    const QByteArray geometry = m_settings.value(Keys::ClockGeometry).toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);

    m_lastWall = wallClockNow();
    m_tick = new QTimer(this);
    m_tick->setInterval(100);
    connect(m_tick, &QTimer::timeout, this, [this] { tick(); });

    refreshWorld();
    refreshAlarms();
    refreshStopwatch();
    refreshTimer();
    refreshRinging();
    if (!m_store.loadWarning().isEmpty())
        statusBar()->showMessage(m_store.loadWarning());
}

QTimeZone ClockWindow::homeZone() const
{
    const QString id = m_settings.value(Keys::TimeZone).toString();
    if (!id.isEmpty() && QTimeZone::isTimeZoneIdAvailable(id.toUtf8()))
        return QTimeZone(id.toUtf8());
    return QTimeZone::systemTimeZone().isValid() ? QTimeZone::systemTimeZone() : QTimeZone::utc();
}

bool ClockWindow::clock24() const
{
    return m_forced24 ? m_force24Value : m_settings.value(Keys::Clock24).toBool();
}

void ClockWindow::setTwentyFourHour(bool on)
{
    m_forced24 = true;
    m_force24Value = on;
    refreshWorld();
    refreshAlarms();
}

QDateTime ClockWindow::wallClockNow() const
{
    const QDateTime there = m_time.nowUtc().toTimeZone(homeZone());
    return QDateTime(there.date(), QTime(there.time().hour(), there.time().minute(), there.time().second()));
}

void ClockWindow::buildUi()
{
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);

    m_big = new QLabel(central);
    m_big->setAlignment(Qt::AlignCenter);
    m_big->setFont(bigFont(font(), 3.0));
    layout->addWidget(m_big);

    auto *bannerRow = new QHBoxLayout;
    m_banner = new QLabel(central);
    m_banner->setAlignment(Qt::AlignCenter);
    m_banner->setStyleSheet(QStringLiteral("background:#b45309;color:white;padding:6px;border-radius:6px;font-weight:bold;"));
    m_dismiss = new QPushButton(tr("Dismiss"), central);
    bannerRow->addWidget(m_banner, 1);
    bannerRow->addWidget(m_dismiss);
    layout->addLayout(bannerRow);
    connect(m_dismiss, &QPushButton::clicked, this, [this] { dismissRinging(); });

    m_tabs = new QTabWidget(central);
    m_tabs->addTab(buildWorldTab(), tr("World Clock"));
    m_tabs->addTab(buildAlarmTab(), tr("Alarms"));
    m_tabs->addTab(buildStopwatchTab(), tr("Stopwatch"));
    m_tabs->addTab(buildTimerTab(), tr("Timer"));
    layout->addWidget(m_tabs, 1);
    setCentralWidget(central);
}

QWidget *ClockWindow::buildWorldTab()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    m_world = new QListWidget(page);
    m_world->setAlternatingRowColors(true);
    v->addWidget(m_world, 1);

    auto *row = new QHBoxLayout;
    m_zoneBox = new QComboBox(page);
    m_zoneBox->setEditable(true);
    m_zoneBox->setInsertPolicy(QComboBox::NoInsert);
    m_zoneBox->lineEdit()->setPlaceholderText(tr("City or time zone, e.g. Asia/Tokyo"));
    QStringList ids;
    for (const QByteArray &id : QTimeZone::availableTimeZoneIds())
        ids << QString::fromUtf8(id);
    ids.sort();
    m_zoneBox->addItems(ids);
    m_zoneBox->setCurrentIndex(-1);
    auto *add = new QPushButton(tr("Add"), page);
    auto *remove = new QPushButton(tr("Remove"), page);
    row->addWidget(m_zoneBox, 1);
    row->addWidget(add);
    row->addWidget(remove);
    v->addLayout(row);

    connect(add, &QPushButton::clicked, this, [this] {
        const QString id = m_zoneBox->currentText().trimmed();
        if (!addZone(id))
            statusBar()->showMessage(tr("Unknown or already added time zone: %1").arg(id), 5000);
        else
            m_zoneBox->setCurrentIndex(-1);
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        if (auto *item = m_world->currentItem())
            removeZone(item->data(kIdRole).toString());
    });
    return page;
}

QWidget *ClockWindow::buildAlarmTab()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    m_nextAlarm = new QLabel(page);
    v->addWidget(m_nextAlarm);
    m_alarmList = new QListWidget(page);
    m_alarmList->setAlternatingRowColors(true);
    v->addWidget(m_alarmList, 1);

    auto *row1 = new QHBoxLayout;
    m_alarmTime = new QTimeEdit(QTime(7, 0), page);
    m_alarmTime->setDisplayFormat(QStringLiteral("HH:mm"));
    m_alarmLabel = new QLineEdit(page);
    m_alarmLabel->setPlaceholderText(tr("Label (optional)"));
    row1->addWidget(m_alarmTime);
    row1->addWidget(m_alarmLabel, 1);
    v->addLayout(row1);

    auto *row2 = new QHBoxLayout;
    const QLocale locale;
    for (int d = 1; d <= 7; ++d) {
        auto *box = new QCheckBox(locale.dayName(d, QLocale::ShortFormat), page);
        m_dayBoxes.append(box);
        row2->addWidget(box);
    }
    v->addLayout(row2);

    auto *row3 = new QHBoxLayout;
    auto *add = new QPushButton(tr("Add Alarm"), page);
    auto *toggle = new QPushButton(tr("On / Off"), page);
    auto *remove = new QPushButton(tr("Delete"), page);
    row3->addWidget(add);
    row3->addWidget(toggle);
    row3->addWidget(remove);
    v->addLayout(row3);

    connect(add, &QPushButton::clicked, this, [this] {
        Alarm a;
        a.time = m_alarmTime->time();
        a.label = m_alarmLabel->text().trimmed();
        for (int i = 0; i < m_dayBoxes.size(); ++i)
            if (m_dayBoxes.at(i)->isChecked())
                a.days |= 1 << i;
        addAlarm(a);
        m_alarmLabel->clear();
    });
    connect(toggle, &QPushButton::clicked, this, [this] {
        if (auto *item = m_alarmList->currentItem()) {
            const QString id = item->data(kIdRole).toString();
            setAlarmEnabled(id, !m_store.alarms().get(id).enabled);
        }
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        if (auto *item = m_alarmList->currentItem())
            removeAlarm(item->data(kIdRole).toString());
    });
    return page;
}

QWidget *ClockWindow::buildStopwatchTab()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    m_swLabel = new QLabel(page);
    m_swLabel->setAlignment(Qt::AlignCenter);
    m_swLabel->setFont(bigFont(font(), 2.4));
    v->addWidget(m_swLabel);

    auto *row = new QHBoxLayout;
    m_swStart = new QPushButton(tr("Start"), page);
    m_swLap = new QPushButton(tr("Lap"), page);
    auto *reset = new QPushButton(tr("Reset"), page);
    row->addWidget(m_swStart);
    row->addWidget(m_swLap);
    row->addWidget(reset);
    v->addLayout(row);

    m_swLaps = new QListWidget(page);
    m_swLaps->setAlternatingRowColors(true);
    v->addWidget(m_swLaps, 1);

    connect(m_swStart, &QPushButton::clicked, this, [this] { stopwatchStartStop(); });
    connect(m_swLap, &QPushButton::clicked, this, [this] { stopwatchLap(); });
    connect(reset, &QPushButton::clicked, this, [this] { stopwatchReset(); });
    return page;
}

QWidget *ClockWindow::buildTimerTab()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    m_tmLabel = new QLabel(page);
    m_tmLabel->setAlignment(Qt::AlignCenter);
    m_tmLabel->setFont(bigFont(font(), 2.4));
    v->addWidget(m_tmLabel);

    auto *row = new QHBoxLayout;
    row->addStretch();
    auto make = [&](int max, const QString &suffix) {
        auto *s = new QSpinBox(page);
        s->setRange(0, max);
        s->setSuffix(suffix);
        row->addWidget(s);
        return s;
    };
    m_tmH = make(99, tr(" h"));
    m_tmM = make(59, tr(" min"));
    m_tmS = make(59, tr(" s"));
    m_tmM->setValue(5);
    row->addStretch();
    v->addLayout(row);

    auto *buttons = new QHBoxLayout;
    m_tmStart = new QPushButton(tr("Start"), page);
    auto *reset = new QPushButton(tr("Reset"), page);
    buttons->addWidget(m_tmStart);
    buttons->addWidget(reset);
    v->addLayout(buttons);
    v->addStretch(1);

    auto apply = [this] {
        if (!m_timer.isRunning())
            timerSetDuration((static_cast<qint64>(m_tmH->value()) * 3600 + m_tmM->value() * 60 + m_tmS->value()) * 1000);
    };
    connect(m_tmH, &QSpinBox::valueChanged, this, apply);
    connect(m_tmM, &QSpinBox::valueChanged, this, apply);
    connect(m_tmS, &QSpinBox::valueChanged, this, apply);
    connect(m_tmStart, &QPushButton::clicked, this, [this] { timerStartPause(); });
    connect(reset, &QPushButton::clicked, this, [this] { timerReset(); });
    return page;
}

// ---------------------------------------------------------------------------
// Ticking
// ---------------------------------------------------------------------------

void ClockWindow::tick()
{
    const QDateTime wall = wallClockNow();
    const QList<Alarm> due = m_store.alarms().collectDue(m_lastWall, wall);
    m_lastWall = wall;
    if (!due.isEmpty()) {
        for (const Alarm &a : due)
            ring(a.label.isEmpty() ? tr("Alarm %1").arg(a.time.toString(QStringLiteral("HH:mm"))) : a.label);
        m_store.save();
        refreshAlarms();
    }

    const qint64 mono = m_time.monotonicMs();
    const bool finished = m_timer.isRunning() && m_timer.isFinished(mono);
    if (finished && !m_timerWasFinished) {
        m_timer.pause(mono); // stops at zero
        ring(tr("Timer finished"));
    }
    m_timerWasFinished = finished;

    m_big->setText(localTimeText());
    refreshWorld();
    refreshStopwatch();
    refreshTimer();
}

void ClockWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    m_lastWall = wallClockNow(); // do not "catch up" on alarms while the window was hidden
    m_tick->start();
    tick();
}

void ClockWindow::closeEvent(QCloseEvent *event)
{
    m_settings.setValue(Keys::ClockGeometry, saveGeometry());
    QMainWindow::closeEvent(event);
}

// ---------------------------------------------------------------------------
// World clock
// ---------------------------------------------------------------------------

QString ClockWindow::localTimeText() const
{
    const QDateTime now = wallClockNow();
    return now.time().toString(clock24() ? QStringLiteral("HH:mm:ss") : QStringLiteral("h:mm:ss AP"));
}

QStringList ClockWindow::worldClockLines() const
{
    QStringList lines;
    const int local = homeZone().offsetFromUtc(m_time.nowUtc());
    for (const QString &id : m_store.zones()) {
        const ZoneInfo z = describeZone(id, m_time.nowUtc(), clock24(), local);
        if (z.valid)
            lines << zoneLine(z);
    }
    return lines;
}

void ClockWindow::refreshWorld()
{
    const QString keep = m_world->currentItem() ? m_world->currentItem()->data(kIdRole).toString() : QString();
    const QSignalBlocker blocker(m_world);
    m_world->clear();
    const int local = homeZone().offsetFromUtc(m_time.nowUtc());
    for (const QString &id : m_store.zones()) {
        const ZoneInfo z = describeZone(id, m_time.nowUtc(), clock24(), local);
        if (!z.valid)
            continue;
        auto *item = new QListWidgetItem(zoneLine(z), m_world);
        item->setData(kIdRole, id);
        if (id == keep)
            m_world->setCurrentItem(item);
    }
    m_big->setText(localTimeText());
}

bool ClockWindow::addZone(const QString &id)
{
    if (!m_store.addZone(id))
        return false;
    refreshWorld();
    return true;
}

bool ClockWindow::removeZone(const QString &id)
{
    if (!m_store.removeZone(id))
        return false;
    refreshWorld();
    return true;
}

// ---------------------------------------------------------------------------
// Alarms
// ---------------------------------------------------------------------------

QString ClockWindow::addAlarm(const Alarm &alarm)
{
    const QString id = m_store.alarms().add(alarm);
    if (id.isEmpty())
        return {};
    if (!m_store.save())
        statusBar()->showMessage(tr("Could not save: %1").arg(m_store.lastError()), 8000);
    refreshAlarms();
    return id;
}

bool ClockWindow::removeAlarm(const QString &id)
{
    if (!m_store.alarms().remove(id))
        return false;
    m_store.save();
    refreshAlarms();
    return true;
}

bool ClockWindow::setAlarmEnabled(const QString &id, bool enabled)
{
    if (!m_store.alarms().setEnabled(id, enabled))
        return false;
    m_store.save();
    refreshAlarms();
    return true;
}

QStringList ClockWindow::alarmLines() const
{
    QStringList lines;
    for (const Alarm &a : m_store.alarms().all()) {
        const QString time = a.time.toString(clock24() ? QStringLiteral("HH:mm") : QStringLiteral("h:mm AP"));
        QString line = QStringLiteral("%1  %2  %3").arg(time, daysToText(a.days), a.label);
        line = line.trimmed();
        if (!a.enabled)
            line += tr("  (off)");
        lines << line;
    }
    return lines;
}

QString ClockWindow::nextAlarmText() const
{
    const auto next = m_store.alarms().nextAlarm(wallClockNow());
    if (!next.second.isValid())
        return tr("No alarms are on.");
    return tr("Next alarm: %1").arg(QLocale().toString(next.second, QLocale::ShortFormat));
}

void ClockWindow::refreshAlarms()
{
    const QString keep = m_alarmList->currentItem() ? m_alarmList->currentItem()->data(kIdRole).toString() : QString();
    const QSignalBlocker blocker(m_alarmList);
    m_alarmList->clear();
    const QStringList lines = alarmLines();
    const QList<Alarm> &alarms = m_store.alarms().all();
    for (qsizetype i = 0; i < alarms.size(); ++i) {
        auto *item = new QListWidgetItem(lines.at(i), m_alarmList);
        item->setData(kIdRole, alarms.at(i).id);
        if (!alarms.at(i).enabled) {
            QFont f = item->font();
            f.setItalic(true);
            item->setFont(f);
            item->setForeground(palette().color(QPalette::Disabled, QPalette::Text));
        }
        if (alarms.at(i).id == keep)
            m_alarmList->setCurrentItem(item);
    }
    m_nextAlarm->setText(nextAlarmText());
}

void ClockWindow::ring(const QString &what)
{
    m_ringing << what;
    refreshRinging();
    QApplication::beep();
}

void ClockWindow::dismissRinging()
{
    m_ringing.clear();
    refreshRinging();
}

void ClockWindow::refreshRinging()
{
    const bool on = !m_ringing.isEmpty();
    m_banner->setVisible(on);
    m_dismiss->setVisible(on);
    m_banner->setText(m_ringing.join(QStringLiteral("  •  ")));
}

// ---------------------------------------------------------------------------
// Stopwatch
// ---------------------------------------------------------------------------

void ClockWindow::stopwatchStartStop()
{
    const qint64 now = m_time.monotonicMs();
    if (m_stopwatch.isRunning())
        m_stopwatch.stop(now);
    else
        m_stopwatch.start(now);
    refreshStopwatch();
}

void ClockWindow::stopwatchLap()
{
    if (m_stopwatch.lap(m_time.monotonicMs()))
        refreshStopwatch();
}

void ClockWindow::stopwatchReset()
{
    m_stopwatch.reset();
    refreshStopwatch();
}

QString ClockWindow::stopwatchText() const
{
    return formatDuration(m_stopwatch.elapsed(m_time.monotonicMs()), true);
}

void ClockWindow::refreshStopwatch()
{
    m_swLabel->setText(stopwatchText());
    m_swStart->setText(m_stopwatch.isRunning() ? tr("Stop") : tr("Start"));
    m_swLap->setEnabled(m_stopwatch.isRunning());

    const auto &laps = m_stopwatch.laps();
    if (m_swLaps->count() != laps.size()) {
        m_swLaps->clear();
        for (qsizetype i = laps.size(); i > 0; --i) { // newest on top
            const Lap &l = laps.at(i - 1);
            m_swLaps->addItem(tr("Lap %1    %2    (total %3)")
                                  .arg(l.number)
                                  .arg(formatDuration(l.split, true), formatDuration(l.total, true)));
        }
    }
}

// ---------------------------------------------------------------------------
// Timer
// ---------------------------------------------------------------------------

bool ClockWindow::timerSetDuration(qint64 ms)
{
    const bool ok = m_timer.setDuration(ms);
    m_timerWasFinished = false;
    refreshTimer();
    return ok;
}

void ClockWindow::timerStartPause()
{
    const qint64 now = m_time.monotonicMs();
    if (m_timer.isRunning()) {
        m_timer.pause(now);
    } else {
        if (m_timer.remaining(now) == 0)
            m_timer.reset(); // finished before: start over
        m_timer.start(now);
        m_timerWasFinished = false;
    }
    refreshTimer();
}

void ClockWindow::timerReset()
{
    m_timer.reset();
    m_timerWasFinished = false;
    refreshTimer();
}

QString ClockWindow::timerText() const
{
    // Whole seconds, rounded up: a timer with 0.4 s left still shows 0:01.
    return formatDuration(m_timer.remaining(m_time.monotonicMs()) + 999, false);
}

void ClockWindow::refreshTimer()
{
    m_tmLabel->setText(timerText());
    m_tmStart->setText(m_timer.isRunning() ? tr("Pause") : tr("Start"));
    for (QSpinBox *s : {m_tmH, m_tmM, m_tmS})
        s->setEnabled(!m_timer.isRunning());
}

} // namespace colibry::clock
