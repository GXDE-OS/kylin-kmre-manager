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

#include <QStandardPaths>
#include <QDebug>
#include <QFileInfo>
#include <QHashIterator>
#include <QThread>
#include <QMutexLocker>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonParseError>
#include <QJsonArray>
#include <QCoreApplication>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QTimer>

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/syslog.h>

#include "file-watcher.h"
#include "custom.h"

namespace kmre {

static const QString kydroidDataPrefix = "/var/lib/kydroid/data";
static const QString kmreDataPrefix = "/var/lib/kmre/data";

FileWatcher* FileWatcher::m_pInstance = nullptr;

QString FileWatcher::legacyContainerName()
{
    return QString("kydroid-") + QString::number(mUid) + "-" + mUserName;
}

QString FileWatcher::containerName()
{
    return QString("kmre-") + QString::number(mUid) + "-" + mUserName;
}

const QString& getUserName()
{
    static QString userName = "";

    if (!userName.isEmpty()) {
        return userName;
    }

    struct passwd  pwd;
    struct passwd* result = 0;
    char buf[1024];

    memset(&buf, 0, sizeof(buf));
    uint32_t uid = getuid();
    (void)getpwuid_r(uid, &pwd, buf, 1024, &result);

    if (result && pwd.pw_name) {
        userName = pwd.pw_name;
    }
    else {
        try {
            userName = std::getenv("USER");
        } 
        catch (...) {
            try {
                userName = std::getenv("USERNAME");
            }
            catch (...) {
                syslog(LOG_ERR, "[%s] Get user name failed!", __func__);
                char name[16] = {0};
                snprintf(name, sizeof(name), "%u", getuid());
                userName = name;
            }
        }
    }

    userName.replace('\\', "_");// 在某些云环境，用户名中会有'\'字符，需将该字符转换为'_'字符
    return userName;
}

FileWatcher::FileWatcher(QObject *parent)
    : QObject(parent),
      m_pSystemWatcher(nullptr),
      mRunning(0),
      mMutex(QMutex::Recursive)
{
    mUid = getuid();
    mUserName = getUserName();
    if (mUserName.isEmpty() || mUserName.isNull()) {
        fprintf(stderr, "User name doesn't match uid\n");
        syslog(LOG_ERR, "User name doesn't match uid.");
        return;
    }

    mContainerName = containerName();
    mLegacyContainerName = legacyContainerName();

    QDBusConnection::systemBus().connect(QString("cn.kylinos.Kmre"),
                                             QString("/cn/kylinos/Kmre"),
                                             QString("cn.kylinos.Kmre"),
                                             QString("Stopped"), this, SLOT(onContainerStopped(QString)));

    prepareDirectoryAndStorage();
}

FileWatcher::~FileWatcher()
{
    QMutexLocker lock(&mMutex);
    QMap<QString, DirectoryCheckWorker*>::iterator directoryWorkerIter;
    QMap<QString, StorageCheckWorker*>::iterator storageWorkerIter;
    QMap<QString, QThread*>::iterator threadIter;

    for (directoryWorkerIter = mDirectoryWorkers.begin(); directoryWorkerIter != mDirectoryWorkers.end();) {
        directoryWorkerIter.value()->deleteLater();
        directoryWorkerIter++;
    }
    mDirectoryWorkers.clear();

    for (storageWorkerIter = mStorageWorkers.begin(); storageWorkerIter != mStorageWorkers.end();) {
        storageWorkerIter.value()->deleteLater();
        storageWorkerIter++;
    }
    mStorageWorkers.clear();

    for (threadIter = mThreads.begin(); threadIter != mThreads.end();) {
        threadIter.value()->quit();
        threadIter.value()->deleteLater();
        threadIter++;
    }
    mThreads.clear();

    if (m_pSystemWatcher) {
        if (m_pSystemWatcher->files().size() > 0) {
            m_pSystemWatcher->removePaths(m_pSystemWatcher->files());
        }
        if (m_pSystemWatcher->directories().size() > 0) {
            m_pSystemWatcher->removePaths(m_pSystemWatcher->directories());
        }
        m_pSystemWatcher->deleteLater();
    }

    if (m_pPendingWatcher) {
        if (m_pPendingWatcher->files().size() > 0) {
            m_pPendingWatcher->removePaths(m_pPendingWatcher->files());
        }
        if (m_pPendingWatcher->directories().size() > 0) {
            m_pPendingWatcher->removePaths(m_pPendingWatcher->directories());
        }
        m_pPendingWatcher->deleteLater();
    }

    if (m_pDependWatcher) {
        if (m_pDependWatcher->files().size() > 0) {
            m_pDependWatcher->removePaths(m_pDependWatcher->files());
        }
        if (m_pDependWatcher->directories().size() > 0) {
            m_pDependWatcher->removePaths(m_pDependWatcher->directories());
        }
        m_pDependWatcher->deleteLater();
    }

    if (m_pLinkWatcher) {
        if (m_pLinkWatcher->files().size() > 0) {
            m_pLinkWatcher->removePaths(m_pLinkWatcher->files());
        }
        if (m_pLinkWatcher->directories().size() > 0) {
            m_pLinkWatcher->removePaths(m_pLinkWatcher->directories());
        }
        m_pLinkWatcher->deleteLater();
    }
}

void FileWatcher::prepareDirectoryAndStorage()
{
    QString legacyContainerDataPath = kydroidDataPrefix + "/" + mLegacyContainerName;
    QString containerDataPath = kmreDataPrefix + "/" + mContainerName;
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);


