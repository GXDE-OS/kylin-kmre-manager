# Copyright (c) KylinSoft Co., Ltd. 2016-2024.All rights reserved.
#
# Authors:
#  Yuan ShanShan    yuanshanshan@kylinos.cn
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 3.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

TARGET = kylin-kmre-gps-server
TEMPLATE = app

QT -= gui
QT += dbus
CONFIG += c++11
CONFIG -= app_bundle

target.source += $$TARGET
target.path = /usr/bin
dbus_service.files += ../data/com.kylin.Kmre.gps.service
dbus_service.path = /usr/share/dbus-1/services/

SOURCES += main.cpp \
    common/socket/SocketStream.cpp \
    common/socket/UnixStream.cpp \
    common/utils/sockets.cpp \
    dbusadaptor.cpp \
    gpsdataget.cpp \
    kmregps.cpp \
    threadpool.cpp \
    utils.cpp

INCLUDEPATH += common/

HEADERS += \
    common/socket/SocketStream.h \
    common/socket/UnixStream.h \
    common/utils/sockets.h \
    dbusadaptor.h \
    gpsdataget.h \
    kmregps.h \
    myutils.h \
    socketstream.h \
    threadpool.h \
    utils.h

INSTALLS += target \
    dbus_service

unix {
    MOC_DIR = .moc
    OBJECTS_DIR = .obj
}
