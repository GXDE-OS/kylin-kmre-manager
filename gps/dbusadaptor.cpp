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

#include "dbusadaptor.h"

DbusAdaptor::DbusAdaptor(QObject *parent) : QObject(parent)
{
    QDBusConnection connection = QDBusConnection::sessionBus();
    if (!connection.registerService(KYLIN_GPSSERVER_SERVICE)) {
        return;
    }
    connection.registerObject(KYLIN_GPSSERVER_PATH, this, QDBusConnection::ExportAllSlots);
}

DbusAdaptor::~DbusAdaptor() {}

/* 传递虚拟定位的数据 */
void DbusAdaptor::passGpsData(QString gpsdata)
{
    GpsdataGet::getInstance()->gpsdata = gpsdata;
    sendData(gpsdata);
}

void DbusAdaptor::sendData(QString data)
{
    GpsdataGet::getInstance()->sendData(data);
}

void DbusAdaptor::start()
{
}

void DbusAdaptor::stop()
{
    exit(0);
}