    QDir desktopDir(desktopPath);

    QString wxSourcePath = containerDataPath + "/tencent/MicroMsg";
    QString legacyWxSourcePath = legacyContainerDataPath + "/tencent/MicroMsg";
    QString wxDestPath = desktopPath + "/" + tr("Wechat_Data");
    QString wxIconPath = "/usr/share/kmre/icons/wechat.png";

    QString dataSourcePath = containerDataPath;
    QString legacyDataSourcePath = legacyContainerDataPath;
    QString dataDestPath = desktopPath + "/" + tr("Mobile_App_Data");
    QString dataIconPath = "/usr/share/kmre/icons/mobile.png";

    //QString oldDataDestPath = desktopPath + "/" + tr("Android_Data");

    QString TencentPath = containerDataPath + "/Tencent";
    QString tencentPath = containerDataPath + "/tencent";
    QString qqDependPath = tencentPath + "/MobileQQ";
    QString qqSourcePath = containerDataPath + "/tencent/_QQ_Data";
    QString legacyQqSourcePath = legacyContainerDataPath + "/tencent/_QQ_Data";
    QString qqDestPath = desktopPath + "/" + tr("QQ_Data");
    QString qqIconPath = "/usr/share/kmre/icons/mobileqq.png";
    QString qqPendingPath = qqSourcePath;

    QString qqImagePath = tencentPath + "/QQ_Images";
    QString qqImageName = "QQ_Images";
    QString qqImageLink = "../" + qqImageName;

    QString qqFilePath = tencentPath + "/QQfile_recv";
    QString qqFileName = "QQfile_recv";
    QString qqFileLink = "../" + qqFileName;

    QFileInfoList infoList = desktopDir.entryInfoList(QDir::AllEntries | QDir::System);
    QFileInfo dataInfo(dataDestPath);
    dataInfo.setCaching(false);
    dataInfo.refresh();
    //bool dataEntryExists = dataInfo.exists() || dataInfo.isSymLink();
    //qDebug() << "dataEntryExists " << dataEntryExists;
    for (QFileInfo& info : infoList) {
        if (info.isSymLink()) {
            QString target = info.symLinkTarget();
            //qDebug() << target;
            //qDebug() << info.absolutePath();
            //qDebug() << info.absoluteFilePath();
            if (target == wxSourcePath ||
                    target == dataSourcePath ||
                    target == qqSourcePath ||
                    target == legacyWxSourcePath ||
                    target == legacyDataSourcePath ||
                    target == legacyQqSourcePath) {
                //qDebug() << "filePath:        " << info.filePath();
                //qDebug() << "fileName:        " << info.fileName();
                //qDebug() << "baseName:        " << info.baseName();
                //qDebug() << "path:            " << info.path();
                //qDebug() << "Mobile_App_Data: " << tr("Mobile_App_Data");
                /*
                if (info.fileName() != tr("Mobile_App_Data")) {
                    if (target == dataSourcePath && QFile::exists(dataSourcePath) && !dataEntryExists) {
                        desktopDir.rename(info.fileName(), tr("Mobile_App_Data"));
                        dataInfo.refresh();
                        dataEntryExists = dataInfo.exists() || dataInfo.isSymLink();
                        //qDebug() << "dataEntryExists " << dataEntryExists;
                    } else {
                        deleteLink(info.absoluteFilePath());
                    }
                }
                */
                QString fileName = info.fileName();
                if (fileName == tr("Mobile_App_Data")
                        || fileName == tr("Android_Data")
                        || fileName == tr("Wechat_Data")
                        || fileName == tr("QQ_Data")
                        || fileName == "Mobile_App_Data"
                        || fileName == "Android_Data"
                        || fileName == "Wechat_Data"
                        || fileName == "QQ_Data") {
                    deleteLink(info.absoluteFilePath());
                }
            }
        }
    }

