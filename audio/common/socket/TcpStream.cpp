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
#include "TcpStream.h"
#include "utils/sockets.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#ifndef _WIN32
#include <netinet/in.h>
#include <netinet/tcp.h>
#else
#include <ws2tcpip.h>
#endif

#define LISTEN_BACKLOG 4

TcpStream::TcpStream(size_t bufSize) : SocketStream(bufSize) {}

TcpStream::TcpStream(int sock, size_t bufSize) :
    SocketStream(sock, bufSize) {
    // disable Nagle algorithm to improve bandwidth of small
    // packets which are quite common in our implementation.
    kylindroid::socketTcpDisableNagle(sock);
}

int TcpStream::listen(const char* addr) {
    m_port = atoi(addr);
    m_sock = kylindroid::socketTcpServer(m_port, SOCK_STREAM);
    if (!valid())
        return int(ERR_INVALID_SOCKET);

    m_port = kylindroid::socketGetPort(m_sock);
    if (m_port < 0) {
        ::close(m_sock);
        return int(ERR_INVALID_SOCKET);
    }

    return 0;
}

SocketStream * TcpStream::accept() {
    int clientSock = kylindroid::socketAccept(m_sock);
    if (clientSock < 0)
        return NULL;

    return new TcpStream(clientSock, m_bufsize);
}

bool TcpStream::setAddr(const char* addr) {
    m_port = atoi(addr);
    if(m_port < 0)
        return false;

    return true;
}

int TcpStream::connect(const char* addr) {
    int port = atoi(addr);
    m_sock = kylindroid::socketTcpLoopbackClient(port, SOCK_STREAM);
    return valid() ? 0 : -1;
}

int TcpStream::connect(const char* hostname, int port)
{
    m_sock = kylindroid::socketTcpClient(hostname, port, SOCK_STREAM);
    return valid() ? 0 : -1;
}
