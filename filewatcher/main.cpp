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

#include <QCoreApplication>
#include <QStorageInfo>
#include <QDebug>
#include <QStandardPaths>
#include <QTranslator>
#include <QLibraryInfo>
#include <QDir>
#include <QFile>

#include "file-watcher.h"
#include "file_watcher_adaptor.h"
#include "file-inotify/file-inotify-service.h"
#include "lock-file.h"

#include <unistd.h>
#include <sys/utsname.h>
#include <sys/syslog.h>

#define SERVICE_INTERFACE "cn.kylinos.Kmre.FileWatcher"
#define SERVICE_PATH "/cn/kylinos/Kmre/FileWatcher"
#define LOG_IDENT "KMRE_kylin-kmre-filewatcher"

static const char *unsupported_kernel[] = {
    "4.4.58",
};

static int kernel_check()
{
    struct utsname un;
    int n = 0;
    int i = 0;

    n = sizeof(unsupported_kernel) / sizeof(char*);

    if (uname(&un) != 0) {
        return -1;
    }

    for (i = 0; i < n; ++i) {
        if (strncmp(un.release, unsupported_kernel[i], strlen(unsupported_kernel[i])) == 0) {
            return -1;
        }
    }

    return 0;
}

static bool prepareLockFile(QString& lockFilePath)
{
    QString homeDirPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QString lockDirPath = homeDirPath + "/.kmre";
    lockFilePath = lockDirPath + "/kylin-kmre-filewatcher.lock";

    QDir lockDir(lockDirPath);
    if (!lockDir.exists()) {
        lockDir.mkpath(lockDirPath);
    }

    if (!lockDir.exists()) {
        //qDebug() << "Failed to create lock file directory " + lockDirPath;
        return false;
    }

    return true;
}

int main(int argc, char *argv[])
{
    int count = 0;

    QCoreApplication a(argc, argv);

    openlog(LOG_IDENT, LOG_NDELAY | LOG_NOWAIT | LOG_PID, LOG_USER);
    syslog(LOG_DEBUG, "kylin-kmre-filewatcher is starting.");

/*
    syslog(LOG_DEBUG, "Check kernel release version.");
    if (kernel_check() < 0) {
        fprintf(stderr, "Unsupported kernel.\n");
        syslog(LOG_CRIT, "Unsupported kernel, exit now.");
        exit(0);
    }
*/

    QString homeDirPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QString userDirsFilePath = homeDirPath + "/.config/user-dirs.dirs";
    while (!QFile::exists(userDirsFilePath)) {
        usleep(100 * 1000);

        if (++count > 30) {
            break;
        }
    }

    QString lockFilePath;
    syslog(LOG_DEBUG, "Prepare lock file directory.");
    if (!prepareLockFile(lockFilePath)) {
        syslog(LOG_ERR, "Failed to prepare lock file directory.");
        return 0;
    }

    if (!test_lockfile(lockFilePath.toStdString().c_str())) {
        //qDebug() << "Failed to lock file " +  lockFilePath;
        syslog(LOG_ERR, "Failed to check lockfile. Maybe another process has already held the lock.");
        return 0;
    }

    kmre::FileInotifyService* s = nullptr;
    s = kmre::FileInotifyService::getInstance();

    if (s) {
        s->initialize();
        s->addWatchRecursively(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
        s->addWatchRecursively(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
        s->addWatchRecursively(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
        s->addWatchRecursively(QStandardPaths::writableLocation(QStandardPaths::MoviesLocation));
        s->addWatchRecursively(QStandardPaths::writableLocation(QStandardPaths::MusicLocation));
        s->addWatchRecursively(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
        s->start();
    }

    QString locale = QLocale::system().name();
    QTranslator translator;
    if (locale == "zh_CN") {
        if (!translator.load("kylin-kmre-filewatcher_" + locale + ".qm",
                    ":/translations/")) {
            //qDebug() << "Load translation file: " << "kylin-kmre-filewatcher_" + locale + ".qm" << " failed!";
            syslog(LOG_WARNING, "Failed to load translation file: kylin-kmre-filewatcher_%s.qm.", locale.toStdString().c_str());
        } else {
            a.installTranslator(&translator);
        }
    }

    QTranslator qtTranslator;
    qtTranslator.load("qt_" + locale,
            QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    a.installTranslator(&qtTranslator);

    kmre::FileWatcher* fw = kmre::FileWatcher::GetInstance();

    FileWatcherAdaptor* adaptor = new FileWatcherAdaptor(fw);
    Q_UNUSED(adaptor);

    QDBusConnection connection = QDBusConnection::sessionBus();
    if (!connection.registerService(SERVICE_INTERFACE) || !connection.registerObject(SERVICE_PATH, fw)) {
        qDebug() << "Failed to register dbus service: " << connection.lastError().message();
        closelog();
        return 1;
    }

    syslog(LOG_DEBUG, "kylin-kmre-filewatcher is running.");
    fw->run();

    return a.exec();
}
