/*
 * Copyright (c) KylinSoft Co., Ltd. 2016-2024.All rights reserved.
 *
 * Authors:
 *  Alan Xie    xiehuijun@kylinos.cn
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "powermanager.h"
#include <QDBusReply>
#include <QList>
#include <unistd.h>
#include "controlmanager.h"
#include "dbusclient.h"
#include "utils.h"


#define FREEDESKTOP_UPOWER                  "org.freedesktop.DBus.Properties"
#define UPOWER_INTERFACE                    "org.freedesktop.UPower"
#define UPOWER_PATH                         "/org/freedesktop/UPower"
#define UPOWER_SERVICE                      "org.freedesktop.UPower"
#define UPOWER_DEVICE_SERVICE               "org.freedesktop.UPower.Device"


PowerManager::PowerManager()
{
    mBatteryPath = getBatteryPath();
    mBatterySysBus = new QDBusInterface(UPOWER_SERVICE, mBatteryPath, FREEDESKTOP_UPOWER, QDBusConnection::systemBus());

    onUpdateBatteryState();
    QDBusConnection::systemBus().connect(UPOWER_SERVICE, mBatteryPath, FREEDESKTOP_UPOWER, "PropertiesChanged", this, SLOT(onUpdateBatteryState()));

}

QString PowerManager::getBatteryPath()
{
    QString batteryPath = "";
    QDBusInterface dface(UPOWER_SERVICE, UPOWER_PATH, UPOWER_INTERFACE, QDBusConnection::systemBus());
    QDBusReply<QList<QDBusObjectPath>> reply = dface.call("EnumerateDevices");
    if (dface.isValid()) {
        for (QDBusObjectPath op : reply.value()) {
            if (op.path().contains("battery_")) {
                batteryPath = op.path();
                break;
            }
        }
    }
    return batteryPath;
}

void PowerManager::onUpdateBatteryState()
{
    QString boot_completed = kmre::DBusClient::getInstance()->GetPropOfContainer(Utils::getUserName(), getuid(), "sys.kmre.boot_completed");
    if (mBatterySysBus && (boot_completed == "1")) {
        // update power state
        int state = -2;
        QDBusReply<QVariant> reply = mBatterySysBus->call("Get", UPOWER_DEVICE_SERVICE, "State");
        if (reply.isValid()) {
            state = reply.value().toInt();
        }
        //syslog(LOG_DEBUG, "[%s]xxxxx state = %d", __func__, state);
        if (mPowerState != state) {
            mPowerState = state;
            ControlManager::getInstance()->setSystemProp(1, "battery_charging", (mPowerState == 2) ? QString("0") : QString("1"));
        }
        // update battery percent
        double percent = 100.0;
        reply = mBatterySysBus->call("Get", UPOWER_DEVICE_SERVICE, "Percentage");
        if (reply.isValid()) {
            percent = reply.value().toDouble();
        }
        //syslog(LOG_DEBUG, "[%s]xxxxx percent = %f", __func__, percent);
        if (mBatteryPercent != percent) {
            mBatteryPercent = percent;
            ControlManager::getInstance()->setSystemProp(1, "battery_percent", QString::number(mBatteryPercent));
        }
    }
}