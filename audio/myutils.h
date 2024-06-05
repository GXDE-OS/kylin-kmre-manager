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

#define DEFAULT_BUF_SIZE 4096
#define PLAYBACK_BUF_SIZE 3840

#define PLAYBACK      0
#define RECORDING     1
#define NONE          100

#define STREAM_BUFFER_SIZE 16384

const int usleepMaxTime = 100000;
const int sampleRate = 48000;
const int audioChannel = 2;
const int sampleBytes = 2;
const int bitsPerByte = 8;
const int64_t playbackDelay = (int64_t)(1000.0 * 1000.0 * PLAYBACK_BUF_SIZE / sampleRate / audioChannel / sampleBytes);
const int playbackDelayAdjust = 200 * PLAYBACK_BUF_SIZE / DEFAULT_BUF_SIZE;
const int waitOutputUs = 200; // us
const char bufferZero[STREAM_BUFFER_SIZE] = {0};

#define RECORDING_BUF_SIZE 1920
const int sampleRate2 = 48000;
const int audioChannel2 = 1;
const int waitInputUs = 200; // us
const int writeTempoAdjust = 50;
const int64_t recordingDelay = (int64_t)(1000.0 * 1000.0 * RECORDING_BUF_SIZE / sampleRate2 / audioChannel2 / sampleBytes);



const QString KMRE_PATH = "/var/lib/kmre";

inline QString containerName(int32_t uid, const QString& userName)
{
    return QString("kmre-") + QString::number(uid) + "-" + userName;
}

inline QString containerPath(int32_t uid, const QString& userName)
{
    return KMRE_PATH + "/" + containerName(uid, userName);
}

inline QString audioPlaySocketPath(int32_t uid, const QString& userName)
{
    return containerPath(uid, userName) + "/sockets/kmre_audio_playback";
}

inline QString audioRecordSocketPath(int32_t uid, const QString& userName)
{
    return containerPath(uid, userName) + "/sockets/kmre_audio_recording";
}


inline int64_t time_diff_ms(const struct timespec& now, const struct timespec& stamp)
{
    int64_t s_diff;
    int64_t ns_diff;

    s_diff = (now.tv_sec - stamp.tv_sec) * 1000; // ms
    ns_diff = (now.tv_nsec - stamp.tv_nsec) / 1000000; //ms

    return s_diff + ns_diff;
}

inline int64_t time_diff_us(const struct timespec& now, const struct timespec& stamp)
{
    int64_t s_diff;
    int64_t us_diff;

    s_diff = (now.tv_sec - stamp.tv_sec) * 1000000; // us
    us_diff = (now.tv_nsec - stamp.tv_nsec) / 1000; //us

    return s_diff + us_diff;
}

inline int64_t time_diff_ns(const struct timespec& now, const struct timespec& stamp)
{
    int64_t s_diff;
    int64_t ns_diff;

    s_diff = (now.tv_sec - stamp.tv_sec) * 1000000000; // ms
    ns_diff = (now.tv_nsec - stamp.tv_nsec); //ms

    return s_diff + ns_diff;
}

inline QString getUnixDomainSocketPath(bool isPlayback)
{
    QString socketPath;

    uint32_t uid = getuid();
    QString userName = Utils::getUserName();
    if (!userName.isEmpty()) {
        if (isPlayback)
            socketPath = audioPlaySocketPath(uid, userName);
        else
            socketPath = audioRecordSocketPath(uid, userName);
    }
    else {
        syslog(LOG_ERR, "userName is empty.");
    }

    return socketPath;
}

#endif
