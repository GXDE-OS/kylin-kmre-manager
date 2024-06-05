/*
 * Copyright (c) KylinSoft Co., Ltd. 2016-2024.All rights reserved.
 *
 * Authors:
 *  Yuan ShanShan    yuanshanshan@kylinos.cn
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
#include <QObject>
#include <QDebug>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/file.h>
#include <pwd.h>
#include <sys/syslog.h>
#include "utils.h"
#include "dbusadaptor.h"
#include "threadpool.h"
#include "kmregps.h"

#define BUF_SIZE 1024
#define LOG_IDENT "KMRE_kylin-gps-server"

static int open_lock_file(const char *path)
{
    int fd;
    fd = open(path, O_RDWR | O_CLOEXEC | O_CREAT, 0666);
    if (fd < 0) {
        return fd;
    }

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

static int do_lock_file(const char *path, int *lockFd)
{
    int fd;
    int ret;

    fd = open_lock_file(path);
    if (fd < 0) {
        return -1;
    }

    ret = try_lock_file(fd);
    if (ret < 0) {
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

    // openlog(LOG_IDENT, LOG_NDELAY | LOG_NOWAIT | LOG_PID, LOG_USER);

    int nRet = 0;
    char lockFilePath[BUF_SIZE] = {0};
    char user[BUF_SIZE] = {0};
    char *home_dir = NULL;
    char home_path[BUF_SIZE] = {0};
    char lock_file_dir[BUF_SIZE] = {0};

    snprintf(user, sizeof(user), "%s", Utils::getUserName().toStdString().c_str());

    home_dir = getenv("HOME");
    if (!home_dir) {
        snprintf(home_path, sizeof(home_path), "/home/%s", user);
        home_dir = home_path;
    }

    snprintf(lock_file_dir, sizeof(lock_file_dir), "%s/.kmre", home_dir);
    snprintf(lockFilePath, sizeof(lockFilePath), "%s/kylin-kmre-gps-server.lock", lock_file_dir);

    mkdir(lock_file_dir, 0777);
    chmod(lock_file_dir, 0777);

    int lockfd;
    int ret = do_lock_file(lockFilePath, &lockfd);
    if (ret < 0) {
        exit(-1);
    }

    /* 创建DBus服务 */
    DbusAdaptor adaptor;
    Q_UNUSED(adaptor);

    KmreGps gps;
    gps.init();

    nRet = a.exec();
    unlock_fd(lockfd);
    return nRet;
}
