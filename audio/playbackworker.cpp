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

#include "playbackworker.h"
#include "myutils.h"

#include <sys/syslog.h>
#include <QDebug>

#define DEFAULT_SAMPLE_RATE 48000
#define DEFAULT_SAMPLE_FORMAT SND_PCM_FORMAT_S16_LE
#define DEFAULT_NUM_CHANNELS 2



PlaybackWorker::PlaybackWorker(QObject *parent): QObject(parent)
      , mPlayback(nullptr)
      , mPlaybackBuffer(PLAYBACK_BUF_SIZE, 0)
{
    

}

void PlaybackWorker::initData()
{
    mListenSock = new UnixStream();
    if (createPlayback()) {
        this->start();
    }
    else {
        syslog(LOG_ERR, "PlaybackWorker: Create playback failed!");
    }
}

PlaybackWorker::~PlaybackWorker()
{
    this->stop();
    if (mListenSock) {
        delete mListenSock;
    }
}

void PlaybackWorker::start()
{
    if (mListenSock) {
        QString socketPath = getUnixDomainSocketPath(true);
        if (socketPath.isEmpty()) {
            syslog(LOG_ERR, "PlaybackWorker: Get socketPath is empty.");
            return;
        }

        if (mListenSock->listen(socketPath.toStdString().c_str()) < 0) {
            syslog(LOG_ERR, "PlaybackWorker: listen %s failed.", socketPath.toStdString().c_str());
            return;
        }

        //必须要在listen完成之后再修改文件权限
        chmod(socketPath.toStdString().c_str(), 0777);

        while (1) {
            int32_t type;

            m_stream = mListenSock->accept();
            if (!m_stream) {
		if (mExit) {
                    syslog(LOG_ERR, "PlaybackWorker: Exit playback now...");
                    break;
                }
                syslog(LOG_ERR, "PlaybackWorker: Error accepting connection.");
                fprintf(stderr, "Error accepting connection, ignoring.\n");
                continue;
            }

            if (!m_stream->readFully(&type, sizeof(type))) {
		if (mExit) {
                    syslog(LOG_ERR, "PlaybackWorker: Exit playback now...");
                    break;
                }
                syslog(LOG_ERR, "PlaybackWorker: Error reading client type info.");
                fprintf(stderr,"Error reading client info\n");
                m_stream->forceStop();
                delete m_stream;
                m_stream = nullptr;
                continue;
            }

            if (type == PLAYBACK) {//播放PLAYBACK: 从android的audio.primary.kmre.so读取音频数据，让 mPlayback 进行播放
                if (!mPlayback) {
                    mPlayback = new playback_handle_t();
                    mPlayback->sample_rate = DEFAULT_SAMPLE_RATE;
                    mPlayback->sample_format = DEFAULT_SAMPLE_FORMAT;
                    mPlayback->channels = DEFAULT_NUM_CHANNELS;

                    if (!initPlayback()) {
                        destroyPlayback();
                        continue;
                    }
                }
            } 
            else {
                syslog(LOG_DEBUG, "PlaybackWorker: client type info is not PLAYBACK.");
                m_stream->forceStop();
                delete m_stream;
                m_stream = nullptr;
                continue;
            }

            while (1) {
                const unsigned char* result = m_stream->readFully(mPlaybackBuffer.data(), PLAYBACK_BUF_SIZE);
                if (nullptr == result) {
                    syslog(LOG_ERR, "PlaybackWorker: Failed to read data from playback socket.");
                    fprintf(stderr, "Failed to read data from playback socket.\n");
                    break;
                }

                snd_pcm_uframes_t frames = PLAYBACK_BUF_SIZE / mPlayback->sample_bytes;
                snd_pcm_uframes_t wc = snd_pcm_writei(mPlayback->pcm, mPlaybackBuffer.constData(), frames);
                if (wc == -EPIPE) {
                    syslog(LOG_WARNING, "PlaybackWorker: snd_pcm_writei underrun occured.");
                    if (snd_pcm_prepare(mPlayback->pcm) < 0) {
                        syslog(LOG_CRIT, "PlaybackWorker: Can't recovery from underrun!");
                        break;
                    }
                    snd_pcm_writei(mPlayback->pcm, mPlaybackBuffer.constData(), frames);
                }
                else if (wc == -ESTRPIPE) {
                    int err;
                    syslog(LOG_ERR, "PlaybackWorker: snd_pcm_writei error(%d) occured!", wc);
                    while((err = snd_pcm_resume(mPlayback->pcm)) == -EAGAIN) {
                        usleep(1000);
                    }
                    if (err < 0) {
                        if (snd_pcm_prepare(mPlayback->pcm) < 0) {
                            syslog(LOG_CRIT, "PlaybackWorker: Can't recovery from underrun!");
                            break;
                        }
                    }
                }
                else if (wc < 0) {
                    syslog(LOG_ERR, "PlaybackWorker: snd_pcm_writei error(%d) occured!", wc);
                    break;
                }
                else if (wc != frames) {
                    syslog(LOG_ERR, "PlaybackWorker: short write! writed %d frames. frames = %d", wc, frames);
                }
            }

	    if (mExit) {
                syslog(LOG_ERR, "PlaybackWorker: Exit playback now...");
                break;
            }

            if (m_stream) {
                m_stream->forceStop();
                delete m_stream;
                m_stream = nullptr;
            }

            this->stop();
        }
    }
}

