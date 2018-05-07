/*
# Copyright (c) 2018, Ole-André Rodlie <ole.andre.rodlie@gmail.com> All rights reserved.
#
# Available under the 3-clause BSD license
# See the LICENSE file for full details
*/

#include "manager.h"
#include "upower.h"
#include <QDebug>
#include <QTimer>
#include <QProcess>

#define XSCREENSAVER_LOCK "xscreensaver-command -lock"

Device::Device(const QString block, QObject *parent)
    : QObject(parent)
    , path(block)
    , isRechargable(false)
    , isPresent(false)
    , percentage(0)
    , online(false)
    , hasPowerSupply(false)
    , isBattery(false)
    , isAC(false)
    , capacity(0)
    , energy(0)
    , energyFullDesign(0)
    , energyFull(0)
    , energyEmpty(0)
    , dbus(0)
{
    QDBusConnection system = QDBusConnection::systemBus();
    dbus = new QDBusInterface(DBUS_SERVICE, path, QString("%1.Device").arg(DBUS_SERVICE), system, parent);
    system.connect(dbus->service(), dbus->path(), QString("%1.Device").arg(DBUS_SERVICE), "Changed", this, SLOT(handlePropertiesChanged()));
    if (name.isEmpty()) { name = path.split("/").takeLast(); }
    updateDeviceProperties();
}

void Device::updateDeviceProperties()
{
    if (!dbus->isValid()) { return; }

    capacity =  dbus->property("Capacity").toDouble();
    isRechargable =  dbus->property("IsRechargeable").toBool();
    isPresent =  dbus->property("IsPresent").toBool();
    percentage =  dbus->property("Percentage").toDouble();
    energyFullDesign = dbus->property("EnergyFullDesign").toDouble();
    energyFull = dbus->property("EnergyFull").toDouble();
    energyEmpty = dbus->property("EnergyEmpty").toDouble();
    energy = dbus->property("Energy").toDouble();
    online = dbus->property("Online").toBool();
    hasPowerSupply = dbus->property("PowerSupply").toBool();

    uint type = dbus->property("Type").toUInt();
    if (type == 2) { isBattery = true; }
    else {
        isBattery = false;
        if (type == 1) { isAC = true; } else { isAC = false; }
    }

    vendor = dbus->property("Vendor").toString();
    nativePath = dbus->property("NativePath").toString();

    emit deviceChanged(path);
}

void Device::handlePropertiesChanged()
{
    updateDeviceProperties();
}

Manager::Manager(QObject *parent)
    : QObject(parent)
    , dbus(0)
    , wasDocked(false)
    , wasLidClosed(false)
    , wasOnBattery(false)
{
    setupDBus();
    timer.setInterval(60000);
    connect(&timer, SIGNAL(timeout()), this, SLOT(checkUPower()));
    timer.start();
}

bool Manager::isDocked()
{
    if (dbus->isValid()) { return dbus->property("IsDocked").toBool(); }
    return false;
}

bool Manager::lidIsPresent()
{
    if (dbus->isValid()) { return dbus->property("LidIsPresent").toBool(); }
    return false;
}

bool Manager::lidIsClosed()
{
    if (dbus->isValid()) { return dbus->property("LidIsClosed").toBool(); }
    return false;
}

/*bool Manager::onLowBattery()
{
    // TODO: setting
    if (onBattery() && batteryLeft()<=15) { return true; }
    //if (dbus->isValid()) { return dbus->property("OnLowBattery").toBool(); }
    return false;
}*/

bool Manager::onBattery()
{
    if (dbus->isValid()) { return dbus->property("OnBattery").toBool(); }
    return false;
}

bool Manager::canHibernate()
{
    if (dbus->isValid()) { return dbus->property("CanHibernate").toBool(); }
    return false;
}

bool Manager::canSuspend()
{
    if (dbus->isValid()) { return dbus->property("CanSuspend").toBool(); }
    return false;
}

double Manager::batteryLeft()
{
    double batteryLeft = 0;
    QMapIterator<QString, Device*> device(devices);
    while (device.hasNext()) {
        device.next();
        if (device.value()->isBattery && device.value()->isPresent) {
            batteryLeft += device.value()->percentage;
        } else { continue; }
    }
    return batteryLeft;
}

void Manager::suspend()
{
    if (canSuspend()) { UPower::suspend(); }
}

