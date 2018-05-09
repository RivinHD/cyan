/*
# Copyright (c) 2018, Ole-André Rodlie <ole.andre.rodlie@gmail.com> All rights reserved.
#
# Available under the 3-clause BSD license
# See the LICENSE file for full details
*/

#include "systray.h"
#include <QIcon>
#include <QMenu>
#include <QAction>
#include <QDebug>
#include <QSettings>
#include <QPainter>
#include "common.h"
#include <X11/extensions/scrnsaver.h>

SysTray::SysTray(QObject *parent)
    : QObject(parent)
    , tray(0)
    , man(0)
    , pm(0)
    , wasLowBattery(false)
    , lowBatteryValue(LOW_BATTERY)
    , critBatteryValue(CRITICAL_BATTERY)
    , hasService(false)
    , lidActionBattery(LID_BATTERY_DEFAULT)
    , lidActionAC(LID_AC_DEFAULT)
    , criticalAction(CRITICAL_DEFAULT)
    , autoSleepBattery(AUTO_SLEEP_BATTERY)
    , autoSleepAC(0)
    , timer(0)
    , timeouts(0)
    , showNotifications(true)
{
    // setup tray
    tray = new QSystemTrayIcon(QIcon::fromTheme(DEFAULT_BATTERY_ICON), this);
    connect(tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(trayActivated(QSystemTrayIcon::ActivationReason)));
    if (tray->isSystemTrayAvailable()) { tray->show(); }

    // setup manager
    man = new Manager(this);
    connect(man, SIGNAL(updatedDevices()), this, SLOT(checkDevices()));
    connect(man, SIGNAL(closedLid()), this, SLOT(handleClosedLid()));
    connect(man, SIGNAL(openedLid()), this, SLOT(handleOpenedLid()));
    connect(man, SIGNAL(switchedToBattery()), this, SLOT(handleOnBattery()));
    connect(man, SIGNAL(switchedToAC()), this, SLOT(handleOnAC()));

    // setup org.freedesktop.PowerManagement
    pm = new PowerManagement();
    connect(pm, SIGNAL(HasInhibitChanged(bool)), this, SLOT(handleHasInhibitChanged(bool)));
    connect(pm, SIGNAL(update()), this, SLOT(loadSettings()));

    // setup timer
    timer = new QTimer(this);
    timer->setInterval(60000);
    connect(timer, SIGNAL(timeout()), this, SLOT(timeout()));
    timer->start();

    // load settings and register service
    loadSettings();
    registerService();
    QTimer::singleShot(1000, this, SLOT(checkDevices()));
}

// what to do when user clicks systray, at the moment nothing
void SysTray::trayActivated(QSystemTrayIcon::ActivationReason reason)
{
    Q_UNUSED(reason)
    /*switch(reason) {
    case QSystemTrayIcon::Context:
    case QSystemTrayIcon::Trigger:
        //if (menu->actions().size()>0) { menu->popup(QCursor::pos()); }
    default:;
    }*/
}

void SysTray::checkDevices()
{
    // get battery left and add tooltip
    double batteryLeft = man->batteryLeft();
    tray->setToolTip(tr("Battery at %1%").arg(batteryLeft));
    if (batteryLeft==100) { tray->setToolTip(tr("Charged")); }
    if (!man->onBattery() && batteryLeft<100) { tray->setToolTip(tray->toolTip().append(tr(" (Charging)"))); }

    // draw battery systray
    drawBattery(batteryLeft);

    // critical battery?
    if (batteryLeft<=(double)critBatteryValue && man->onBattery()) { handleCritical(); }

    // Register service if not already registered
    if (!hasService) { registerService(); }
}

// what to do when user open/close lid
void SysTray::handleClosedLid()
{
    int type = lidNone;
    if (man->onBattery()) { type = lidActionBattery; } // on battery
    else { type = lidActionAC; } // on ac
    switch(type) {
    case lidLock:
        man->lockScreen();
        break;
    case lidSleep:
        man->suspend();
        break;
    case lidHibernate:
        man->hibernate();
        break;
    default: ;
    }
}

// do something when lid is opened
void SysTray::handleOpenedLid()
{
    // do nothing
}

// do something when switched to battery power
void SysTray::handleOnBattery()
{
    if (showNotifications) {
        tray->showMessage(tr("On Battery"), tr("Switched to battery power."));
    }
}

// do something when switched to ac power
void SysTray::handleOnAC()
{
    if (showNotifications) {
        tray->showMessage(tr("On AC"), tr("Switched to AC power."));
    }
}

