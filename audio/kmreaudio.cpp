/*
 * Copyright (c) KylinSoft Co., Ltd. 2016-2024.All rights reserved.
 *
 * Authors:
 *  Kobe Lee    lixiang@kylinos.cn
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

#include "kmreaudio.h"

#include <QCoreApplication>
#include <QDebug>
#include <QtDBus/QDBusConnection>

#include "threadpool.h"
#include "audioserver.h"
#include "utils.h"

KmreAudio::KmreAudio(QObject * parent)
    : QObject(parent)
{

}

KmreAudio::~KmreAudio() {
    if (m_server) {
        m_server->deleteLater();
    }

    ThreadPool::instance()->deleteLater();
}

void KmreAudio::init()
{
    m_server = new AudioServer;
    QThread *thread = ThreadPool::instance()->newThread();
    m_server->moveToThread(thread);
    connect(thread, SIGNAL(started()), m_server, SLOT(onInit()));
    thread->start();

    QDBusConnection::systemBus().connect(QString("cn.kylinos.Kmre"),
                                             QString("/cn/kylinos/Kmre"),
                                             QString("cn.kylinos.Kmre"),
                                             QString("Stopped"), this, SLOT(onStopApplication(QString)));
}

void KmreAudio::start()
{
    // do nothing
}

void KmreAudio::stop()
{
    qApp->quit();
}

void KmreAudio::onStopApplication(const QString &container)
{
    QString name = QString("kmre-%1-%2").arg(Utils::getUid()).arg(Utils::getUserName());
    if (name == container) {
        if (m_server) {
            m_server->deleteLater();
        }

        ThreadPool::instance()->deleteLater();

        exit(0);
    }
}
