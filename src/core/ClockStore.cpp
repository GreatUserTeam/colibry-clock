#include "ClockStore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QTimeZone>

namespace colibry::clock {

ClockStore::ClockStore(QString fileName)
    : m_file(std::move(fileName))
{
}

QString ClockStore::defaultFileName()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/clock.json");
}

bool ClockStore::isValidZone(const QString &id)
{
    if (id.isEmpty() || id.size() > 64)
        return false;
    return QTimeZone::isTimeZoneIdAvailable(id.toUtf8());
}

bool ClockStore::addZone(const QString &id)
{
    if (!isValidZone(id) || m_zones.contains(id))
        return false;
    m_zones.append(id);
    save();
    return true;
}

bool ClockStore::removeZone(const QString &id)
{
    if (!m_zones.removeOne(id))
        return false;
    save();
    return true;
}

void ClockStore::load()
{
    m_alarms = AlarmList();
    m_zones.clear();
    m_warning.clear();

    QFile file(m_file);
    if (!file.exists())
        return;
    if (!file.open(QIODevice::ReadOnly)) {
        m_warning = QCoreApplication::translate("colibry::clock", "The clock file cannot be read: %1").arg(file.errorString());
        return;
    }
    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        const QString backup = m_file + QStringLiteral(".corrupt");
        QFile::remove(backup);
        const bool kept = QFile::rename(m_file, backup);
        m_warning = kept ? QCoreApplication::translate("colibry::clock", "The clock file was damaged. It was saved as \"%1\".").arg(backup)
                         : QCoreApplication::translate("colibry::clock", "The clock file was damaged and could not be read.");
        return;
    }

    int skipped = 0;
    QList<Alarm> alarms;
    QSet<QString> seen;
    const QJsonArray alarmArray = doc.object().value(QStringLiteral("alarms")).toArray();
    for (const QJsonValue &v : alarmArray) {
        const QJsonObject o = v.toObject();
        Alarm a;
        a.id = o.value(QStringLiteral("id")).toString();
        a.time = QTime::fromString(o.value(QStringLiteral("time")).toString(), QStringLiteral("HH:mm"));
        a.label = o.value(QStringLiteral("label")).toString();
        a.enabled = o.value(QStringLiteral("enabled")).toBool(true);
        a.days = o.value(QStringLiteral("days")).toInt(0);
        if (!v.isObject() || a.id.isEmpty() || !a.isValid() || seen.contains(a.id)) {
            ++skipped;
            continue;
        }
        seen.insert(a.id);
        alarms.append(a);
    }
    m_alarms.setAll(alarms);

    const QJsonArray zoneArray = doc.object().value(QStringLiteral("zones")).toArray();
    for (const QJsonValue &v : zoneArray) {
        const QString id = v.toString();
        if (isValidZone(id) && !m_zones.contains(id))
            m_zones.append(id);
        // A zone this computer does not know is dropped silently: tz databases differ between systems.
    }
    if (skipped > 0)
        m_warning = QCoreApplication::translate("colibry::clock", "%n damaged alarms were skipped.", nullptr, skipped);
}

bool ClockStore::save()
{
    m_error.clear();
    if (!QDir().mkpath(QFileInfo(m_file).absolutePath())) {
        m_error = QCoreApplication::translate("colibry::clock", "Cannot create the folder for the clock data.");
        return false;
    }
    QJsonArray alarms;
    for (const Alarm &a : m_alarms.all()) {
        QJsonObject o;
        o.insert(QStringLiteral("id"), a.id);
        o.insert(QStringLiteral("time"), a.time.toString(QStringLiteral("HH:mm")));
        o.insert(QStringLiteral("label"), a.label);
        o.insert(QStringLiteral("enabled"), a.enabled);
        o.insert(QStringLiteral("days"), a.days);
        alarms.append(o);
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("alarms"), alarms);
    root.insert(QStringLiteral("zones"), QJsonArray::fromStringList(m_zones));

    QSaveFile file(m_file);
    if (!file.open(QIODevice::WriteOnly)) {
        m_error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        m_error = file.errorString();
        return false;
    }
    return true;
}

} // namespace colibry::clock
