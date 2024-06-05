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

#include "lock-file.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <error.h>
#include <stdio.h>
#include <unistd.h>

static int open_lockfile(const char* path)
{
    int fd;
    fd = open(path, O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    if (fd < 0) {
        return fd;
    }

    fchmod(fd, 0644);

    return fd;
}

static int try_lockfile(int fd)
{
    int ret;

    ret = flock(fd, LOCK_EX | LOCK_NB);

    return ret;
}

bool test_lockfile(const char *path)
{
    int fd;
    int ret;

    fd = open_lockfile(path);
    if (fd < 0) {
        return false;
    }

    ret = try_lockfile(fd);
    if (ret < 0) {
        close(fd);
        return false;
    }

    return true;
}
