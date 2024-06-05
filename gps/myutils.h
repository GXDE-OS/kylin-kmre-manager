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

#ifndef MYUTILS_H
#define MYUTILS_H

#include <QObject>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include "socket/SocketStream.h"
#include "utils.h"
#include "syslog.h"


const QString KMRE_PATH = "/var/lib/kmre";

inline QString containerName(int32_t uid, const QString &userName)
{
    return QString("kmre-") + QString::number(uid) + "-" + userName;
}

inline QString containerPath(int32_t uid, const QString &userName)
{
    return KMRE_PATH + "/" + containerName(uid, userName);
}
inline QString GpsSocketPath(int32_t uid, const QString &userName)
{
    return containerPath(uid, userName) + "/sockets/kmre_gps";
}


inline QString getUnixDomainSocketPath()
{
    QString socketPath;

    uint32_t uid = getuid();
    QString userName = Utils::getUserName();
    if (!userName.isEmpty()) {
        socketPath = GpsSocketPath(uid, userName);
    }
    else {
        syslog(LOG_ERR, "userName is empty.");
    }

    return socketPath;
}

inline int64_t time_diff_us(const struct timespec &now, const struct timespec &stamp)
{
    int64_t s_diff;
    int64_t us_diff;

    s_diff = (now.tv_sec - stamp.tv_sec) * 1000000; // us
    us_diff = (now.tv_nsec - stamp.tv_nsec) / 1000; // us

    return s_diff + us_diff;
}
#endif
