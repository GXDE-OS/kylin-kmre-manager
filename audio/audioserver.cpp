/*
 * Copyright (c) KylinSoft Co., Ltd. 2016-2024.All rights reserved.
 *
 * Authors:
 *  Kobe Lee    lixiang@kylinos.cn
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

#include <QDebug>
#include <sys/syslog.h>
#include <QDBusConnection>

#include "audioserver.h"
#include "playbackworker.h"
#include "recordingworker.h"
#include "threadpool.h"

AudioServer::AudioServer(QObject *parent)
    : QObject(parent)
{

}

AudioServer::~AudioServer()
{

}

void AudioServer::onInit()
{
    m_playWorker = new PlaybackWorker();
    mPlayWorkerThread = ThreadPool::instance()->newThread();
    m_playWorker->moveToThread(mPlayWorkerThread);
    connect(mPlayWorkerThread, SIGNAL(started()), m_playWorker, SLOT(initData()));
    //connect(mPlayWorkerThread, &QThread::finished, mPlayWorkerThread, &QThread::deleteLater);
    mPlayWorkerThread->start();


    m_recordWorker = new RecordingWorker();
    QThread *thread2 = ThreadPool::instance()->newThread();
    m_recordWorker->moveToThread(thread2);
    connect(thread2, SIGNAL(started()), m_recordWorker, SLOT(initData()));
    thread2->start();

#ifdef UKUI_WAYLAND
    QDBusConnection::systemBus().connect(QString("org.freedesktop.login1"),
                                         QString("/org/freedesktop/login1"),
                                         QString("org.freedesktop.login1.Manager"),
                                         QString("PrepareForSleep"),
					 this,
					 SLOT(onSleep(bool)));
#endif
}

void AudioServer::onSleep(bool sleep)
{
    syslog(LOG_INFO, "[%s] sleep = %d, ", __func__, sleep);
    if (!sleep) {// wake form sleep
        if (m_playWorker) {
            m_playWorker->exitPlayback();
            ThreadPool::instance()->quitThread(mPlayWorkerThread);
            mPlayWorkerThread->wait();
            delete m_playWorker;
            delete mPlayWorkerThread;
        }
        syslog(LOG_DEBUG, "[%s] re-init playback ...", __func__);
        m_playWorker = new PlaybackWorker();
        mPlayWorkerThread = ThreadPool::instance()->newThread();
        m_playWorker->moveToThread(mPlayWorkerThread);
        connect(mPlayWorkerThread, SIGNAL(started()), m_playWorker, SLOT(initData()));
        mPlayWorkerThread->start();
        syslog(LOG_DEBUG, "[%s] re-init playback finished.", __func__);
    }
}