void Manager::hibernate()
{
    if (canHibernate()) { UPower::hibernate(); }
}

void Manager::lockScreen()
{
    QProcess::startDetached(XSCREENSAVER_LOCK);
}

void Manager::setupDBus()
{
    QDBusConnection system = QDBusConnection::systemBus();
    if (system.isConnected()) {
        system.connect(DBUS_SERVICE, DBUS_PATH, DBUS_SERVICE, DBUS_DEVICE_ADDED, this, SLOT(deviceAdded(const QDBusObjectPath&)));
        system.connect(DBUS_SERVICE, DBUS_PATH, DBUS_SERVICE, DBUS_DEVICE_REMOVED, this, SLOT(deviceRemoved(const QDBusObjectPath&)));
        system.connect(DBUS_SERVICE, DBUS_PATH, DBUS_SERVICE, "Changed", this, SLOT(deviceChanged()));
        system.connect(DBUS_SERVICE, DBUS_PATH, DBUS_SERVICE, "DeviceChanged", this, SLOT(deviceChanged()));
        system.connect(DBUS_SERVICE, DBUS_PATH, DBUS_SERVICE, "NotifyResume", this, SLOT(notifyResume()));
        system.connect(DBUS_SERVICE, DBUS_PATH, DBUS_SERVICE, "NotifySleep", this, SLOT(notifySleep()));
        if (dbus==NULL) { dbus = new QDBusInterface(DBUS_SERVICE, DBUS_PATH, DBUS_SERVICE, system); }
        scanDevices();
    }
}

void Manager::scanDevices()
{
    QStringList foundDevices = UPower::getDevices();
    for (int i=0;i<foundDevices.size();i++) {
        QString foundDevicePath = foundDevices.at(i);
        bool hasDevice = devices.contains(foundDevicePath);
        if (hasDevice) { continue; }
        Device *newDevice = new Device(foundDevicePath, this);
        //connect(newDevice, SIGNAL(deviceChanged(QString)), this, SLOT(handleDeviceChanged(QString)));
        devices[foundDevicePath] = newDevice;
    }
    emit updatedDevices();
}

void Manager::deviceAdded(const QDBusObjectPath &obj)
{
    qDebug() << "device added?";
    if (!dbus->isValid()) { return; }
    QString path = obj.path();
    if (path.startsWith(QString("%1/jobs").arg(DBUS_PATH))) { return; }

    scanDevices();
}

void Manager::deviceRemoved(const QDBusObjectPath &obj)
{
    qDebug() << "device removed?";
    if (!dbus->isValid()) { return; }
    QString path = obj.path();
    bool deviceExists = devices.contains(path);
    if (path.startsWith(QString("%1/jobs").arg(DBUS_PATH))) { return; }

    if (deviceExists) {
        if (UPower::getDevices().contains(path)) { return; }
        delete devices.take(path);
    }
    scanDevices();
}

void Manager::deviceChanged()
{
    qDebug() << "changed";

    if (wasDocked != isDocked()) {
        // TODO: untested
        qDebug() << "docked status changed";
    }
    wasDocked = isDocked();
    qDebug() << "is docked?" << wasDocked;

    if (wasLidClosed != lidIsClosed()) {
        if (!wasLidClosed && lidIsClosed()) {
            emit closedLid();
        } else if (wasLidClosed && !lidIsClosed()) {
            emit openedLid();
        }
    }
    wasLidClosed = lidIsClosed();

    if (wasOnBattery != onBattery()) {
        if (!wasOnBattery && onBattery()) {
            emit switchedToBattery();
        } else if (wasOnBattery && !onBattery()) {
            emit switchedToAC();
        }
    }
    wasOnBattery = onBattery();

    qDebug() << "battery left" << batteryLeft();

    //emit lowBattery(onLowBattery());
    emit updatedDevices();
}

void Manager::handleDeviceChanged(QString devicePath)
{
    Q_UNUSED(devicePath)
    /*if (devicePath.isEmpty()) { return; }
    emit updatedDevices();*/
}

void Manager::checkUPower()
{
    if (!QDBusConnection::systemBus().isConnected()) {
        setupDBus();
        return;
    }
    if (!dbus->isValid()) { scanDevices(); }
}

void Manager::notifyResume()
{
    qDebug() << "system is about to resume ...";
}

void Manager::notifySleep()
{
    qDebug() << "system is about to sleep ...";
    lockScreen();
}
