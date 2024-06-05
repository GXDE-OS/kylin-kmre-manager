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
#include "UnixStream.h"

#include "utils/sockets.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <sys/stat.h>

#include <limits.h>
#include <libgen.h>
#include <QDebug>
/* Not all systems define PATH_MAX, those who don't generally don't
 * have a limit on the maximum path size, so use a value that is
 * large enough for our very limited needs.
 */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

UnixStream::UnixStream(size_t bufSize) : SocketStream(bufSize), bound_socket_path(std::string()) {}

UnixStream::UnixStream(int sock, size_t bufSize) : SocketStream(sock, bufSize), bound_socket_path(std::string()) {}

UnixStream::~UnixStream()
{
    if (!bound_socket_path.empty()) {
        int ret = 0;
        do {
            ret = unlink(bound_socket_path.c_str());
        } while (ret < 0 && errno == EINTR);
        if (ret != 0) {
            perror("UNIX socket could not be unlinked");
        }
    }
}

static void mkdir_recursively(char *dir_name)
{
    struct stat sb;
    char tmp[PATH_MAX] = {0};
    char *p;

    if (!dir_name || (strcmp(dir_name, "") == 0))
        return;

    if (::stat(dir_name, &sb) == 0) {
        if (S_ISDIR(sb.st_mode))
            return;
    }

    p = dir_name;
    ++p;

    while ((p != NULL) && (*p != '\0')) {
        if (*p == '/') {
            snprintf(tmp, p - dir_name + 1, "%s", dir_name);
            if (stat(tmp, &sb) != 0) {
                ::mkdir(tmp, 0755);
            }
        }

        p++;
    }

    ::mkdir(dir_name, 0755);
}

static void mkpath(const char *path)
{
    if (!path || (strlen(path) == 0)) { // For fortify scan
        return;
    }

    char tmp[PATH_MAX] = {0};
    if (::access(path, F_OK) == 0) {
        ::remove(path);
        return;
    }

    strcpy(tmp, path);

    mkdir_recursively(dirname(tmp));
}


int UnixStream::listen(const char *addrstr)
{
    if (!setAddr(addrstr)) {
        return -1;
    }

    mkpath(bound_socket_path.c_str());

    m_sock = kmre::socketLocalServer(bound_socket_path.c_str(), SOCK_STREAM);

    if (!valid())
        return int(ERR_INVALID_SOCKET);

    return 0;
}

SocketStream *UnixStream::accept()
{
    int clientSock = -1;
    while (true) {
        struct sockaddr_un addr;
        socklen_t len = sizeof(addr);
        clientSock = ::accept(m_sock, (sockaddr *)&addr, &len);
        if (clientSock < 0 && errno == EINTR) {
            continue;
        }
        break;
    }

    UnixStream *clientStream = NULL;

    if (clientSock >= 0) {
        clientStream = new UnixStream(clientSock, m_bufsize);
    }
    return clientStream;
}

int UnixStream::connect(const char *addr)
{
    m_sock = kmre::socketLocalClient(addr, SOCK_STREAM);
    if (!valid())
        return -1;

    return 0;
}


bool UnixStream::setAddr(const char *addr)
{
    sockaddr_un local;

    if (::strlen(addr) >= sizeof(local.sun_path))
        return false;

    bound_socket_path = std::string(addr);
    return true;
}
