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

#include "audioworker.h"

namespace kmre {

AudioWorker::AudioWorker(AudioServer *server, SocketStream *socket, unsigned int workerIndex)
    : mServer(server),
      mSocket(socket),
      mIndex(workerIndex),
      mStop(false)
{

}

AudioWorker::~AudioWorker()
{
    delete mSocket;
}



void AudioWorker::stop()
{
    stopAudio();
    mStop = true;
    mSocket->forceStop();
}

} // namespace kmre
