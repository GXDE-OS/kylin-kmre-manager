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

#include "sensordataget.h"
#include "myutils.h"
#include <sys/syslog.h>
#include <QDebug>


SensordataGet::SensordataGet(QObject *parent) : QObject(parent) {}

SensordataGet *SensordataGet::getInstance(void)
{
    static SensordataGet instance;
    return &instance;
}

void SensordataGet::initData()
{
    mListenSock = new UnixStream();
    this->start();
}

SensordataGet::~SensordataGet()
{
    if (mListenSock) {
        delete mListenSock;
        mListenSock = nullptr;
    }
}

void SensordataGet::start()
{
    if (mListenSock) {
        QString socketPath = getUnixDomainSocketPath();
        if (socketPath.isEmpty()) {
            syslog(LOG_ERR, "SensordataGet: Get socketPath is empty.");
            return;
        }
        if (mListenSock->listen(socketPath.toStdString().c_str()) < 0) {
            syslog(LOG_ERR, "SensordataGet: listen %s failed.");
            qDebug()<<"连接失败";
            return;
        }
        chmod(socketPath.toStdString().c_str(), 0777);

        syslog(LOG_DEBUG, "SensordataGet: Waiting for accept");
        m_stream = mListenSock->accept();
        qDebug()<<"接收成功";
        if (!m_stream) {
            syslog(LOG_ERR, "SensordataGet: Fail to accept.");
        } else {
            syslog(LOG_DEBUG, "SensordataGet: Socket connect successfully.");
        }
    }
}

void SensordataGet::sendData(QString data)
{
    qDebug()<<"data:"<<data;
    if (m_stream) {
        int ret = writeData(data);
        if (ret < 0) {
            syslog(LOG_DEBUG, "SensordataGet: Failed to write data to sensor socket.");
            updateSocket(m_stream);
            ret = writeData(data);
        }
    }
}

int SensordataGet::writeData(QString data)
{
    qDebug()<<"data:"<<data;
    QString temp ="acceleration:";
    temp.append(data);
    QDateTime time = QDateTime::currentDateTime();
    long int timeT = time.toTime_t();
    QByteArray ba;
    ba = temp.toLatin1();
    char *ptr = ba.data();
    char senddata[24] = "";
    memcpy(senddata, ptr, 24);
    char sensordata[128] = {0};
    sprintf(sensordata, "%ssync:%ld", senddata, timeT);
    int ret = m_stream->writeFully(sensordata, sizeof(sensordata));
    return ret;
}

void SensordataGet::updateSocket(SocketStream *)
{
    m_stream = mListenSock->accept();
    if (!m_stream) {
        syslog(LOG_ERR, "updateSocket: Fail to accept.");
    } else {
        syslog(LOG_DEBUG, "updateSocket: Socket connect successfully.");
    }
}
