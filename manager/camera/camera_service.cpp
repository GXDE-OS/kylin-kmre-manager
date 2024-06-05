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

#include "camera_service.h"

#include <stdio.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/syslog.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

#include <QApplication>
#include <QFile>
#include <QDir>
#include <QSettings>
#include <QDebug>

#include "utils.h"

#define KMRE_CAMERA_SOURCE "/dev/video0"

#define CONTROLLEN CMSG_LEN(sizeof(int))

CameraService::CameraService(QObject *parent) :
    QObject(parent)
{
    m_cameraDeviceName = "";
    QString confName = QDir::homePath() + "/.config/kmre/kmre.ini";
    if (QFile::exists(confName)) {
        QSettings settings(confName, QSettings::IniFormat);
        settings.setIniCodec("UTF-8");
        settings.beginGroup("camera");
        m_cameraDeviceName = settings.value("device", QString("")).toString();
        settings.endGroup();
    }
}

CameraService::~CameraService()
{

}

void CameraService::startWorker(void)
{
    bool useSelectedCamera = false;
    QString androidPath = QString("/var/lib/kmre/kmre-%1-%2").arg(Utils::getUid()).arg(Utils::getUserName());
    QString SocketPath = androidPath + "/sockets/kmre_camera";
    std::string str = SocketPath.toStdString();
    const char *cameraSocketPath = str.c_str();
    int listen_fd; 
    int com_fd; 
    int ret;
    int i;
    int recopt_ret;
    int fd_to_send;
    struct timeval timeout = {3, 0}; //3s
    static char recv_buf[24];  
    socklen_t len; 
    struct sockaddr_un clt_addr; 
    struct sockaddr_un srv_addr; 
    listen_fd = socket(AF_UNIX,SOCK_STREAM | SOCK_CLOEXEC,0);
//    printf("kmre_camera_service socket listen_fd=%d\n",listen_fd);
    if (listen_fd<0) {
        syslog(LOG_ERR, "Camera: cannot create communication socket");
        perror("cannot create communication socket"); 
        return;
    }  
    //set server addr_param 
    memset(&srv_addr, 0 , sizeof(struct sockaddr_un));
    srv_addr.sun_family=AF_UNIX; 
    strncpy(srv_addr.sun_path,cameraSocketPath,sizeof(srv_addr.sun_path)-1); 
    unlink(cameraSocketPath); 
    //bind sockfd & addr 
    ret=bind(listen_fd,(struct sockaddr*)&srv_addr,sizeof(srv_addr)); 
//    printf("kmre_camera_service socket bind ret = %d\n",ret);
    if(ret==-1){
        syslog(LOG_ERR, "Camera: cannot bind server socket");
        perror("cannot bind server socket"); 
        close(listen_fd); 
        unlink(cameraSocketPath); 
        return;
    } 
    chmod(srv_addr.sun_path, S_IRWXU | S_IRWXG | S_IRWXO);

    //listen sockfd  
    ret=listen(listen_fd,1); 
//    printf("kmre_camera_service socket listen ret=%d\n",ret);
    if(ret==-1){
        syslog(LOG_ERR, "Camera: cannot listen the client connect request");
        perror("cannot listen the client connect request"); 
        close(listen_fd); 
        unlink(cameraSocketPath); 
        return;
    } 
    //have connect request use accept 
    len=sizeof(clt_addr); 
    while(1){
//        printf("kmre_camera_service socket accept\n");
        com_fd=accept(listen_fd,(struct sockaddr*)&clt_addr,&len); 
//        printf("kmre_camera_service socket accept com_fd=%d\n",com_fd);
        if(com_fd<0){
            syslog(LOG_ERR, "Camera: Cannot accept client connect request");
            perror("cannot accept client connect request"); 
            close(listen_fd); 
            unlink(cameraSocketPath); 
            return;
        }

        recopt_ret = setsockopt(com_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*) &timeout, sizeof(timeout));
//        printf("**************** recopt_ret=%d\n",recopt_ret);
        memset(recv_buf,0,24);
        ssize_t num=recv(com_fd,recv_buf,sizeof(recv_buf),0);
        //printf("Message from client (%zd)) :%s\n",num,recv_buf);
        char dev_name[24];
        char dev_name_new[24];

        snprintf(dev_name, sizeof(dev_name), "%s", m_cameraDeviceName.toStdString().c_str());

        fd_to_send = open(dev_name,O_RDWR | O_CLOEXEC);
        if(fd_to_send >0 && ((ret = isAvailableCamera(fd_to_send)) ==0) ){
            useSelectedCamera = true;
        }else{
            for (i=0;i<12;i++) {
                if(fd_to_send > 0)
                    close(fd_to_send);
                memset(dev_name_new, 0, sizeof(dev_name_new));
                snprintf(dev_name_new, sizeof(dev_name_new), "/dev/video%d", i);
                if(strcmp(dev_name_new,dev_name) == 0)
                    continue;
                fd_to_send = open(dev_name_new,O_RDWR | O_CLOEXEC);
                strcpy(dev_name,dev_name_new);
                if(fd_to_send > 0 && ((ret = isAvailableCamera(fd_to_send)) ==0) ){
                    break;
                }
                if (i >= 11) {
                    syslog(LOG_ERR,"open kmre camera error\n");
                }
            }
        }

        if (fd_to_send > 0) {
            m_cameraDeviceName = QString(dev_name);
            if (useSelectedCamera) {
                emit this->sigCurrentCameraDevice(m_cameraDeviceName);
            }
            int videoNum = 0;
            QRegExp rx("\\d+$");
            rx.indexIn(m_cameraDeviceName, 0);
            videoNum = rx.cap(0).toInt();
            send_fd(com_fd, fd_to_send, videoNum);
        }

        close(fd_to_send);
        close(com_fd);
    }
}

