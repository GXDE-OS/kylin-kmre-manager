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
    clock_gettime(CLOCK_MONOTONIC, &stamp);
    QDBusConnection connection = QDBusConnection::sessionBus();
    if (!connection.registerService(KYLIN_SENSOR_SERVICE)) {
        return;
    }
    connection.registerObject(KYLIN_SENSOR_PATH, this, QDBusConnection::ExportAllSlots);
}

DbusAdaptor::~DbusAdaptor() {}

/* 传递加速度传感器数据 */
void DbusAdaptor::passAcceKey(QString sensorData)
{
    if (sensorData == ""){
        if (flag){
            qDebug()<<"-8.:3.0:6.6";
            sensorData = "-8.:3.0:6.6";
            flag = false;
        }else {
            qDebug()<<"2.9:-4.:2.6";
            sensorData = "2.9:-4.:2.6";
            flag = true;
        }
    }
    qDebug()<<"发送数据";
    clock_gettime(CLOCK_MONOTONIC, &now);
    int64_t duration = time_diff_us(now, stamp);
    if (duration > inter) {
        SensordataGet::getInstance()->sendData(sensorData);
    }
    stamp = now;
    qDebug()<<"sensorData:"<<sensorData;
}

/* 传递加速度传感器数据 */
//void DbusAdaptor::passAcceKey()
//{
//    clock_gettime(CLOCK_MONOTONIC, &now);
//    int64_t duration = time_diff_us(now, stamp);
//    if (duration > inter) {
//        SensordataGet::getInstance()->sendData();
//    }
//    stamp = now;
//}

void DbusAdaptor::start()
{
}

void DbusAdaptor::stop()
{
    exit(0);
}