    //mHashDirectory.insert(wxSourcePath, { wxDestPath, wxIconPath, false } );

    //mHashStorage.insert(dataSourcePath, { dataDestPath, dataIconPath, true } );

    //mHashPending.insert(qqImagePath, { qqDependPath, qqImageLink, qqImageName, qqPendingPath } );
    //mHashPending.insert(qqFilePath, { qqDependPath, qqFileLink, qqFileName, qqPendingPath } );

    //mHashDepend.insert(qqDependPath, { qqSourcePath, { qqImagePath, qqFilePath }, { qqDestPath, qqIconPath, false } } );

    //mHashLink.insert(TencentPath, { tencentPath, "Tencent" } );

    /*
    {
        QFile file(wxDestPath);
        if (file.symLinkTarget() == wxSourcePath) {
            file_set_custom_icon(wxDestPath.toStdString().c_str(), wxIconPath.toStdString().c_str());
        }
    }

    {
        QFile file(qqDestPath);
        if (file.symLinkTarget() == qqSourcePath) {
            file_set_custom_icon(qqDestPath.toStdString().c_str(), qqIconPath.toStdString().c_str());
        }
    }

    {
        QFile file(dataDestPath);
        if (file.symLinkTarget() == dataSourcePath) {
            file_set_custom_icon(dataDestPath.toStdString().c_str(), dataIconPath.toStdString().c_str());
        }
    }
    */

    //deleteLink(oldDataDestPath);
}

FileWatcher* FileWatcher::GetInstance()
{
    if (m_pInstance == nullptr) {
        m_pInstance = new FileWatcher();
        m_pInstance->m_pSystemWatcher = new QFileSystemWatcher();
        m_pInstance->m_pPendingWatcher = new QFileSystemWatcher();
        m_pInstance->m_pDependWatcher = new QFileSystemWatcher();
        m_pInstance->m_pLinkWatcher = new QFileSystemWatcher();

        connect(m_pInstance->m_pSystemWatcher, SIGNAL(directoryChanged(QString)), m_pInstance, SLOT(directoryUpdated(QString)));
        connect(m_pInstance->m_pSystemWatcher, SIGNAL(fileChanged(QString)), m_pInstance, SLOT(fileUpdated(QString)));

        connect(m_pInstance->m_pPendingWatcher, SIGNAL(directoryChanged(QString)), m_pInstance, SLOT(pendingDirectoryUpdated(QString)));
        connect(m_pInstance->m_pDependWatcher, SIGNAL(directoryChanged(QString)), m_pInstance, SLOT(dependDirectoryUpdated(QString)));
        connect(m_pInstance->m_pLinkWatcher, SIGNAL(directoryChanged(QString)), m_pInstance, SLOT(linkDirectoryUpdated(QString)));
    }

    return m_pInstance;
}

bool FileWatcher::shouldRun()
{
    QJsonParseError err;
    QByteArray json;
    QString installedFilePath = "/var/lib/kmre/" + mContainerName + "/data/local/tmp/installed.json";
    QFile installedFile(installedFilePath);

    if (!installedFile.exists()) {
        //qDebug() << "File " + installedFilePath + " doesn't exist.";
        return false;
    }

    installedFile.open(QFile::ReadOnly);
    if (!installedFile.isOpen()) {
        //qDebug() << "Failed to open file " + installedFilePath + ".";
        return false;
    }

    json = installedFile.readAll();
    installedFile.close();

    //qDebug() << json;

    QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError) {
        //qDebug() << "Failed to parse file " + installedFilePath + ".";
        return false;
    }

    QJsonObject obj = doc.object();

    if (!obj.contains(QStringLiteral("data"))) {
        //qDebug() << "Json doesn't have data array.";
        return false;
    }

    QJsonArray dataArray = obj.value("data").toArray();
    //qDebug() << dataArray.isEmpty();
    //qDebug() << dataArray.size();
    //qDebug() << dataArray;
    if (dataArray.isEmpty() || dataArray.size() <= 0) {
        //qDebug() << "Data array doesn't have any item.";
        return false;
    }

    return true;
}