int CameraService::isAvailableCamera(int fd)
{
    int ret;
    struct v4l2_capability videoIn;
    ret = ioctl (fd, VIDIOC_QUERYCAP, &videoIn);
    if (ret < 0) {
        syslog(LOG_ERR,"Camera: Error opening device: unable to query device.");
        return -1;
    }

    if ((videoIn.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0) {
        syslog(LOG_ERR,"Camera: Error opening device: video capture not supported.");
        return -1;
    }

    if (!(videoIn.capabilities & V4L2_CAP_STREAMING)) {
        syslog(LOG_ERR,"Camera: Capture device does not support streaming i/o");
        return -1;
    }
    return 0;
}

int CameraService::send_fd(int fd, int fd_to_send, int videoNum)
{
    static struct cmsghdr *cmptr = NULL;
    struct iovec iov[1];
    struct msghdr msg;
    char buf[3] = {0};

    iov[0].iov_base = buf;
    iov[0].iov_len = sizeof(buf);
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;

//    printf("send_fd fd_to_send= %d\n",fd_to_send);
    if(fd_to_send < 0) {
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
        buf[1] = -fd_to_send;
        if (buf[1] == 0) {
            buf[1] = 1;
        }
    }else {
        if (cmptr == NULL && (cmptr = (cmsghdr*)malloc(CONTROLLEN)) == NULL) {
            return -1;
        }
        cmptr->cmsg_level = SOL_SOCKET;
        cmptr->cmsg_type = SCM_RIGHTS;
        cmptr->cmsg_len = CONTROLLEN;
        msg.msg_control = cmptr;
        msg.msg_controllen = CONTROLLEN;
        *(int*)CMSG_DATA(cmptr) = fd_to_send;
        buf[1] = 0;
        buf[2] = videoNum;
    }

    buf[0] = 0;
    int send_result;
//    printf("send_fd sendmsg\n");
    send_result = sendmsg(fd, &msg, MSG_NOSIGNAL);
//    printf("send_fd sendmsg send_result=%d\n",send_result);
    if(send_result != 2){
        return -1;
    }

    return 0;
}

void CameraService::setCameraDevice(const QString &device)
{
    if (!device.isNull() && !device.isEmpty()) {
        m_cameraDeviceName = device;
    }
}
