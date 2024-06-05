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

#include <QDebug>
#include <QtDBus/QDBusConnection>
#include "kmregps.h"
#include "threadpool.h"
#include "utils.h"

KmreGps::KmreGps(QObject *parent) : QObject(parent) {}

KmreGps::~KmreGps()
{
    if (m_gpsdataget) {
        m_gpsdataget->deleteLater();
    }
    ThreadPool::instance()->deleteLater();
}

void KmreGps::init()
{
    m_gpsdataget = new GpsdataGet;
    m_gpsdataget = GpsdataGet::getInstance();
    QThread *thread = ThreadPool::instance()->newThread();
    m_gpsdataget->moveToThread(thread);
    QObject::connect(thread, SIGNAL(started()), m_gpsdataget, SLOT(initData()));
    thread->start();
    QDBusConnection::systemBus().connect(QString("cn.kylinos.Kmre"), QString("/cn/kylinos/Kmre"),
                                         QString("cn.kylinos.Kmre"), QString("Stopped"), this,
                                         SLOT(onStopApplication(QString)));
}

void KmreGps::onStopApplication(const QString &container)
{
    QString name = QString("kmre-%1-%2").arg(Utils::getUid()).arg(Utils::getUserName());
    if (name == container) {
        ThreadPool::instance()->deleteLater();
        exit(0);
    }
}