// load default settings
void SysTray::loadSettings()
{
    if (Common::validPowerSettings("autoSleepBattery")) {
        autoSleepBattery = Common::loadPowerSettings("autoSleepBattery").toInt();
    }
    if (Common::validPowerSettings("autoSleepAC")) {
        autoSleepAC = Common::loadPowerSettings("autoSleepAC").toInt();
    }
    if (Common::validPowerSettings("lowBattery")) {
        lowBatteryValue = Common::loadPowerSettings("lowBattery").toInt();
    }
    if (Common::validPowerSettings("criticalBattery")) {
        critBatteryValue = Common::loadPowerSettings("criticalBattery").toInt();
    }
    if (Common::validPowerSettings("lidBattery")) {
        lidActionBattery = Common::loadPowerSettings("lidBattery").toInt();
    }
    if (Common::validPowerSettings("lidAC")) {
        lidActionAC = Common::loadPowerSettings("lidAC").toInt();
    }
    if (Common::validPowerSettings("criticalAction")) {
        criticalAction = Common::loadPowerSettings("criticalAction").toInt();
    }
    qDebug() << "auto sleep battery" << autoSleepBattery;
    qDebug() << "auto sleep ac" << autoSleepAC;
    qDebug() << "low battery setting" << lowBatteryValue;
    qDebug() << "critical battery setting" << critBatteryValue;
    qDebug() << "lid battery" << lidActionBattery;
    qDebug() << "lid ac" << lidActionAC;
    qDebug() << "critical action" << criticalAction;
}

// register session service
void SysTray::registerService()
{
    if (hasService) { return; }
    if (!QDBusConnection::sessionBus().isConnected()) {
        qWarning("Cannot connect to D-Bus.");
        return;
    }
    if (!QDBusConnection::sessionBus().registerService(SERVICE)) {
        qWarning() << QDBusConnection::sessionBus().lastError().message();
        return;
    }
    if (!QDBusConnection::sessionBus().registerObject("/PowerManagement", pm, QDBusConnection::ExportAllContents)) {
        qWarning() << QDBusConnection::sessionBus().lastError().message();
        return;
    }
    hasService = true;
}

// dbus session inhibit status handler
void SysTray::handleHasInhibitChanged(bool has_inhibit)
{
    qDebug() << "HasInhibitChanged?" << has_inhibit;
    if (has_inhibit) { resetTimer(); }
}

// handle critical battery
void SysTray::handleCritical()
{
    qDebug() << "critical battery level, action?" << criticalAction;
    switch(criticalAction) {
    case criticalHibernate:
        man->hibernate();
        break;
    case criticalShutdown:
        qDebug() << "feature not added!"; // TODO!!!!
        break;
    default: ;
    }
}

// draw battery percent over tray icon
void SysTray::drawBattery(double left)
{
    QIcon icon = QIcon::fromTheme(DEFAULT_BATTERY_ICON);
    if (left<=(double)lowBatteryValue && man->onBattery()) {
        icon = QIcon::fromTheme(DEFAULT_BATTERY_ICON_LOW);
        if (!wasLowBattery) { tray->showMessage(tr("Low Battery!"), tr("You battery is almost empty, please consider connecting your computer to a power supply.")); }
        wasLowBattery = true;
    } else { wasLowBattery = false; }

    if (left > 99 || left == 0) {
        tray->setIcon(icon);
        return;
    }

    QPixmap pixmap = icon.pixmap(QSize(24, 24));
    QPainter painter(&pixmap);
    painter.setPen(QColor(Qt::black));
    painter.drawText(pixmap.rect().adjusted(1, 1, 1, 1), Qt::AlignCenter, QString("%1").arg(left));
    painter.setPen(QColor(Qt::white));
    painter.drawText(pixmap.rect(), Qt::AlignCenter, QString("%1").arg(left));
    tray->setIcon(pixmap);
}

// timeout, check if idle
// timeouts and xss must be >= user value and service has to be empty before auto sleep
void SysTray::timeout()
{
    qDebug() << "timeout?" << timeouts;
    qDebug() << "XSS?" << xIdle();
    qDebug() << "inhibit?" << pm->HasInhibit();

    int autoSleep = 0;
    if (man->onBattery()) { autoSleep = autoSleepBattery; }
    else { autoSleep = autoSleepAC; }

    bool doSleep = false;
    if (autoSleep>0 && timeouts>=autoSleep && xIdle()>=autoSleep && !pm->HasInhibit()) { doSleep = true; }
    if (!doSleep) { timeouts++; }
    else {
        timeouts = 0;
        man->suspend();
    }
}

// get user idle time
int SysTray::xIdle()
{
    long idle = 0;
    Display *display = XOpenDisplay(0);
    if (display != 0) {
        XScreenSaverInfo *info = XScreenSaverAllocInfo();
        XScreenSaverQueryInfo(display, DefaultRootWindow(display), info);
        if (info) {
            idle = info->idle;
            XFree(info);
        }
    }
    int hours = idle/(1000*60*60);
    int minutes = (idle-(hours*1000*60*60))/(1000*60);
    return minutes;
}

// reset the idle timer
void SysTray::resetTimer()
{
    timeouts = 0;
}
