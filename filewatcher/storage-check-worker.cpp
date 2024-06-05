/*
 * Copyright (c) KylinSoft Co., Ltd. 2016-2024.All rights reserved.
 *
 * Authors:
 *  Ma Chao    machao@kylinos.cn
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

#include <QStorageInfo>
#include <unistd.h>
#include <QDebug>
#include "storage-check-worker.h"

namespace kmre {

StorageCheckWorker::StorageCheckWorker(const QString &path)
    : mPath(path)
{

}

void StorageCheckWorker::doCheck(const QString &path, bool fuseMounted)
{

    if (path != mPath) {
        return;
    }



    while (true) {
        QStorageInfo info(path);
        bool isFuseMounted = false;

        usleep(2 * 1000 * 1000);

        if (!info.isValid() || !info.isReady()) {
            break;
        }


        if ((QString(info.device()) == QString("/dev/fuse")) && (QString(info.fileSystemType()) == QString("fuse"))){
            isFuseMounted = true;
        }


        if (fuseMounted != isFuseMounted) {
            break;
        }
    }

    emit statusChanged(path);
}

} // namespace kmre
