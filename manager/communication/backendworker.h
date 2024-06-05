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

#ifndef BACKENDWORKER_H
#define BACKENDWORKER_H

#include <QObject>

class QTimer;
struct AppInfo;

class BackendWorker : public QObject
{
    Q_OBJECT
public:
    explicit BackendWorker(const QString &userName, const QString &userId, QObject *parent = 0);
    ~BackendWorker();

public slots:
    bool hasEnvBootcompleted();
    void getRunningAppList();
    void closeApp(const QString &appName, const QString &pkgName);
    void pasueAllApps();
    void controlApp(int id, const QString &pkgName, int event_type, int event_value = 0);
    void insertOneRecordToAndroidDB(const QString &path, const QString &mime_type);
    void removeOneRecordFromAndroidDB(const QString &path, const QString &mime_type);
    void requestAllFilesFromAndroidDB(int type);
    bool updateDekstopAndIcon(const AppInfo &appInfo);
    void removeDekstopAndIcon(const QString &appName);
    bool generateDesktop(const AppInfo &appInfo);
    bool generateIcon(const QString &pkgName);
    QByteArray getInstalledAppListJsonStr();
    bool installApp(const QString &filePath, const QString &pkgName, const QString &appName);
    bool uninstallApp(const QString &pkgName);
    void setSystemProp(int type, const QString &propName, const QString &propValue);
    QString getSystemProp(int type, const QString &propName);

signals:
    void sendRunningAppList(const QByteArray &array);
    void launchFinished(const QString &pkgName, bool result);
    void closeFinished(const QString &pkgName, bool result);

private:
    QString m_loginUserName;
    QString m_loginUserId;

    template<typename T> T getCallback(QString func_name, void *&handle);
};

#endif // BACKENDWORKER_H
