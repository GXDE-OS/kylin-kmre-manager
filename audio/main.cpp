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

#include <QCoreApplication>
#include <QDebug>

#include "audio_adaptor.h"
#include "kmreaudio.h"
#include "utils.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/file.h>
#include <pwd.h>
#include <sys/syslog.h>

#define SERVICE_INTERFACE "cn.kylinos.Kmre.Audio"
#define SERVICE_PATH "/cn/kylinos/Kmre/Audio"
#define LOG_IDENT "KMRE_kylin-kmre-audio"

static int open_lock_file(const char* path)
{
    int fd;
    fd = open(path, O_RDWR | O_CLOEXEC | O_CREAT, 0666);
    if (fd < 0) {
        //printf("[kylin-kmre-audio] Failed to open file: %s\n", path);
        return fd;
    }
    //fchmod(fd, 0644);

    return fd;
}

static int try_lock_file(int fd)
{
    int ret = -1;
    int count = 0;

    for (count = 0; count < 5; count++) {
        ret = flock(fd, LOCK_EX | LOCK_NB);
        if (ret == 0) {
            break;
        }
        if ((EBADF == errno) || (EINVAL == errno) || (EWOULDBLOCK == errno)) {
            break;
        }
        printf("[kylin-kmre-audio] Failed to lock file\n");
        sleep(1);
    }

    return ret;
}

static void unlock_fd(int fd)
{
    if (fd < 0) {
        return;
    }
    flock(fd, LOCK_UN);
    close(fd);
}

static int do_lock_file(const char* path, int *lockFd)
{
    int fd;
    int ret;

    fd = open_lock_file(path);
    if (fd < 0) {
        return -1;
    }

    ret = try_lock_file(fd);
    if (ret < 0) {
        //printf("[kylin-kmre-audio] Failed to lock file\n");
        close(fd);
        return -1;
    }

    if (lockFd) {
        *lockFd = fd;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    openlog(LOG_IDENT, LOG_NDELAY | LOG_NOWAIT | LOG_PID, LOG_USER);

    int nRet = 0;
    char lockFilePath[1024] = {0};
    char user[1024] = {0};
    char* home_dir = NULL;
    char home_path[1024] = {0};
    char lock_file_dir[1024] = {0};

    snprintf(user, sizeof(user), "%s", Utils::getUserName().toStdString().c_str());

    home_dir = getenv("HOME");
    if (!home_dir) {
        snprintf(home_path, sizeof(home_path), "/home/%s", user);
        home_dir = home_path;
    }

    snprintf(lock_file_dir, sizeof(lock_file_dir), "%s/.kmre", home_dir);
    snprintf(lockFilePath, sizeof(lockFilePath), "%s/kylin-kmre-audio.lock", lock_file_dir);

    mkdir(lock_file_dir, 0777);
    chmod(lock_file_dir, 0777);

    int lockfd;
    int ret = do_lock_file(lockFilePath, &lockfd);
    if (ret < 0) {
        //closelog();
        //printf("[kylin-kmre-audio] File %s has been locked for another kylin-kmre-audio, exit now\n", lockFilePath);
        exit(-1);
    }

    KmreAudio audio;
    audio.init();

    AudioAdaptor adaptor(&audio);
    Q_UNUSED(adaptor);

    QDBusConnection connection = QDBusConnection::sessionBus();
    if (!connection.registerService(SERVICE_INTERFACE) || !connection.registerObject(SERVICE_PATH, &audio)) {
        qDebug() << "Failed to register dbus service: " << connection.lastError().message();
        closelog();
        return 1;
    }

    nRet = a.exec();

    unlock_fd(lockfd);

    return nRet;
}
