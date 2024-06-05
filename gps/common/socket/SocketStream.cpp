/*
 * Copyright (C) 2011 The Android Open Source Project
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
#include "SocketStream.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <QDebug>
#ifndef _WIN32
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <sys/socket.h>
#else
#include <ws2tcpip.h>
#endif

SocketStream::SocketStream(size_t bufSize) : m_sock(-1), m_bufsize(bufSize), m_buf(NULL) {}

SocketStream::SocketStream(int sock, size_t bufSize) : m_sock(sock), m_bufsize(bufSize), m_buf(NULL) {}

SocketStream::~SocketStream()
{
    if (m_sock >= 0) {
        forceStop();
#ifndef _WIN32
        if (close(m_sock) < 0)
            perror("Closing SocketStream failed");
#endif
        // DBG("SocketStream::~close  @ %d \n", m_sock);
        m_sock = -1;
    }
    if (m_buf != NULL) {
        free(m_buf);
        m_buf = NULL;
    }
}

int SocketStream::writeFully(const void *buffer, size_t size)
{
    ssize_t stat = ::send(m_sock, (const char *)buffer, size, MSG_NOSIGNAL);
    return stat;
}
const unsigned char *SocketStream::readFully(void *buf, size_t len)
{
    if (!valid())
        return NULL;
    ssize_t stat = ::recv(m_sock, (char *)(buf), len, 0);
    return (const unsigned char *)buf;
}

int SocketStream::recv(void *buf, size_t len)
{
    if (!valid())
        return int(ERR_INVALID_SOCKET);
    int res = 0;

    while (true) {
        res = ::recv(m_sock, (char *)buf, len, 0);
        if (res < 0) {
            if (errno == EINTR) {
                continue;
            }
        }
        break;
    }
    return res;
}

void SocketStream::forceStop()
{
    // Shutdown socket to force read/write errors.
#ifdef _WIN32
    ::shutdown(m_sock, SD_BOTH);
    // As documented by programmers on MSDN, shutdown implementation in Windows does
    // NOT result to unblocking threads that are blocked on a recv on the socket
    // being shut down. The only way to actually implement this behavior (expected
    // by this forceStop() implementation) is to rudely close the socket.
    ::closesocket(m_sock);
    m_sock = -1;
#else
    ::shutdown(m_sock, SHUT_RDWR);
#endif
}
