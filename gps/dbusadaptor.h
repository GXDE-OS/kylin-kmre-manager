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

#define KYLIN_GPSSERVER_PATH "/"
#define KYLIN_GPSSERVER_SERVICE "com.kylin.Kmre.gpsserver"
#define KYLIN_GPSSERVER_INTERFACE "com.kylin.Kmre.gpsserver"

#include <QObject>
#include <QCoreApplication>
#include <QtDBus/QDBusConnection>
#include <QDBusInterface>
#include <QtDBus/QDBusError>
#include <pthread.h>
#include <stdint.h>
#include <QDateTime>
#include <QDebug>
#include "gpsdataget.h"
#include "myutils.h"

class DbusAdaptor : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", KYLIN_GPSSERVER_SERVICE)

public:
    DbusAdaptor(QObject *parent = 0);
    virtual ~DbusAdaptor();

public slots:
    /* 传递gps数据 */
    void passGpsData(QString gpsdata);
    void start();
    void stop();

private:
    void sendData(QString data);
};

#endif