void FileWatcher::run()
{
    if (!mRunning.testAndSetOrdered(0, 1)) {
        return;
    }

    if (!shouldRun()) {
        QTimer::singleShot(5000, [=] () {
            qApp->quit();
        });
    }

    {
        QHashIterator<QString, LinkDestination> directoryIter(mHashDirectory);
        while (directoryIter.hasNext()) {
            directoryIter.next();

            {
                QMutexLocker lock(&mMutex);
                QString destination = directoryIter.value().destination;
                QString source = directoryIter.key();
                DirectoryCheckWorker* worker = new DirectoryCheckWorker(source);
                mDirectoryWorkers.insert(source, worker);
                QThread *t = new QThread();
                mThreads.insert(source, t);
                connect(this, SIGNAL(check(QString, int)), worker, SLOT(doCheck(QString, int)));
                connect(worker, SIGNAL(addPath(QString)), this, SLOT(addWatchPath(QString)));

                worker->moveToThread(t);
                t->start();

                {
                    QFile file(source);
                    if (!directoryIter.value().shouldCreate || !file.exists()) {
                        deleteLink(destination);
                    }
                }

                emit check(source, CHECK_TYPE_NORMAL);
            }
        }
    }

    {
        QHashIterator<QString, PendingDirectory> pendingIter(mHashPending);
        while (pendingIter.hasNext()) {
            pendingIter.next();
            {
                QMutexLocker lock(&mMutex);
                QString sourcePath = pendingIter.key();
                QString dependPath = pendingIter.value().dependPath;
                QString pendingPath = pendingIter.value().pendingPath;
                QString pendingName = pendingIter.value().pendingName;
                DirectoryCheckWorker *worker = new DirectoryCheckWorker(sourcePath);
                mDirectoryWorkers.insert(sourcePath, worker);
                QThread *t = new QThread();
                mThreads.insert(sourcePath, t);
                connect(this, SIGNAL(check(QString,int)), worker, SLOT(doCheck(QString,int)));
                connect(worker, SIGNAL(pendingAddPath(QString)), this, SLOT(pendingAddWatchPath(QString)));

                worker->moveToThread(t);
                t->start();

                {
                    QFile dependFile(dependPath);
                    if (!dependFile.exists()) {
                        dependDeleteLink(dependPath);
                    }

                    QFile source(sourcePath);
                    if (!source.exists()) {
                        QDir pending(pendingPath);
                        if (pending.exists()) {
                            pending.remove(pendingName);
                        }
                    }
                }

                emit check(sourcePath, CHECK_TYPE_PENDING);
            }
        }
    }

    {
        QHashIterator<QString, DependDirectory> dependIter(mHashDepend);
        while (dependIter.hasNext()) {
            dependIter.next();
            {
                QMutexLocker lock(&mMutex);
                QString destPath = dependIter.value().destLink.destination;
                QString dependPath = dependIter.key();
                QStringList directories = dependIter.value().directories;
                DirectoryCheckWorker *worker = new DirectoryCheckWorker(dependPath);
                mDirectoryWorkers.insert(dependPath, worker);
                QThread *t = new QThread();
                mThreads.insert(dependPath, t);
                connect(this, SIGNAL(check(QString,int)), worker, SLOT(doCheck(QString,int)));
                connect(worker, SIGNAL(dependAddPath(QString)), this, SLOT(dependAddWatchPath(QString)));

                worker->moveToThread(t);
                t->start();

                {
                    QFile dependFile(dependPath);
                    if (!dependFile.exists()) {
                        deleteLink(destPath);
                    }

                    QList<QString>::const_iterator i = directories.begin();
                    bool hasRequiredDirectory = false;
                    while (i != directories.end()) {
                        QFile file(*i);
                        if (file.exists()) {
                            hasRequiredDirectory = true;
                            break;
                        }
                        ++i;
                    }

                    if (!dependIter.value().destLink.shouldCreate || !hasRequiredDirectory) {
                        deleteLink(destPath);
                    }
                }

                emit check(dependPath, CHECK_TYPE_DEPEND);
            }
        }
    }

    {
        QHashIterator<QString, LinkInfo> linkIter(mHashLink);
        while (linkIter.hasNext()) {
            linkIter.next();
            {
                QMutexLocker lock(&mMutex);
                QString linkPath = linkIter.key();
                DirectoryCheckWorker *worker = new DirectoryCheckWorker(linkPath);
                mDirectoryWorkers.insert(linkPath, worker);
                QThread *t = new QThread();
                mThreads.insert(linkPath, t);
                connect(this, SIGNAL(check(QString,int)), worker, SLOT(doCheck(QString,int)));
                connect(worker, SIGNAL(linkAddPath(QString)), this, SLOT(linkAddWatchPath(QString)));

                worker->moveToThread(t);
                t->start();

                emit check(linkPath, CHECK_TYPE_LINK);
            }
        }
    }

    {
        QHashIterator<QString, LinkDestination> storageIter(mHashStorage);
        while (storageIter.hasNext()) {
            storageIter.next();

            {
                QMutexLocker lock(&mMutex);
                QString destination = storageIter.value().destination;
                QString source = storageIter.key();
                QString iconPath = storageIter.value().iconPath;
                StorageCheckWorker* worker = new StorageCheckWorker(source);
                mStorageWorkers.insert(source, worker);
                bool fuseMounted;
                QThread *t = new QThread();
                mThreads.insert(source, t);
                connect(this, SIGNAL(checkStorage(QString, bool)), worker, SLOT(doCheck(QString,bool)));
                connect(worker, SIGNAL(statusChanged(QString)), this, SLOT(storageStatusChanged(QString)));

                worker->moveToThread(t);
                t->start();

                // make or delete link
                {
                    QStorageInfo info(source);
                    if (info.isValid() &&
                        info.isReady() &&
                        (QString(info.device()) == QString("/dev/fuse")) &&
                        (QString(info.fileSystemType()) == QString("fuse"))) {
                        if (storageIter.value().shouldCreate) {
                            makeLink(destination, source, iconPath);
                        }
                        fuseMounted = true;
                    } else {
                        fuseMounted = false;
                        //deleteLink(destination);
                    }
                    if (!storageIter.value().shouldCreate) {
                        deleteLink(destination);
                    }
                }

                emit checkStorage(source, fuseMounted);
            }
        }
    }
}


