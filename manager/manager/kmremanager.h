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

class QSessionManager;
class CameraService;
class ControlManager;
class SignalManager;
class PowerManager;

class KmreManager : public QObject, public kmre::SingletonP<KmreManager>
{
    Q_OBJECT

public slots:
    // dbus method
    void quit();
    QString getDisplayInformation();
    void closeApp(const QString &appName, const QString& pkgName, bool forceKill);
    void controlApp(int id, const QString &pkgName, int event_type, int event_value = 0);
    void addOneRecord(const QString &path, const QString &mime_type);
    void removeOneRecord(const QString &path, const QString &mime_type);
    void commandToGetAllFiles(int type);
    AndroidMetaList getAllFiles(const QString &uri, bool reverse_order);
    bool filesIsEmpty();
    void setCameraDevice(const QString &device);
    QString getCameraDevice();
    void setSystemProp(int type, const QString &propName, const QString &propValue);
    QString getSystemProp(int type, const QString &propName);
    // bool installApp(const QString &filePath, const QString &pkgName, 
    //                 const QString &appName, const QString &appName_ZH);
    // bool uninstallApp(const QString &pkgName);
    bool isHostSupportDDS();

private slots:
    void commitData(QSessionManager &manager);
    void onStopApplication(const QString &container);
    void onStartLogout();
    void onContainerEnvBooted(bool status, const QString &errInfo);

signals:
    // dbus signals
    void filesMessage(int type, AndroidMetaList metas);
    void currentCameraDevice(const QString &deviceName);
    void appSeparationStateChanged(const QString &pkgName, bool enable);

    // custom signals
    void requestRunningCamreService();

private:
    explicit KmreManager();
    ~KmreManager();

    void initDockerDns();
    void initConnections();
    void initCameraServiceThread();
    void initDisplayInformation();
    void initCpuInformation();
    void setAppropriateQemuDensity();
    void initHostPlatformProp();
    void stopServices();
    void uninhibit();

    ControlManager *mControlManager = nullptr;
    SignalManager *mSignalManager = nullptr;
    PowerManager* mPowerManager = nullptr;
    CameraService *mCameraServiceWorker = nullptr;
    QThread *mCameraThread = nullptr;

    friend SingletonP<KmreManager>;
};

