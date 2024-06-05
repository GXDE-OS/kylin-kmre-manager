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

#include "gpsdataget.h"
#include "myutils.h"
#include <sys/syslog.h>
#include <QDebug>


GpsdataGet::GpsdataGet(QObject *parent) : QObject(parent) {}

GpsdataGet *GpsdataGet::getInstance(void)
{
    static GpsdataGet instance;
    return &instance;
}

void GpsdataGet::initData()
{
    mListenSock = new UnixStream();
    this->start();
}

GpsdataGet::~GpsdataGet()
{
    if (mListenSock) {
        delete mListenSock;
        mListenSock = nullptr;
    }
}

void GpsdataGet::start()
{
    if (mListenSock) {
        QString socketPath = getUnixDomainSocketPath();
        if (socketPath.isEmpty()) {
            syslog(LOG_ERR, "GpsdataGet: Get socketPath is empty.");
            return;
        }
        if (mListenSock->listen(socketPath.toStdString().c_str()) < 0) {
            syslog(LOG_ERR, "GpsdataGet: listen %s failed.");
            return;
        }
        chmod(socketPath.toStdString().c_str(), 0777);

        qDebug() << "GpsdataGet: Waiting for accept";
        syslog(LOG_DEBUG, "GpsdataGet: Waiting for accept");
        m_stream = mListenSock->accept();
        if (!m_stream) {
            qDebug() << "GpsdataGet: Fail to accept.";
        } else {
            qDebug() << "GpsdataGet: Socket connect successfully.";
            sendData(gpsdata);
        }
    }
}

void GpsdataGet::sendData(QString data)
{
    qDebug() << "sendData()"<<data;
    int ret = writeData(data);
    if (ret < 0) {
        syslog(LOG_DEBUG, "GpsdataGet: Failed to write data to gps socket.");
        if (updateSocket(m_stream)) {
            writeData(data);
        }
    }
}

int GpsdataGet::writeData(QString data)
{
    int ret = 0;
    if (data == "")
        data = "3954.4405,N,11623.4799,E";
    QString gpsdata = "$GPGGA,005548,";
    gpsdata = gpsdata.append(data);
    gpsdata = gpsdata.append(",1,06,,0.0,M,0.,M,,0000*47");
    QByteArray ba;
    ba = gpsdata.toLatin1();
    char *ptr = ba.data();
    char senddata[65] = "";
    memcpy(senddata, ptr, 65);
    if (m_stream) {
        ret = m_stream->writeFully(senddata, sizeof(senddata));
    }
    return ret;
}

bool GpsdataGet::updateSocket(SocketStream *)
{
    m_stream = mListenSock->accept();
    if (!m_stream) {
        syslog(LOG_ERR, "updateSocket: Fail to accept.");
        return false;
    } else {
        return true;
    }
}
