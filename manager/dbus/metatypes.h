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

#ifndef DBUS_METATYPES_H_
#define DBUS_METATYPES_H_

#include <QMetaType>
#include <QMetaObject>
#include <QDBusMetaType>
#include <QDBusArgument>
#include <QString>
#include <QDebug>

//struct AndroidMeta {
//    QString path;
//    QString mimeType;
//};

class AndroidMeta
{
public:
    QString path;
    QString mimeType;
    QString pkgName;

    friend QDebug operator<<(QDebug argument, const AndroidMeta &data) {
        argument << data.path;
        return argument;
    }

    friend QDBusArgument& operator<<(QDBusArgument& arg, const AndroidMeta& meta) {
        arg.beginStructure();
        arg << meta.path << meta.mimeType << meta.pkgName;
        arg.endStructure();
        return arg;
    }

    friend const QDBusArgument& operator>>(const QDBusArgument& arg, AndroidMeta& meta) {
        arg.beginStructure();
        arg >> meta.path >> meta.mimeType >> meta.pkgName;
        arg.endStructure();
        return arg;
    }

    bool operator==(const AndroidMeta data) const {
        return data.path == path;
    }

    bool operator!=(const AndroidMeta data) const {
        return data.path != path;
    }
};

typedef QList<AndroidMeta> AndroidMetaList;
Q_DECLARE_METATYPE(AndroidMeta);
Q_DECLARE_METATYPE(AndroidMetaList);
Q_DECLARE_METATYPE(QList<QByteArray>)

#endif // DBUS_METATYPES_H_