void PlaybackWorker::exitPlayback()
{
    if (m_stream) {
        m_stream->forceStop();
    }

    if (mListenSock) {
        mListenSock->forceStop();
    }

    mExit = true;
}

void PlaybackWorker::stop()
{
    destroyPlayback();
}

bool PlaybackWorker::createPlayback()
{
    if (!mPlayback) {
        mPlayback = new playback_handle_t();
        mPlayback->sample_rate = DEFAULT_SAMPLE_RATE;
        mPlayback->sample_format = DEFAULT_SAMPLE_FORMAT;
        mPlayback->channels = DEFAULT_NUM_CHANNELS;

        if (!initPlayback()) {
            destroyPlayback();
            return false;
        }
    }
    return true;
}

bool PlaybackWorker::initPlayback()
{
    if (!mPlayback) {
        syslog(LOG_ERR, "PlaybackWorker: Invalid record handle!");
        return false;
    }

    if (snd_pcm_open(&(mPlayback->pcm), "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        syslog(LOG_ERR, "PlaybackWorker: Open default PCM device failed!");
        return false;
    }

    snd_pcm_hw_params_t *hwparams;
    snd_pcm_hw_params_alloca(&hwparams);

    do {
        if (snd_pcm_hw_params_any(mPlayback->pcm, hwparams) < 0) {
            syslog(LOG_ERR, "PlaybackWorker: snd_pcm_hw_params_any fail!");
            break;
        }
    
        if (snd_pcm_hw_params_set_access(mPlayback->pcm, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
            syslog(LOG_ERR, "PlaybackWorker: set pcm hw params failed!");
            break;
        }

        if (snd_pcm_hw_params_set_format(mPlayback->pcm, hwparams, mPlayback->sample_format) < 0) {
            syslog(LOG_ERR, "PlaybackWorker: set pcm hw params format failed!");
            break;
        }
    
        if (snd_pcm_hw_params_set_channels(mPlayback->pcm, hwparams, mPlayback->channels) < 0) {
            syslog(LOG_ERR, "PlaybackWorker: set pcm hw params channel failed!");
            break;
        }
    
        if (snd_pcm_hw_params_set_rate_near(mPlayback->pcm, hwparams, &(mPlayback->sample_rate), 0) < 0) {
            syslog(LOG_ERR, "PlaybackWorker: set pcm hw params rate failed!");
            break;
        }
        syslog(LOG_DEBUG, "PlaybackWorker: set sample_rate: %d", mPlayback->sample_rate);

        mPlayback->bits_per_sample = snd_pcm_format_physical_width(mPlayback->sample_format);
        mPlayback->sample_bytes = mPlayback->bits_per_sample * mPlayback->channels / 8;
        syslog(LOG_DEBUG, "PlaybackWorker: bits_per_sample = %d, sample_bytes = %d", mPlayback->bits_per_sample, mPlayback->sample_bytes);

        snd_pcm_uframes_t buffer_size = PLAYBACK_BUF_SIZE;
        if (snd_pcm_hw_params_set_buffer_size_near(mPlayback->pcm, hwparams, &buffer_size) < 0) {
            syslog(LOG_ERR, "PlaybackWorker: snd_pcm_hw_params_set_buffer_size_near fail!");
            break;
        }

        snd_pcm_uframes_t period_size = buffer_size / mPlayback->sample_bytes;
        if (snd_pcm_hw_params_set_period_size_near(mPlayback->pcm, hwparams, &period_size, 0) < 0) {
            syslog(LOG_ERR, "PlaybackWorker: snd_pcm_hw_params_set_period_size_near fail!");
            break;
        }

        if (snd_pcm_hw_params(mPlayback->pcm, hwparams) < 0) {
            syslog(LOG_ERR, "PlaybackWorker: snd_pcm_hw_params fail.\n");
            break;
        }

        snd_pcm_sw_params_t *swparams;
        snd_pcm_sw_params_alloca(&swparams);

        snd_pcm_sw_params_current(mPlayback->pcm, swparams);

        if (snd_pcm_sw_params_set_avail_min(mPlayback->pcm, swparams, period_size) < 0) {
            syslog(LOG_ERR, "PlaybackWorker: snd_pcm_sw_params_set_avail_min fail!");
            break;
        }

        if (snd_pcm_sw_params_set_start_threshold(mPlayback->pcm, swparams, buffer_size) < 0) {
            syslog(LOG_ERR, "PlaybackWorker: snd_pcm_sw_params_set_start_threshold fail!");
            break;
        }

        if (snd_pcm_sw_params_set_stop_threshold(mPlayback->pcm, swparams, buffer_size) < 0) {
            syslog(LOG_ERR, "PlaybackWorker: snd_pcm_sw_params_set_stop_threshold fail!");
            break;
        }

        // if (snd_pcm_sw_params_set_silence_threshold(mPlayback->pcm, swparams, period_size) < 0) {
        //     syslog(LOG_ERR, "PlaybackWorker: snd_pcm_sw_params_set_stop_threshold fail!");
        //     break;
        // }

        // if (snd_pcm_sw_params_set_silence_size(mPlayback->pcm, swparams, ) < 0) {
        //     syslog(LOG_ERR, "PlaybackWorker: snd_pcm_sw_params_set_stop_threshold fail!");
        //     break;
        // }

        if (snd_pcm_sw_params(mPlayback->pcm, swparams) < 0) {
            syslog(LOG_ERR, "PlaybackWorker: snd_pcm_sw_params fail.\n");
            break;
        }


        // snd_pcm_uframes_t periods;
        // snd_pcm_uframes_t buffer;
        // snd_pcm_hw_params_get_period_size(hwparams, &periods, 0);
        // snd_pcm_hw_params_get_buffer_size(hwparams, &buffer);
        // syslog(LOG_DEBUG, "PlaybackWorker: buffer = %d, periods = %d", buffer, periods);

        return true;
    }while(0);
 
    snd_pcm_close(mPlayback->pcm);
    return false;
}

void PlaybackWorker::destroyPlayback()
{
    if (mPlayback) {
        snd_pcm_drop(mPlayback->pcm);
        snd_pcm_close(mPlayback->pcm);
        delete mPlayback;
        mPlayback = nullptr;
    }
}