void FileWatcher::storageStatusChanged(const QString &path)
{
    QString destination;
    QString source;
    QString iconPath;
    bool fuseMounted;

    if (!mHashStorage.contains(path)) {
        return;
    }

    QHash<QString, LinkDestination>::const_iterator iter = mHashStorage.find(path);
    if (iter == mHashStorage.end()) {
        return;
    }

    destination = iter.value().destination;
    source = iter.key();
    iconPath = iter.value().iconPath;

    QStorageInfo info(path);
    if (!info.isReady() || !info.isValid()) {
        deleteLink(destination);
        emit checkStorage(path, false);
        return;
    }

    if ((QString(info.device()) == QString("/dev/fuse")) &&
            (QString(info.fileSystemType()) == QString("fuse"))) {
        if (iter.value().shouldCreate) {
            makeLink(destination, source, iconPath);
        }
        fuseMounted = true;
    } else {
        deleteLink(destination);
        fuseMounted = false;
    }
    if (!iter.value().shouldCreate) {
        deleteLink(destination);
    }

    emit checkStorage(path, fuseMounted);
}

void FileWatcher::start()
{
}

void FileWatcher::stop()
{
    qApp->quit();
}

void FileWatcher::onContainerStopped(const QString &container)
{
    if (container == mContainerName) {
        //qDebug() << "Container " + mContainerName + " has been stopped.";
        qApp->quit();
        this->deleteLater();
    }
}

