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

#ifndef FILEINOTIFYWATCHER_H
#define FILEINOTIFYWATCHER_H

#include <atomic>

#include <sys/inotify.h>

#include <QMutex>
#include <QMap>
#include <QMultiMap>

#ifndef NAME_MAX
#define NAME_MAX 255
#endif

#define EVENT_SIZE          (sizeof(struct inotify_event))
#define CURRENT_EVENT_SIZE  (16 * (EVENT_SIZE + NAME_MAX + 1))
#define EVENT_BUFFER_SIZE   (64 * (EVENT_SIZE + NAME_MAX + 1))

namespace kmre {

class FileInotifyWatcher
{
public:
    FileInotifyWatcher();
    ~FileInotifyWatcher();

    int initialize();
    void cleanup();

    int addWatch(const QString& path, int mask);
    int removeWatch(const QString& path);
    int removeWatch(int wd);

    QString watchedPathFromWd(int wd);
    int wdFromWatchedPath(const QString& path);

    struct inotify_event* readNextEvent(int timeout = 1000);

private:
    QMultiMap<int, QString> mWdToPathMap;
    QMap<QString, int> mPathToWdMap;
    int mInotifyFd;
    std::atomic<bool> mInitialized;

    QMutex mLock;

    int mCurrentPosition;
    int mTotalRead;
    unsigned char mCurrentEvent[CURRENT_EVENT_SIZE];
    unsigned char mEventBuffer[EVENT_BUFFER_SIZE];
};

} // namespace kmre

#endif // FILEINOTIFYWATCHER_H
