/*
 * Copyright (c) KylinSoft Co., Ltd. 2016-2024.All rights reserved.
 *
 * Authors:
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

#pragma once

#include <QObject>
#include <QtDBus>
#include <QDebug>
#include <QGSettings>
#include <QVector>
#include <memory>
#include "metatypes.h"
#include "singleton.h"


class BackendWorker;
class SignalManager;

struct AppInfo 
{
    QString pkgName;
    QString appName;
    QString version;
};

class ControlManager : public QObject, public kmre::SingletonP<ControlManager>
{
    Q_OBJECT
public:
    void getRunningAppList();
    QString getDisplayInformation();
    void closeApp(const QString &appName, const QString& pkgName, bool forceKill);
    void controlApp(int id, const QString &pkgName, int event_type, int event_value = 0);
    void addOneRecord(const QString &path, const QString &mime_type);
    void removeOneRecord(const QString &path, const QString &mime_type);
    void commandToGetAllFiles(int type);
    AndroidMetaList getAllFiles(const QString &uri, bool reverse_order);
    bool filesIsEmpty();
    void setSystemProp(int type, const QString &propName, const QString &propValue);
    QString getSystemProp(int type, const QString &propName);
    bool installApp(const QString &filePath, const QString &pkgName, 
                    const QString &appName, const QString &appName_ZH);
    bool uninstallApp(const QString &pkgName);
    void updateAllAppNameInDesktop();

public slots:
    void onSyncFiles(int type, AndroidMetaList metas, int totalNum);
    void onUpdateAppDesktopAndIcon(const QString &pkgName, int status, int type);

signals:
    // custom signals

private:
    explicit ControlManager();
    ~ControlManager();

    void initBackendThread();
    void initConnections();
    QVector<AppInfo> getInstalledAppList();
    AppInfo getAppInfo(QString pkgName);

    SignalManager *mSignalManager = nullptr;
    BackendWorker *mBackendWorker = nullptr;
    QThread *mBackendThread = nullptr;

    AndroidMetaList m_imageMetas;
    AndroidMetaList m_videoMetas;
    AndroidMetaList m_audioMetas;
    AndroidMetaList m_documentMetas;
    AndroidMetaList m_otherMetas;
    int m_totalNum;
    int m_currentNum;

    friend SingletonP<ControlManager>;
};

