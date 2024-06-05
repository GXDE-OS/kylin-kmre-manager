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

#include "file-inotify-service.h"
#include "dbus-client.h"

#include <QMutexLocker>
#include <QStandardPaths>

#include <unistd.h>
#include <sys/inotify.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>


#include <QDebug>

namespace kmre {

static QString homeDirPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);

FileInotifyService::Garbo FileInotifyService::garbo;
QMutex FileInotifyService::lock(QMutex::Recursive);
FileInotifyService* FileInotifyService::m_pInstance = nullptr;

FileInotifyService* FileInotifyService::getInstance()
{
    if (nullptr == m_pInstance) {
        QMutexLocker _l(&lock);
        if (nullptr == m_pInstance) {
            m_pInstance = new FileInotifyService;
        }
    }

    return m_pInstance;
}

FileInotifyService::FileInotifyService()
    : mWatcherLock(QMutex::Recursive),
      mListLock(QMutex::Recursive),
      mInitialized(false),
      mIsRunning(false),
      mIsThreadRunning(false),
      mStopWatcher(false),
      mStopList(false),
      mDBusClient(nullptr)
{
    mDBusClient = new DBusClient;
}

FileInotifyService::~FileInotifyService()
{
    stop();
    wait();

    mWatcher.cleanup();

    delete mDBusClient;
}

void FileInotifyService::wait()
{
    if (mListThread.joinable()) {
        mListThread.join();
    }

    if (mWatcherThread.joinable()) {
        mWatcherThread.join();
    }
}

void FileInotifyService::stop()
{
    mStopList = true;
    mStopWatcher = true;
}

void FileInotifyService::start()
{
    if (!mInitialized) {
        return;
    }

    if (mIsRunning) {
        return;
    }

    mIsRunning = true;
    mStopList = false;
    mStopWatcher = false;

    mListThread = std::thread(&FileInotifyService::loopList, this);
    mWatcherThread = std::thread(&FileInotifyService::run, this);

    while (!mIsThreadRunning) {
        usleep(50 * 1000);
    }
}

int FileInotifyService::initialize()
{
    if (mInitialized) {
        return 0;
    }

    if (mWatcher.initialize() < 0) {
        return -1;
    }

    mInitialized = true;

    return 0;
}

int FileInotifyService::addWatch(const QString &path)
{
    if (!mInitialized) {
        return -1;
    }

    QMutexLocker _l(&mWatcherLock);
    return addWatchLocked(path);
}

int FileInotifyService::addWatchLocked(const QString &path)
{
    if (mWatcher.addWatch(path, IN_CREATE | IN_MOVED_TO | IN_MOVE_SELF | IN_DELETE_SELF | IN_EXCL_UNLINK | IN_DONT_FOLLOW) < 0) {
        return -1;
    }

    return 0;
}

int FileInotifyService::addWatchRecursively(const QString &path, bool shouldNotifyFile)
{
    if (!mInitialized) {
        return -1;
    }

    if (!isPathDir(path.toStdString().c_str())) {
        return -1;
    }

    QMutexLocker _l(&mWatcherLock);
    watchAndNotifyDirectoryRecursivelyLocked(path.toStdString().c_str(), shouldNotifyFile);

    return 0;
}

void FileInotifyService::watchAndNotifyDirectoryRecursivelyLocked(const QString& path, bool shouldNotifyFile)
{
    struct dirent* entry = nullptr;
    DIR* dp = nullptr;

    char resolvedPath[PATH_MAX] = {0};

    if (!realpath(path.toStdString().c_str(), resolvedPath)) {
        return;
    }

    addWatchLocked(resolvedPath);

    dp = opendir(resolvedPath);
    if (!dp) {
        return;
    }

    while ((entry = readdir(dp)) != nullptr) {
        char filePath[PATH_MAX + NAME_MAX + 1] = {0};
        if (strlen(entry->d_name) == 0) {
            continue;
        }

        if (entry->d_name[0] == '.') {
            continue;
        }

        snprintf(filePath, PATH_MAX + NAME_MAX + 1, "%s/%s", resolvedPath, entry->d_name);
        if (isPathRegularFile(filePath) && shouldNotifyFile) {
            addNotifyFileLocked(filePath);
        } else if (isPathDir(filePath)) {
            watchAndNotifyDirectoryRecursivelyLocked(filePath, shouldNotifyFile);
        }
    }

    closedir(dp);

}

void FileInotifyService::run()
{
    struct inotify_event* event = nullptr;

    if (!mInitialized) {
        goto out;
    }

    mIsThreadRunning = true;

    while (!mStopWatcher) {
        event = mWatcher.readNextEvent(1000);
        if (event != nullptr) {
            if ((event->mask & IN_CREATE) ||
                (event->mask & IN_MOVED_TO)) {
                if (event->len > 0) {
                    QString newPath;
                    QString pathName = mWatcher.watchedPathFromWd(event->wd);
                    if (pathName.isNull() || pathName.isEmpty()) {
                        continue;
                    }

                    newPath = pathName + "/" + QString(event->name);
                    if (isPathDir(newPath.toStdString().c_str())) {
                        addWatchRecursively(newPath, true);
                    } else if (isPathRegularFile(newPath.toStdString().c_str())) {
                        addNotifyFile(newPath);
                    }
                }
            } else if ((event->mask & IN_MOVE_SELF) ||
                       (event->mask & IN_DELETE_SELF)) {
                mWatcher.removeWatch(event->wd);
            }
        }
    }

out:
    mIsThreadRunning = false;
    mIsRunning = false;
}

void FileInotifyService::addNotifyFile(const QString &path)
{
    QMutexLocker _l(&mWatcherLock);
    addNotifyFileLocked(path);
}

void FileInotifyService::addNotifyFileLocked(const QString& path)
{

    QMimeType mimeType = mMimeDB.mimeTypeForFile(path, QMimeDatabase::MatchExtension);
    QString mimeTypeName = mimeType.name();

    if (path.startsWith(homeDirPath)) {
        if (mimeTypeName.startsWith("image/") || mimeTypeName.startsWith("video/")) {
            QMutexLocker _l(&mListLock);
            mFileList.append( { path, -1 } );
        }
    }
}

bool FileInotifyService::notifyFile(const NotifyFileInfo& info)
{
    if (mDBusClient->queryService()) {
        QString path = info.path;
        QMimeType mimeType = mMimeDB.mimeTypeForFile(path, QMimeDatabase::MatchContent);
        QString mimeTypeName = mimeType.name();
        if (mimeTypeName.startsWith("image/") || mimeTypeName.startsWith("video/")) {
            path.replace(0, homeDirPath.length(), "/storage/emulated/0/0-麒麟文件/");
            mDBusClient->notifyFile(path, mimeTypeName);
        }

        return true;
    }

    return false;
}

void FileInotifyService::loopList()
{
    struct stat sb;

    while (!mStopList) {
        usleep(100 * 1000);

        {
            QMutexLocker _l(&mListLock);
            QMutableListIterator<NotifyFileInfo> i(mFileList);
            while (i.hasNext()) {
                NotifyFileInfo& info = i.next();

                if (stat(info.path.toStdString().c_str(), &sb) != 0) {
                    i.remove();
                    continue;
                }

                if (sb.st_size != info.size) {
                    info.size = sb.st_size;
                    continue;
                }

                if (notifyFile(info)) {
                    i.remove();
                    continue;
                }
            }
        }
    }
}

} // namespace kmre
