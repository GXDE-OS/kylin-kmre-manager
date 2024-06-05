/*
 * Copyright (c) KylinSoft Co., Ltd. 2016-2024.All rights reserved.
 *
 * Authors:
 *  Yuan ShanShan    yuanshanshan@kylinos.cn
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

#ifndef GPSDATAGET_H
#define GPSDATAGET_H
#include <QObject>
#include <QThread>
#include <QList>
#include <QMap>
#include <pthread.h>
#include <QTime>
#include <QDateTime>
#include "socket/SocketStream.h"
#include "socket/UnixStream.h"


class GpsdataGet : public QObject
{
    Q_OBJECT
public:
    GpsdataGet(QObject *parent = 0);
    ~GpsdataGet();
    static GpsdataGet *getInstance(void);
    QString gpsdata = "";

public slots:
    void initData();
    void sendData(QString data);

private:
    UnixStream *mListenSock = nullptr;
    QByteArray mBuffer;
    SocketStream *m_stream = nullptr;
    bool flag = false;
    void start();

    int writeData(QString data);
    bool updateSocket(SocketStream *);
};

#endif // GPSDATAGET_H
