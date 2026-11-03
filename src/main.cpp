#include "ClockWindow.h"
#include "colibry/SettingsStore.h"
#include "colibry/ThemeSync.h"

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("colibry-clock"));
    QApplication::setApplicationDisplayName(QStringLiteral("Clock"));
    QApplication::setApplicationVersion(QStringLiteral(Colibry_APPS_VERSION));
    QApplication::setOrganizationName(QStringLiteral("GreatUserTeam"));
    QApplication::setDesktopFileName(QStringLiteral("colibry-clock"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("World clock, alarms, stopwatch and timer for the Great User System"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    colibry::SettingsStore settings;
    colibry::ThemeSync themeSync(app, settings);

    colibry::clock::SystemTimeSource time;
    colibry::clock::ClockStore store(colibry::clock::ClockStore::defaultFileName());
    store.load();
    colibry::clock::ClockWindow window(time, store, settings);
    window.show();
    return QApplication::exec();
}
