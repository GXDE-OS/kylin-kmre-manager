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

#ifndef CAMERE_SERVICE_H
#define CAMERE_SERVICE_H

#include <QObject>
#include <linux/videodev2.h>
#include <sys/ioctl.h>

class CameraService : public QObject
{
    Q_OBJECT
public:
    explicit CameraService(QObject *parent = 0);
    ~CameraService();

    QString getCameraDevice() {return m_cameraDeviceName;}

private:    
    int send_fd(int fd, int fd_to_send, int videoNum);
    int isAvailableCamera(int fd);

    QString m_cameraDeviceName;

public slots:    
    void startWorker(void);
    void setCameraDevice(const QString &device);

signals:
    void sigCurrentCameraDevice(const QString &device);
};

#endif // CAMERE_SERVICE_H
