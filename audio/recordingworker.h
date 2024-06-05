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

#ifndef RECORDINGWORKER_H
#define RECORDINGWORKER_H

#include <QObject>
#include <vector>
#include <alsa/asoundlib.h>
#include <speex/speex_resampler.h>
#include <speex/speex_preprocess.h>

#include "audioserver.h"

class RecordingWorker : public QObject
{
    Q_OBJECT
public:
    RecordingWorker(QObject *parent = 0);
    ~RecordingWorker();

    void start();
    void stop();

signals:
    void requestSendAudioDataToAndroid(QByteArray buffer, int len);

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
        size_t period_bytes;
        uint32_t period_size;
        uint32_t buffer_size;
    } record_handle_t;

    const std::vector<snd_pcm_format_t> SampleFormats = {SND_PCM_FORMAT_S16_LE, SND_PCM_FORMAT_S8};

    UnixStream* mListenSock = nullptr;
    record_handle_t *mRecoder = nullptr;
    QByteArray mReadBuffer;
    uint32_t mClientSampleRate;
    snd_pcm_format_t mClientSampleFormat;

    SocketStream *m_stream = nullptr;
    SpeexResamplerState* mSpeexResampler = nullptr;
    SpeexPreprocessState *mSpeexPreprocesser = nullptr;

    void settingsForDenoise();
    bool initRecoder();
    void destroyRecorder();
};

#endif // RECORDINGWORKER_H
