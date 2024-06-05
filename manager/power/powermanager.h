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
#include <QDBusInterface>
#include <QString>
#include "singleton.h"

class PowerManager : public QObject, public kmre::SingletonP<PowerManager>
{
    Q_OBJECT
private:
    explicit PowerManager();
    ~PowerManager(){}
    
public slots:
    void onUpdateBatteryState();

private:
    QString getBatteryPath();

private:
    QDBusInterface *mBatterySysBus = nullptr;
    int mPowerState = -1;
    double mBatteryPercent = 0;
    QString mBatteryPath = "";

    friend SingletonP<PowerManager>;
};