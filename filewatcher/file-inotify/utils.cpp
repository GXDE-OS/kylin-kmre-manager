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

#include "utils.h"

#include <sys/stat.h>
#include <sys/types.h>

namespace kmre {

static bool isPathFileType(const char* path, mode_t fileType)
{
    struct stat sb;
    if (lstat(path, &sb) != 0) {
        return false;
    }

    return ((sb.st_mode & S_IFMT) == fileType);
}

bool isPathDir(const char* path)
{
    return isPathFileType(path, S_IFDIR);
}

bool isPathCharDevice(const char* path)
{
    return isPathFileType(path, S_IFCHR);
}

bool isPathRegularFile(const char* path)
{
    return isPathFileType(path, S_IFREG);
}

bool isPathSymlink(const char* path)
{
    return isPathFileType(path, S_IFLNK);
}

}
