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

#ifndef SIGNALMANAGER_H
#define SIGNALMANAGER_H

#include <QObject>
#include "metatypes.h"
#include "singleton.h"

class SignalManager : public QObject, public kmre::SingletonP<SignalManager>
{
    Q_OBJECT

signals:
    void requestRunningAppList();
    void requestLaunchApp(const QString &pkgName, bool fullscreen, int width, int height, int density);
    void requestCloseApp(const QString &appName, const QString &pkgName);
    void requestPauseAllAndroidApps();
    void requestCloseAllApps();
    void requestControlApp(int id, const QString &pkgName, int event_type, int event_value);
    void requestAddFileRecord(const QString &path, const QString &mime_type);
    void requestRemoveFileRecord(const QString &path, const QString &mime_type);
    void requestAllFiles(int type);
    void requestSetSystemProp(int type, const QString &propName, const QString &propValue);

private:
    friend SingletonP<SignalManager>;
};

#endif // SIGNALMANAGER_H
