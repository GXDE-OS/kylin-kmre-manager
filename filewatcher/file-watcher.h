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

#ifndef FILEWATCHER_H
#define FILEWATCHER_H

#include <QObject>
#include <QString>
#include <QFileSystemWatcher>
#include <QStorageInfo>
#include <QHash>
#include <QAtomicInt>
#include <stdint.h>
#include <QList>
#include <QThread>
#include <QMap>
#include <QMutex>

#include "directory-check-worker.h"
#include "storage-check-worker.h"


namespace kmre {

class FileWatcher;




struct LinkDestination
{
    QString destination;
    QString iconPath;
    bool shouldCreate;
};

struct LinkInfo
{
    QString destination;
    QString linkName;
};

struct PendingDirectory
{
    QString dependPath;
    QString relativeLinkPath;
    QString pendingName;
    QString pendingPath;
};

struct DependDirectory
{
    QString targetPath;
    QStringList directories;
    LinkDestination destLink;
};


class FileWatcher : public QObject
{
    Q_OBJECT

public:

    static FileWatcher* GetInstance();
    bool shouldRun();
    void run();

public slots:
    void addWatchPath(const QString& path);
    void pendingAddWatchPath(const QString& path);
    void dependAddWatchPath(const QString& path);
    void linkAddWatchPath(const QString& path);
    void directoryUpdated(const QString &path);
    void pendingDirectoryUpdated(const QString &path);
    void dependDirectoryUpdated(const QString &path);
    void linkDirectoryUpdated(const QString &path);
    void fileUpdated(const QString &path);
    void storageStatusChanged(const QString &path);
    void start();
    void stop();
    void onContainerStopped(const QString &container);

signals:
    void check(const QString& path, int checkType);
    void checkStorage(const QString& path, bool fuseMounted);

private:
    explicit FileWatcher(QObject *parent = 0);
    ~FileWatcher();

private:
    QString containerName();
    QString legacyContainerName();
    void prepareDirectoryAndStorage();
    void makeLink(const QString& dest, const QString& source, const QString& iconPath);
    void makeLink(const QString& dest, const QString& source);
    void dependMakeLink(const QString& path);
    void deleteLink(const QString& dest);
    void dependDeleteLink(const QString& path);

    static FileWatcher * m_pInstance;
    QFileSystemWatcher *m_pSystemWatcher;
    QFileSystemWatcher *m_pPendingWatcher;
    QFileSystemWatcher *m_pDependWatcher;
    QFileSystemWatcher *m_pLinkWatcher;
    QHash<QString, LinkDestination> mHashDirectory;
    QHash<QString, LinkDestination> mHashStorage;
    QHash<QString, PendingDirectory> mHashPending;
    QHash<QString, DependDirectory> mHashDepend;
    QHash<QString, LinkInfo> mHashLink;
    int32_t mUid;
    QString mUserName;
    QString mContainerName;
    QString mLegacyContainerName;
    QAtomicInteger<int> mRunning;
    QMap<QString, QThread*> mThreads;
    QMap<QString, DirectoryCheckWorker*> mDirectoryWorkers;
    QMap<QString, StorageCheckWorker*> mStorageWorkers;
    QMutex mMutex;
};

} // namespace kmre

#endif // FILEWATCHER_H
