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

#ifndef AUDIOWORKER_H
#define AUDIOWORKER_H

#include <QObject>

#include "socket/SocketStream.h"

namespace kmre {

#define PLAYBACK      0
#define RECORDING     1
#define NONE          100

#define STREAM_BUFFER_SIZE 16384

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

class AudioServer;

class AudioWorker : public QObject
{
    Q_OBJECT
public:
    explicit AudioWorker(AudioServer* server, SocketStream* socket, unsigned int workerIndex);
    ~AudioWorker();

public:
    virtual void start() = 0;
    void stop();
    virtual void stopAudio() = 0;

signals:
    void finished(unsigned int workerIndex);

//protected slots:
//    virtual void onSocketError(QLocalSocket::LocalSocketError socketError) = 0;

public slots:
    virtual void run(unsigned int workerIndex) = 0;

protected:
    AudioServer* mServer;
    SocketStream* mSocket;
    unsigned int mIndex;
    bool mStop;
};

} // namespace kmre

#endif // AUDIOWORKER_H
