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

#include "dbus-client.h"

namespace kmre {

DBusClient::DBusClient()
    : mServiceNameOwned(false),
      mQueryInterface(nullptr),
      mNotifyInterface(nullptr)
{
    mQueryInterface = new QDBusInterface("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", QDBusConnection::sessionBus());
    mNotifyInterface = new QDBusInterface("cn.kylinos.Kmre.Manager", "/cn/kylinos/Kmre/Manager", "cn.kylinos.Kmre.Manager", QDBusConnection::sessionBus());
}

DBusClient::~DBusClient()
{
    if (mQueryInterface) {
        delete mQueryInterface;
    }

    if (mNotifyInterface) {
        delete mNotifyInterface;
    }
}

bool DBusClient::queryService()
{
    bool owned = false;

    if (mServiceNameOwned) {
        return true;
    }

    QDBusMessage response = mQueryInterface->call("NameHasOwner", "cn.kylinos.Kmre.Manager");
    if (response.type() == QDBusMessage::ReplyMessage) {
        owned = response.arguments().takeFirst().toBool();
    }

    mServiceNameOwned = owned;

    return mServiceNameOwned;
}

void DBusClient::notifyFile(QString path, QString mimeType)
{
    mNotifyInterface->call("addOneRecord", path, mimeType);
}

} // namespace kmre
