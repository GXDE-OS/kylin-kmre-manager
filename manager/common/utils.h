/*
 * Copyright (c) KylinSoft Co., Ltd. 2016-2024.All rights reserved.
 *
 * Authors:
 *  Kobe Lee    lixiang@kylinos.cn
 *  Alan Xie    xiehuijun@kylinos.cn
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

#ifndef UTILS_H
#define UTILS_H

#include <QString>

class Utils {
public:
    static QString getShellProgram();
    static const QString& getUserName();
    static const QString& getUid();
    static QString readFileContent(const QString &path);
    static bool copyFile(const QString &srcFile, const QString &destFile);
    static bool removeFile(const QString &file);
    static bool ifFileContainInfo(char * filePath, char * info);
    static bool isKmrePrefRunning();
    static QStringList getDnsListFromSysConfig();
    static QString getHWPlatformString();
    static bool tryLockFile(const QString& lockFilePath, int msecond);
    static void releaseLockFile();
    static QString execCmd(const QString &cmd, int msec);
    static QString getHostVirtType();
    static QString getHostCloudPlatform();
};

#endif // UTILS_H
