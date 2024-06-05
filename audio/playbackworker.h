/*
 * Copyright (c) KylinSoft Co., Ltd. 2016-2024.All rights reserved.
 *
 * Authors:
 *  Alan Xie    xiehuijun@kylinos.cn
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

#ifndef PLAYBACKWORKER_H
#define PLAYBACKWORKER_H

#include "audioserver.h"
#include <alsa/asoundlib.h>

class PlaybackWorker : public QObject
{
    Q_OBJECT
public:
    PlaybackWorker(QObject *parent = 0);
    ~PlaybackWorker();

    void start();
    void stop();
    void exitPlayback();

public slots:
    void initData();

private:
    typedef struct{
        snd_pcm_t *pcm;
        uint32_t channels;
        uint32_t sample_rate;
        snd_pcm_format_t sample_format;
        size_t bits_per_sample;
        size_t sample_bytes;
    } playback_handle_t;

    UnixStream* mListenSock = nullptr;
    playback_handle_t *mPlayback = nullptr;

    QByteArray mPlaybackBuffer;
    SocketStream *m_stream = nullptr;

private:
    bool mExit = false;
    bool createPlayback();
    bool initPlayback();
    void destroyPlayback();
};

#endif // PLAYBACKWORKER_H
