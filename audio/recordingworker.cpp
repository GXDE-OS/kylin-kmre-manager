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

#include "recordingworker.h"
#include "myutils.h"

#include <sys/syslog.h>
#include <QDebug>


#define DEFAULT_SAMPLE_RATE 48000
#define DEFAULT_SAMPLE_FORMAT SND_PCM_FORMAT_S16_LE
#define DEFAULT_NUM_CHANNELS 1
#define READ_TRY_TIMES 500
#define RECORD_BUFFER_TIME_MAX 500000 // 500ms


RecordingWorker::RecordingWorker(QObject *parent) : QObject(parent)
      , mRecoder(nullptr)
      , mSpeexResampler(nullptr)
      , mSpeexPreprocesser(nullptr)
      , mClientSampleRate(0)
      , mClientSampleFormat(DEFAULT_SAMPLE_FORMAT)
{

}

void RecordingWorker::initData()
{
    mListenSock = new UnixStream();

    this->start();
}

RecordingWorker::~RecordingWorker()
{
    this->stop();
    if (mListenSock) {
        delete mListenSock;
    }

    if (mSpeexResampler) {
        speex_resampler_destroy(mSpeexResampler);
        mSpeexResampler = nullptr;
    }

    destroyRecorder();
}

void RecordingWorker::start()
{
    uint32_t speexResamplerInRate = 0;
    uint32_t speexResamplerOutRate = 0;
    int speexErr;
    int ret = 0;
    int count = 0;
    QByteArray speexOutBuffer;

    if (mListenSock) {
        QString socketPath = getUnixDomainSocketPath(false);
        if (socketPath.isEmpty()) {
            syslog(LOG_ERR, "RecordingWorker: Get socketPath is empty.");
            return;
        }

        if (mListenSock->listen(socketPath.toStdString().c_str()) < 0) {
            syslog(LOG_ERR, "RecordingWorker: listen %s failed.", socketPath.toStdString().c_str());
            return;
        }

        //必须要在listen完成之后再修改文件权限
        chmod(socketPath.toStdString().c_str(), 0777);

        while (1) {
            int32_t type;
            m_stream = mListenSock->accept();
            if (!m_stream) {
                syslog(LOG_ERR, "RecordingWorker: Error accepting connection");
                fprintf(stderr, "Error accepting connection, ignoring.\n");
                continue;
            }
            if (!m_stream->readFully(&type, sizeof(type))) {
                syslog(LOG_ERR, "RecordingWorker: Error reading client type info.");
                fprintf(stderr,"Error reading client info\n");
                m_stream->forceStop();
                delete m_stream;
                m_stream = nullptr;
                continue;
            }
            if (type == RECORDING) {//录音RECORDING: 从PCM读取数据，发送给android的audio.primary.kmre.so去处理
                if (!m_stream->readFully(&mClientSampleRate, sizeof(mClientSampleRate))) {
                    syslog(LOG_ERR, "RecordingWorker: Error reading client samplerate.");
                    fprintf(stderr,"Error reading client samplerate\n");
                    m_stream->forceStop();
                    delete m_stream;
                    m_stream = nullptr;
                    continue;
                }

                if (mClientSampleRate != 8000 && mClientSampleRate != 16000 && mClientSampleRate != 48000) {
                    mClientSampleRate = 0;
                }

                if (!mRecoder) {
                    mRecoder = new record_handle_t();
                    if (!initRecoder()) {
                        destroyRecorder();
                        continue;
                    }
                    speexOutBuffer = QByteArray(mRecoder->period_bytes * 2, 0);
                }

                if (mClientSampleRate == 0) {
                    if (mSpeexResampler) {
                        speex_resampler_destroy(mSpeexResampler);
                        mSpeexResampler = nullptr;
                    }
                } 
                else {
                    if (!mSpeexPreprocesser) {
                        //syslog(LOG_DEBUG, "RecordingWorker: create new mSpeexPreprocesser.");
                        mSpeexPreprocesser = speex_preprocess_state_init(mRecoder->period_size, mRecoder->sample_rate);
                        if (mSpeexPreprocesser) {
                            settingsForDenoise();
                        }
                    }

                    //syslog(LOG_DEBUG, "RecordingWorker: sample_rate = %d, mClientSampleRate = %d", mRecoder->sample_rate, mClientSampleRate);
                    if (mSpeexResampler) {
                        speex_resampler_get_rate(mSpeexResampler, &speexResamplerInRate, &speexResamplerOutRate);
                        if (speexResamplerInRate != mRecoder->sample_rate || speexResamplerOutRate != mClientSampleRate) {
                            speex_resampler_destroy(mSpeexResampler);
                            mSpeexResampler = nullptr;
                        }
                    }

                    if ((!mSpeexResampler) && (mRecoder->sample_rate != mClientSampleRate)) {
                        //syslog(LOG_DEBUG, "RecordingWorker: create new mSpeexResampler.");
                        mSpeexResampler = speex_resampler_init(
                                    mRecoder->channels, 
                                    mRecoder->sample_rate, 
                                    mClientSampleRate, 
                                    10, 
                                    &speexErr);
                        if (speexErr != RESAMPLER_ERR_SUCCESS && mSpeexResampler != nullptr) {
                            syslog(LOG_ERR, "RecordingWorker: speex_resampler_init failed!");
                            speex_resampler_destroy(mSpeexResampler);
                            mSpeexResampler = nullptr;
                        }
                    }
                }
            } 
            else {
                //syslog(LOG_DEBUG, "RecordingWorker: client type info is not RECORDING.");
                m_stream->forceStop();
                delete m_stream;
                m_stream = nullptr;
                continue;
            }

            while (1) {
                //读取音频
                int32_t nframe = snd_pcm_readi(mRecoder->pcm, mReadBuffer.data(), mRecoder->period_size);
                if (nframe == -EAGAIN ) {
                    snd_pcm_wait(mRecoder->pcm, 100);
                } 
                else if (nframe == -EPIPE) {
                    syslog(LOG_WARNING, "RecordingWorker: An overrun has occurred, some samples were lost!");
                    if (snd_pcm_prepare(mRecoder->pcm) < 0) {
                        syslog(LOG_ERR, "RecordingWorker: snd_pcm_prepare failed!");
                        break;
                    }
                    if(snd_pcm_start(mRecoder->pcm) < 0) {
                        syslog(LOG_ERR, "RecordingWorker: snd_pcm_start failed!");
                        break;
                    }
                } 
                else if (nframe == 0) {
                    // do nothing, continue.
                } 
                else if (nframe < 0) {
                    syslog(LOG_ERR, "RecordingWorker: read pcm device failed!");
                    break;
                }

                if (nframe > 0) {
                    if (mClientSampleFormat != mRecoder->sample_format) {// convert sample format
                        // TODO
                    }

                    if (mSpeexPreprocesser) {// denoise
                        speex_preprocess_run(mSpeexPreprocesser, (spx_int16_t*)mReadBuffer.data());
                    }
                    
                    spx_uint32_t inFrame = nframe;
                    spx_uint32_t outFrame = speexOutBuffer.size() / mRecoder->sample_bytes;
                    if (mSpeexResampler) {// resample
                        int result = speex_resampler_process_int(mSpeexResampler,
                                                             0,
                                                             (const spx_int16_t*)mReadBuffer.data(),
                                                             &inFrame,
                                                             (spx_int16_t*)speexOutBuffer.data(),
                                                             &outFrame);
                        //syslog(LOG_DEBUG, "RecordingWorker: nframe = %d, inFrame = %d, outFrame = %d, result = %d", 
                        //        nframe, inFrame, outFrame, result);
                        if (result == RESAMPLER_ERR_SUCCESS) {
                            ret = m_stream->writeFully(speexOutBuffer.constData(), outFrame * mRecoder->sample_bytes);
                            if (ret < 0) {
                                syslog(LOG_ERR, "RecordingWorker: Failed to write data to recording stream.");
                                fprintf(stderr, "Failed to write data to recording stream.\n");
                                break;
                            }
                        }
                    } 
                    else {
                        ret = m_stream->writeFully(mReadBuffer.data(), nframe * mRecoder->sample_bytes);
                        if (ret < 0) {
                            syslog(LOG_ERR, "RecordingWorker: Failed to write data to recording stream.");
                            fprintf(stderr, "Failed to write data to recording stream.\n");
                            break;
                        }
                    }

                    count = 0;
                } 
                else {
                    if (count++ == READ_TRY_TIMES) {
                        syslog(LOG_ERR, "RecordingWorker: Failed to read recording data from microphone.");
                        fprintf(stderr, "RecordingWorker: Failed to read recording data from microphone.\n");
                        break;
                    }
                    usleep(recordingDelay);
                }
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

void RecordingWorker::stop()
{
    destroyRecorder();
}

bool RecordingWorker::initRecoder()
{
    if (!mRecoder) {
        syslog(LOG_ERR, "RecordingWorker: Invalid record handle!");
        return false;
    }
    //syslog(LOG_DEBUG, "RecordingWorker: initRecoder...");
    mRecoder->period_size = RECORDING_BUF_SIZE;
    mRecoder->buffer_size = mRecoder->period_size * 8;

    snd_pcm_hw_params_t *hwparams;
    if (snd_pcm_hw_params_malloc(&hwparams) < 0) {
        syslog(LOG_ERR, "RecordingWorker: hw params malloc failed!");
        return false;
    }

    if (snd_pcm_open(&(mRecoder->pcm), "default", SND_PCM_STREAM_CAPTURE, 0/*SND_PCM_NONBLOCK*/) < 0) {
        syslog(LOG_ERR, "RecordingWorker: Open default PCM device failed!");
        return false;
    }

    do {
        if (snd_pcm_hw_params_any(mRecoder->pcm, hwparams) < 0) {
            syslog(LOG_ERR, "RecordingWorker: snd_pcm_hw_params_any fail!");
            break;
        }
    
        if (snd_pcm_hw_params_set_access(mRecoder->pcm, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
            syslog(LOG_ERR, "RecordingWorker: set pcm hw params failed!");
            break;
        }
    
        if (snd_pcm_hw_params_test_format(mRecoder->pcm, hwparams, mClientSampleFormat) < 0) {
            syslog(LOG_WARNING, "RecordingWorker: client requested sample format is not supported! choose another one!");
            for (auto format : SampleFormats) {
                if (snd_pcm_hw_params_test_format(mRecoder->pcm, hwparams, format) == 0) {
                    mRecoder->sample_format = format;
                    break;
                }
            }
            if (mRecoder->sample_format == 0) {
                syslog(LOG_ERR, "RecordingWorker: Can't find any supported sample format!");
                break;
            }
        }
        else {
            mRecoder->sample_format = mClientSampleFormat;
        }

        if (snd_pcm_hw_params_set_format(mRecoder->pcm, hwparams, mRecoder->sample_format) < 0) {
            syslog(LOG_ERR, "RecordingWorker: set pcm hw params format failed!");
            break;
        }
    
        mRecoder->channels = DEFAULT_NUM_CHANNELS;
        if (snd_pcm_hw_params_set_channels(mRecoder->pcm, hwparams, mRecoder->channels) < 0) {
            syslog(LOG_ERR, "RecordingWorker: set pcm hw params channel failed!");
            break;
        }
    
        unsigned int sample_rate = DEFAULT_SAMPLE_RATE;
        if (snd_pcm_hw_params_set_rate_near(mRecoder->pcm, hwparams, &sample_rate, 0) < 0) {
            syslog(LOG_ERR, "RecordingWorker: set pcm hw params rate failed!");
            break;
        }
        mRecoder->sample_rate = sample_rate;
        //syslog(LOG_DEBUG, "RecordingWorker: set pcm hw params rate: %d", sample_rate);

		snd_pcm_uframes_t period_size = mRecoder->period_size;
		if(snd_pcm_hw_params_set_period_size_near(mRecoder->pcm, hwparams, &period_size, NULL) < 0) {
			syslog(LOG_ERR, "RecordingWorker: set pcm hw params period size failed!");
            break;
		}
		if(period_size != mRecoder->period_size) {
            syslog(LOG_WARNING, "RecordingWorker: Period size %d is not supported, using %d instead.", 
                    mRecoder->period_size, period_size);
			mRecoder->period_size = period_size;
		}

		snd_pcm_uframes_t buffer_size = mRecoder->buffer_size;
		if(snd_pcm_hw_params_set_buffer_size_near(mRecoder->pcm, hwparams, &buffer_size) < 0) {
			syslog(LOG_ERR, "RecordingWorker: set pcm hw params buffer size failed!");
            break;
		}
		if(buffer_size != mRecoder->buffer_size) {
            syslog(LOG_WARNING, "RecordingWorker: Buffer size %d is not supported, using %d instead.", 
                    mRecoder->buffer_size, buffer_size);
			mRecoder->buffer_size = buffer_size;
		}

        if (snd_pcm_hw_params(mRecoder->pcm, hwparams) < 0) {
            syslog(LOG_ERR, "RecordingWorker: snd_pcm_hw_params fail.");
            break;
        }

        mRecoder->bits_per_sample = snd_pcm_format_physical_width(mRecoder->sample_format);
        mRecoder->sample_bytes = mRecoder->bits_per_sample / 8 * mRecoder->channels;
        mRecoder->period_bytes = mRecoder->period_size * mRecoder->sample_bytes;
    
        mReadBuffer = QByteArray(mRecoder->period_bytes, 0);

        if(snd_pcm_start(mRecoder->pcm) < 0) {
            syslog(LOG_ERR, "RecordingWorker: snd_pcm_start fail.");
            break;
        }
    
        snd_pcm_hw_params_free(hwparams);

        return true;
    }while(0);
 
	snd_pcm_hw_params_free(hwparams);
    snd_pcm_close(mRecoder->pcm);
    return false;
}

void RecordingWorker::destroyRecorder()
{
    //syslog(LOG_DEBUG, "RecordingWorker: destroyRecorder...");
    if (mRecoder) {
        snd_pcm_close(mRecoder->pcm);
        delete mRecoder;
        mRecoder = nullptr;
    }

    if (mSpeexPreprocesser) {
        speex_preprocess_state_destroy(mSpeexPreprocesser);
        mSpeexPreprocesser = nullptr;
    }
}

void RecordingWorker::settingsForDenoise()
{
    if (mSpeexPreprocesser) {
        int denoise = 1;
        int noise_suppress = -25;
        speex_preprocess_ctl(mSpeexPreprocesser, SPEEX_PREPROCESS_SET_DENOISE, &denoise);
        speex_preprocess_ctl(mSpeexPreprocesser, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &noise_suppress);
        
        int setting_int = 0;
        float setting_float = 0;

        setting_int = 1;
        speex_preprocess_ctl(mSpeexPreprocesser, SPEEX_PREPROCESS_SET_AGC, &setting_int);
        setting_float = 16000;
        speex_preprocess_ctl(mSpeexPreprocesser, SPEEX_PREPROCESS_SET_AGC_LEVEL, &setting_float);
        setting_int = 0;
        speex_preprocess_ctl(mSpeexPreprocesser, SPEEX_PREPROCESS_SET_DEREVERB, &setting_int);
        setting_float = 0;
        speex_preprocess_ctl(mSpeexPreprocesser, SPEEX_PREPROCESS_SET_DEREVERB_DECAY, &setting_float);
        setting_float = 0;
        speex_preprocess_ctl(mSpeexPreprocesser, SPEEX_PREPROCESS_SET_DEREVERB_LEVEL, &setting_float);

        //静音检测
        // setting_int = 0;
        // speex_preprocess_ctl(mSpeexPreprocesser, SPEEX_PREPROCESS_SET_VAD, &setting_int); 
        // setting_int = 80;
        // speex_preprocess_ctl(mSpeexPreprocesser, SPEEX_PREPROCESS_SET_PROB_START, &setting_int); 
        // setting_int = 65;
        // speex_preprocess_ctl(mSpeexPreprocesser, SPEEX_PREPROCESS_SET_PROB_CONTINUE, &setting_int);
        
    }
}
