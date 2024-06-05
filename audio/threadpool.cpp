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

#include "threadpool.h"

#include <QDebug>

ThreadPool::ThreadPool(QObject *parent) : QObject(parent)
{

}

ThreadPool::~ThreadPool()
{
    quitAll();
}

ThreadPool* ThreadPool::instance()
{
    static ThreadPool threadPool;
    return &threadPool;
}

QThread *ThreadPool::newThread()
{
    QThread *thread = new QThread;
    m_pool.push_back(thread);
    return thread;
}

void ThreadPool::quitThread(QThread *thread)
{
    int index = m_pool.indexOf(thread);
    if (index >= 0) {
        m_pool.takeAt(index)->quit();
    }
}

void ThreadPool::quitAll()
{
    foreach (QThread *thread, m_pool) {
        //qDebug() << thread;
        thread->quit();
        //thread->wait(2000);
    }
}
