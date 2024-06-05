/*
 * Copyright (c) KylinSoft Co., Ltd. 2016-2024.All rights reserved.
 *
 * Authors:
 *  Yuan ShanShan    yuanshanshan@kylinos.cn
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

#ifndef DBUSADAPTOR_H
#define DBUSADAPTOR_H

#define KYLIN_SENSOR_PATH "/"
#define KYLIN_SENSOR_SERVICE "com.kylin.Kmre.sensor"
#define KYLIN_SENSOR_INTERFACE "com.kylin.Kmre.sensor"

#include <QObject>
#include <QCoreApplication>
#include <QtDBus/QDBusConnection>
#include <QDBusInterface>
#include <QtDBus/QDBusError>
#include <pthread.h>
#include <stdint.h>
#include <QDateTime>
#include <QDebug>
#include "sensordataget.h"
#include "myutils.h"

class DbusAdaptor : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", KYLIN_SENSOR_SERVICE)

public:
    DbusAdaptor(QObject *parent = 0);
    virtual ~DbusAdaptor();

public slots:
    /* 传递加速度传感器数据 */
    void passAcceKey(QString sensorData);
    void start();
    void stop();

private:
    struct timespec now;
    struct timespec stamp;
    int64_t inter = 100000;
    bool flag = true;
};

#endif
