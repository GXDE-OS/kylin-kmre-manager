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

#ifndef DIRECTORYCHECKWORKER_H
#define DIRECTORYCHECKWORKER_H

#include <QObject>
#include <QFileInfo>
namespace kmre {

class FileWatcher;

enum
{
    CHECK_TYPE_NORMAL = 0,
    CHECK_TYPE_PENDING,
    CHECK_TYPE_DEPEND,
    CHECK_TYPE_LINK,
};

class DirectoryCheckWorker : public QObject
{
    Q_OBJECT

public:
    explicit DirectoryCheckWorker(const QString& path);
    ~DirectoryCheckWorker();

signals:
    void addPath(const QString& path);
    void pendingAddPath(const QString& path);
    void dependAddPath(const QString& path);
    void linkAddPath(const QString& path);

public slots:
    void doCheck(const QString& path, int checkType);
private:
    QString mPath;
};

} // namespace kmre

#endif // DIRECTORYCHECKWORKER_H
