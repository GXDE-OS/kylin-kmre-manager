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

#include "dbusclient.h"
#include <QMutexLocker>

namespace kmre {

DBusClient::DBusClient()
    : mSystemBusInterface(nullptr)
{
    mSystemBusInterface = new QDBusInterface("cn.kylinos.Kmre",
                                             "/cn/kylinos/Kmre",
                                             "cn.kylinos.Kmre",
                                             QDBusConnection::systemBus());

}

DBusClient::~DBusClient()
{
    if (mSystemBusInterface) {
        delete mSystemBusInterface;
    }

}

/***********************************************************
   Function:       Prepare
   Description:    启动容器的准备工作
   Calls:
   Called By:
   Input:
   Output:
   Return:
   Others:
 ************************************************************/
int32_t DBusClient::Prepare(const QString &userName, int32_t uid)
{
    int ret = -1;

    QDBusMessage response = mSystemBusInterface->call("Prepare", userName, uid);
    if (response.type() == QDBusMessage::ReplyMessage) {
        ret = response.arguments().takeFirst().toInt();
    }

    return ret;
}

/***********************************************************
   Function:       StartContainer
   Description:    开启指定容器
   Calls:
   Called By:
   Input:
   Output:
   Return:
   Others:
 ************************************************************/
int32_t DBusClient::StartContainer(const QString &userName, int32_t uid, int32_t width, int32_t height)
{
    int ret = -1;

    QDBusMessage response = mSystemBusInterface->call("StartContainer", userName, uid, width, height);
    if (response.type() == QDBusMessage::ReplyMessage) {
        ret = response.arguments().takeFirst().toInt();
    }

    return ret;
}

/***********************************************************
   Function:       SetFocusOnContainer
   Description:    设置指定容器激活与否
   Calls:
   Called By:
   Input:
   Output:
   Return:
   Others:
 ************************************************************/
void DBusClient::SetFocusOnContainer(const QString &userName, int32_t uid, int32_t onFocus)
{
    mSystemBusInterface->call("SetFocusOnContainer", userName, uid, onFocus);
}

/***********************************************************
   Function:       GetPropOfContainer
   Description:    从容器中的android环境获取指定属性的值
   Calls:
   Called By:
   Input:
   Output:
   Return:
   Others:
 ************************************************************/
QString DBusClient::GetPropOfContainer(const QString &userName, int32_t uid, const QString &prop)
{
    QString value;

    QDBusMessage response = mSystemBusInterface->call("GetPropOfContainer", userName, uid, prop);
    if (response.type() == QDBusMessage::ReplyMessage) {
        value = response.arguments().takeFirst().toString();
    }

    return value;
}

/***********************************************************
   Function:       SetPropOfContainer
   Description:    给容器中的android环境设置指定属性的值
   Calls:
   Called By:
   Input:
   Output:
   Return:
   Others:
 ************************************************************/
void DBusClient::SetPropOfContainer(const QString &userName, int32_t uid, const QString &prop, const QString &value)
{
    mSystemBusInterface->call("SetPropOfContainer", userName, uid, prop, value);
}

/***********************************************************
   Function:       GetDefaultPropOfContainer
   Description:    从容器中的android环境获取默认属性
   Calls:
   Called By:
   Input:
   Output:
   Return:
   Others:
 ************************************************************/
QString DBusClient::GetDefaultPropOfContainer(const QString &userName, int32_t uid, const QString &prop)
{
    QString value;

    QDBusMessage response = mSystemBusInterface->call("GetDefaultPropOfContainer", userName, uid, prop);
    if (response.type() == QDBusMessage::ReplyMessage) {
        value = response.arguments().takeFirst().toString();
    }

    return value;
}

/***********************************************************
   Function:       SetDefaultPropOfContainer
   Description:    给容器中的android环境设置默认属性
   Calls:
   Called By:
   Input:
   Output:
   Return:
   Others:
 ************************************************************/
void DBusClient::SetDefaultPropOfContainer(const QString &userName, int32_t uid, const QString &prop, const QString &value)
{
    mSystemBusInterface->call("SetDefaultPropOfContainer", userName, uid, prop, value);
}

/***********************************************************
   Function:       SetGlobalEnvironmentVariable
   Description:    为kylin-kmre-appstream和kylin-kmre-window设置环境变量
   Calls:
   Called By:
   Input:
   Output:
   Return:
   Others:
 ************************************************************/
void DBusClient::SetGlobalEnvironmentVariable(const QString &key, const QString &value)
{
    mSystemBusInterface->call("SetGlobalEnvironmentVariable", key, value);
}

}