void FileWatcher::addWatchPath(const QString &path)
{
    //qDebug() << QString("Add to watch: %1").arg(path);

    m_pSystemWatcher->addPath(path);
    QHash<QString, LinkDestination>::const_iterator iter = mHashDirectory.find(path);
    if (iter == mHashDirectory.end()) {
        return;
    }

    if (iter.value().shouldCreate) {
        makeLink(iter.value().destination, iter.key(), iter.value().iconPath);
    }

}

void FileWatcher::pendingAddWatchPath(const QString &path)
{
    //qDebug() << QString("Pending Add to watch: %1").arg(path);

    m_pPendingWatcher->addPath(path);
    QHash<QString, PendingDirectory>::const_iterator iter = mHashPending.find(path);
    if (iter == mHashPending.end()) {
        return;
    }
    QString dependPath = iter.value().dependPath;
    QString pendingPath = iter.value().pendingPath;

    QFile dependFile(dependPath);
    QDir pending(pendingPath);
    if (!pending.exists()) {
        pending.mkpath(pendingPath);
    }

    makeLink(iter.value().pendingPath + "/" + iter.value().pendingName, iter.value().relativeLinkPath);
    if (dependFile.exists()) {
        dependMakeLink(dependPath);
    } else {
        dependDeleteLink(dependPath);
    }
}

void FileWatcher::linkAddWatchPath(const QString &path)
{
    //qDebug() << QString("Link Add to watch: %1").arg(path);
    m_pLinkWatcher->addPath(path);
    QHash<QString, LinkInfo>::const_iterator iter = mHashLink.find(path);
    if (iter == mHashLink.end()) {
        return;
    }

    QString destination = iter.value().destination;
    QString linkName = iter.value().linkName;
    QFile::link(linkName, destination);
}

void FileWatcher::dependAddWatchPath(const QString &path)
{
    //qDebug() << QString("Depend Add to watch: %1").arg(path);

    m_pDependWatcher->addPath(path);
    dependMakeLink(path);
}

void FileWatcher::dependMakeLink(const QString &path)
{
    QHash<QString, DependDirectory>::const_iterator iter = mHashDepend.find(path);
    if (iter == mHashDepend.end()) {
        return;
    }

    QString sourcePath = iter.value().targetPath;
    QString destPath = iter.value().destLink.destination;
    QString iconPath = iter.value().destLink.iconPath;
    QStringList directories = iter.value().directories;
    bool hasRequiredDirectory = false;

    QFile dependFile(path);
    if (dependFile.exists()) {
        QList<QString>::const_iterator i = directories.begin();
        while (i != directories.end()) {
            QFile file(*i);
            if (file.exists()) {
                hasRequiredDirectory = true;
                break;
            }
            ++i;
        }
    }

    if (iter.value().destLink.shouldCreate && hasRequiredDirectory) {
        makeLink(destPath, sourcePath, iconPath);
    }
}

void FileWatcher::dependDeleteLink(const QString &path)
{
    QHash<QString, DependDirectory>::const_iterator iter = mHashDepend.find(path);
    if (iter == mHashDepend.end()) {
        return;
    }

    QString destPath = iter.value().destLink.destination;
    QStringList directories = iter.value().directories;

    QFile dependFile(path);
    if (!dependFile.exists()) {
        deleteLink(destPath);
    }

    QList<QString>::const_iterator i = directories.begin();
    bool hasRequiredDirectory = false;
    while (i != directories.end()) {
        QFile file(*i);
        if (file.exists()) {
            hasRequiredDirectory = true;
            break;
        }
        ++i;
    }

    if (!hasRequiredDirectory) {
        deleteLink(destPath);
    }

}

void FileWatcher::directoryUpdated(const QString &path)
{
    //qDebug() << QString("Directory updated: %1").arg(path);
    QFileInfo info(path);
    if (info.exists()) {
        //qDebug() << QString("Directory changed: %1").arg(path);
    } else {
        //qDebug() << QString("Directory removed: %1").arg(path);
        m_pSystemWatcher->removePath(path);
        QHash<QString, LinkDestination>::const_iterator iter = mHashDirectory.find(path);
        if (iter != mHashDirectory.end()) {
            deleteLink(iter.value().destination);
        }

        {

            QMutexLocker lock(&mMutex);
            if (mDirectoryWorkers.contains(path)) {
                emit check(path, CHECK_TYPE_NORMAL);
            }

        }
    }
}

