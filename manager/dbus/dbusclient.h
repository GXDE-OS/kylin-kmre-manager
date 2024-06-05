/*
 * Copyright (c) KylinSoft Co., Ltd. 2016-2024.All rights reserved.
 *
 * Authors:
 *  Kobe Lee    lixiang@kylinos.cn
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

#ifndef DBUSCLIENT_H
#define DBUSCLIENT_H

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusInterface>
#include <stdint.h>
#include <pthread.h>

#include "singleton.h"

namespace kmre {

class DBusClient : public SingletonP<DBusClient>
{
public:
    /* For system bus. */
    int32_t Prepare(const QString& userName, int32_t uid);
    int32_t StartContainer(const QString& userName, int32_t uid, int32_t width, int32_t height);
    void SetFocusOnContainer(const QString& userName, int32_t uid, int32_t onFocus);
    QString GetPropOfContainer(const QString& userName, int32_t uid, const QString& prop);
    void SetPropOfContainer(const QString& userName, int32_t uid, const QString& prop, const QString& value);
    QString GetDefaultPropOfContainer(const QString& userName, int32_t uid, const QString& prop);
    void SetDefaultPropOfContainer(const QString& userName, int32_t uid, const QString& prop, const QString& value);
    void SetGlobalEnvironmentVariable(const QString& key, const QString& value);

private:
    DBusClient();
    ~DBusClient();
    
    QDBusInterface* mSystemBusInterface = nullptr;

    friend SingletonP<DBusClient>;

};

} // namespace kmre

#endif // DBUSCLIENT_H
