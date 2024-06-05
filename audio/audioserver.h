/*
 * Copyright (c) KylinSoft Co., Ltd. 2016-2024.All rights reserved.
 *
 * Authors:
 *  Kobe Lee    lixiang@kylinos.cn
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

#ifndef AUDIOSEVER_H
#define AUDIOSEVER_H

#include <QObject>
#include <QThread>
#include <QList>
#include <QMap>

#include "socket/UnixStream.h"

class PlaybackWorker;
class RecordingWorker;

class AudioServer : public QObject
{
    Q_OBJECT
public:
    explicit AudioServer(QObject *parent = nullptr);
    ~AudioServer();

public slots:
    void onInit();
    void onSleep(bool sleep);

private:
    PlaybackWorker *m_playWorker = nullptr;
    RecordingWorker *m_recordWorker = nullptr;
    QThread *mPlayWorkerThread = nullptr;
    QThread *mRecordWorkerThread = nullptr;
};

#endif // AUDIOSEVER_H
