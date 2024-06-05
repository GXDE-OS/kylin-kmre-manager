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

#include "file-inotify-watcher.h"
#include "utils.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syslog.h>
#include <sys/poll.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>



namespace kmre {

FileInotifyWatcher::FileInotifyWatcher()
    : mInotifyFd(-1),
      mInitialized(false),
      mLock(QMutex::Recursive),
      mCurrentPosition(0),
      mTotalRead(0)
{
    memset(mCurrentEvent, 0, CURRENT_EVENT_SIZE);
    memset(mEventBuffer, 0, EVENT_BUFFER_SIZE);
}

FileInotifyWatcher::~FileInotifyWatcher()
{
    cleanup();

    if (mInotifyFd != -1) {
        close(mInotifyFd);
    }
}

int FileInotifyWatcher::initialize()
{
    if (mInitialized) {
        return 0;
    }

    mInotifyFd = inotify_init1(IN_CLOEXEC);
    if (mInotifyFd < 0) {
        syslog(LOG_WARNING, "FileInotifyWatcher: Failed to init inotify: %s", strerror(errno));
        return -1;
    }

    mInitialized = true;
    return 0;
}

void FileInotifyWatcher::cleanup()
{
    if (!mInitialized) {
        return;
    }

    {
        QMutexLocker lock(&mLock);
        auto it = mWdToPathMap.begin();
        while (it != mWdToPathMap.end()) {
            inotify_rm_watch(mInotifyFd, it.key());
            it++;
        }

        mWdToPathMap.clear();
        mPathToWdMap.clear();
    }

    close(mInotifyFd);
    mInotifyFd = -1;

    mInitialized = false;
}

int FileInotifyWatcher::addWatch(const QString &path, int mask)
{
    int wd;

    if (!mInitialized) {
        return -1;
    }

    QMutexLocker lock(&mLock);

    auto it = mPathToWdMap.find(path);
    if (it != mPathToWdMap.end()) {
        return 0;
    }

    wd = inotify_add_watch(mInotifyFd, path.toStdString().c_str(), mask);
    if (wd < 0) {
        return -1;
    }

    mWdToPathMap.insert(wd, path);
    mPathToWdMap[path] = wd;

    return 0;
}

int FileInotifyWatcher::removeWatch(const QString &path)
{
    int wd = -1;
    int count = 0;
    int i = 0;
    int ret = 0;

    if (!mInitialized) {
        return -1;
    }

    QMutexLocker lock(&mLock);

    auto pathIter = mPathToWdMap.find(path);
    if (pathIter != mPathToWdMap.end()) {
        wd = pathIter.value();
        pathIter = mPathToWdMap.erase(pathIter);
    }

    if (wd == -1) {
        return -1;
    }

    count = mWdToPathMap.count(wd);
    auto wdIter = mWdToPathMap.find(wd);
    for (i = 0; i < count; i++) {
        if (strcmp(wdIter.value().toStdString().c_str(), path.toStdString().c_str()) == 0) {
            wdIter = mWdToPathMap.erase(wdIter);
        } else {
            wdIter++;
        }
    }

    if (mWdToPathMap.count(wd) == 0) {
        ret = inotify_rm_watch(mInotifyFd, wd);
    }

    return ret;
}

int FileInotifyWatcher::removeWatch(int wd)
{
    QString path;
    int ret = 0;
    int count = 0;
    int i = 0;

    if (!mInitialized) {
        return -1;
    }

    if (wd < 0) {
        return -1;
    }

    QMutexLocker lock(&mLock);

    count = mWdToPathMap.count(wd);
    auto wdIter = mWdToPathMap.find(wd);

    for (i = 0; i < count; i++) {
        path = wdIter.value();
        if (!isPathDir(path.toStdString().c_str())) {
            wdIter = mWdToPathMap.erase(wdIter);
            auto pathIter = mPathToWdMap.find(path);
            if (pathIter != mPathToWdMap.end()) {
                pathIter = mPathToWdMap.erase(pathIter);
            }
        } else {
            wdIter++;
        }
    }

    if (mWdToPathMap.count(wd) == 0) {
        ret = inotify_rm_watch(mInotifyFd, wd);
    }

    return ret;
}

QString FileInotifyWatcher::watchedPathFromWd(int wd)
{
    QString path;
    int count = 0;
    int i = 0;

    if (!mInitialized) {
        return path;
    }

    if (wd < 0) {
        return path;
    }

    QMutexLocker lock(&mLock);

    count = mWdToPathMap.count(wd);
    auto wdIter = mWdToPathMap.find(wd);

    for (i = 0; i < count; i++) {
        QString tmp = wdIter.value();
        if (isPathDir(tmp.toStdString().c_str())) {
            path = tmp;
            break;
        }

        wdIter++;
    }

    return path;
}

int FileInotifyWatcher::wdFromWatchedPath(const QString &path)
{
    int wd = -1;

    if (!mInitialized) {
        return -1;
    }

    QMutexLocker lock(&mLock);

    auto pathIter = mPathToWdMap.find(path);
    if (pathIter != mPathToWdMap.end()) {
        wd = pathIter.value();
    }

    return wd;
}

struct inotify_event* FileInotifyWatcher::readNextEvent(int timeout)
{
    struct pollfd pollfds[1];
    int ret = 0;
    struct inotify_event* e = nullptr;

    if (!mInitialized) {
        return nullptr;
    }

    if (mCurrentPosition == mTotalRead) {
        // no event in buffer now, read new events.
        mTotalRead = 0;
        mCurrentPosition = 0;

        pollfds[0].fd = mInotifyFd;
        pollfds[0].events = POLLIN;

        ret = poll(pollfds, 1, timeout);
        if (ret <= 0) {
            return nullptr;
        }

        mTotalRead = read(mInotifyFd, mEventBuffer, EVENT_BUFFER_SIZE);
        if (mTotalRead < 0) {
            mTotalRead = 0;
            mCurrentPosition = 0;
            return nullptr;
        }
    }

    e = (struct inotify_event*)&mEventBuffer[mCurrentPosition];
    if ((int)(mCurrentPosition + EVENT_SIZE + e->len) > mTotalRead) {
        mCurrentPosition = 0;
        mTotalRead = 0;
        return nullptr;
    }

    memcpy(mCurrentEvent, &mEventBuffer[mCurrentPosition], EVENT_SIZE + e->len);
    mCurrentPosition += (EVENT_SIZE + e->len);

    return (struct inotify_event*) mCurrentEvent;
}

} // namespace kmre
