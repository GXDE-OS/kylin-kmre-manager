/*
 * Copyright (c) KylinSoft Co., Ltd. 2016-2024.All rights reserved.
 *
 * Authors:
 *  Ma Chao    machao@kylinos.cn
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

#ifndef FILEINOTIFYSERVICE_H
#define FILEINOTIFYSERVICE_H

#include <thread>
#include <atomic>
#include <set>
#include <string>

#include <sys/types.h>

#include <QMutex>
#include <QMimeDatabase>
#include <QList>

#include "file-inotify-watcher.h"
#include "utils.h"


namespace kmre {

class DBusClient;

struct NotifyFileInfo
{
    QString path;
    int size;
};

class FileInotifyService
{
public:
    int initialize();
    void start();

    int addWatch(const QString& path);
    int addWatchRecursively(const QString& path, bool shouldNotifyFile = false);

    static FileInotifyService* getInstance();

private:
    FileInotifyService();
    ~FileInotifyService();

    void addNotifyFile(const QString& path);
    void addNotifyFileLocked(const QString& path);

    bool notifyFile(const NotifyFileInfo& info);

    void stop();
    void wait();
    void run();
    void loopList();

    int addWatchLocked(const QString& path);
    void watchAndNotifyDirectoryRecursivelyLocked(const QString& path, bool shouldNotifyFile);

    static FileInotifyService* m_pInstance;

    class Garbo
    {
    public:
        ~Garbo()
        {
            if (FileInotifyService::m_pInstance) {
                delete FileInotifyService::m_pInstance;
                FileInotifyService::m_pInstance = nullptr;
            }
        }
    };

    static Garbo garbo;
    static QMutex lock;

    QMutex mWatcherLock;
    QMutex mListLock;
    std::atomic<bool> mInitialized;
    std::atomic<bool> mIsRunning;
    std::atomic<bool> mIsThreadRunning;
    std::thread mWatcherThread;
    std::thread mListThread;
    bool mStopWatcher;
    bool mStopList;
    FileInotifyWatcher mWatcher;
    QMimeDatabase mMimeDB;
    DBusClient* mDBusClient;
    QList<NotifyFileInfo> mFileList;

    DISALLOW_COPY_AND_ASSIGN(FileInotifyService);
};

} // namespace kmre

#endif // FILEINOTIFYSERVICE_H