void FileWatcher::linkDirectoryUpdated(const QString &path)
{
    //qDebug() << QString("Link directory updated: %1").arg(path);
    QFileInfo info(path);

    if (info.exists()) {
        //qDebug() << QString("Directory changed: %1").arg(path);
    } else {
        //qDebug() << QString("Directory removed: %1").arg(path);
        m_pLinkWatcher->removePath(path);
        QHash<QString, LinkInfo>::const_iterator iter = mHashLink.find(path);
        if (iter != mHashLink.end()) {
            deleteLink(iter.value().destination);
        }
    }
}

void FileWatcher::pendingDirectoryUpdated(const QString &path)
{
    QFileInfo info(path);
    QHash<QString, PendingDirectory>::const_iterator iter = mHashPending.find(path);

    if (iter == mHashPending.end()) {
        return;
    }

    QString dependPath = iter.value().dependPath;
    QFileInfo dependFile(dependPath);

    if (!dependFile.exists()) {
        dependDeleteLink(dependPath);
    }

    if (info.exists()) {
        //qDebug() << QString("Pending directory changed: %1").arg(path);
    } else {
        //qDebug() << QString("Pending directory removed: %1").arg(path);
        m_pPendingWatcher->removePath(path);
        if (iter != mHashPending.end()) {
            deleteLink(iter.value().pendingPath + "/" + iter.value().pendingName);
        }

        dependDeleteLink(dependPath);

        {
            QMutexLocker lock(&mMutex);
            if (mDirectoryWorkers.contains(path)) {
                emit check(path, CHECK_TYPE_PENDING);
            }
        }
    }

}

void FileWatcher::dependDirectoryUpdated(const QString &path)
{
    QHash<QString, DependDirectory>::const_iterator iter = mHashDepend.find(path);
    if (iter == mHashDepend.end()) {
        return;
    }

    QFile dependFile(path);

    if (dependFile.exists()) {
        //qDebug() << QString("Depend directory changed: %1").arg(path);
        QString sourcePath = iter.value().targetPath;
        QString destPath = iter.value().destLink.destination;

        QFile destination(destPath);
        if (destination.symLinkTarget() != sourcePath) {
            dependMakeLink(path);
        }
    } else {
        //qDebug() << QString("Depend directory removed: %1").arg(path);
        m_pDependWatcher->removePath(path);
        dependDeleteLink(path);

        {
            QMutexLocker lock(&mMutex);
            if (mDirectoryWorkers.contains(path)) {
                emit check(path, CHECK_TYPE_DEPEND);
            }
        }
    }
}

void FileWatcher::fileUpdated(const QString &path)
{
    //qDebug() << QString("File updated: %1").arg(path);
    QFileInfo info(path);
    if (info.exists()) {
        //qDebug() << QString("Directory changed: %1").arg(path);
    } else {
        //qDebug() << QString("Directory removed: %1").arg(path);
        m_pSystemWatcher->removePath(path);
    }
}


void FileWatcher::makeLink(const QString &dest, const QString &source, const QString &iconPath)
{
    QFile file(dest);
    QString linkPath;


    linkPath = file.symLinkTarget();
    if (linkPath == source) {
        return;
    }

    if (linkPath.isEmpty() || linkPath.isNull()) {
        if (file.exists()) {
            // dest path is not a link file
            return;
        } else {
            QFile::link(source, dest);
            file_set_custom_icon(dest.toStdString().c_str(), iconPath.toStdString().c_str());
        }
    }
}

void FileWatcher::makeLink(const QString &dest, const QString &source)
{
    QFile file(dest);
    QString linkPath;

    linkPath = file.symLinkTarget();
    if (linkPath == source) {
        return;
    }

    if (linkPath.isEmpty() || linkPath.isNull()) {
        if (file.exists()) {
            // dest path is not a link file
            return;
        } else {
            QFile::link(source, dest);
        }
    }
}

void FileWatcher::deleteLink(const QString &dest)
{
    QFile file(dest);
    QString linkPath;

    linkPath = file.symLinkTarget();

    if (linkPath.isEmpty() || linkPath.isNull()) {
        if (file.exists()) {
            // dest path is not a link file
            return;
        }
    }

    QFile::remove(dest);
}

} // namespace kmre
