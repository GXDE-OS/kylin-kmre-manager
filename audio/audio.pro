# Copyright (c) KylinSoft Co., Ltd. 2016-2024.All rights reserved.
#
# Authors:
#  Kobe Lee    lixiang@kylinos.cn
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

TARGET = kylin-kmre-audio
TEMPLATE = app

QT -= gui

QT += multimedia core network dbus

CONFIG += c++14 console
CONFIG -= app_bundle

CONFIG += link_pkgconfig
PKGCONFIG += speexdsp

target.source += $$TARGET
target.path = /usr/bin
dbus_service.files += ../data/cn.kylinos.Kmre.Audio.service
dbus_service.path = /usr/share/dbus-1/services/

INSTALLS += target \
    dbus_service

!system(/usr/bin/qdbusxml2cpp cn.kylinos.Kmre.Audio.xml -a audio_adaptor): error("Failed to generate qdbus source!")

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

QMAKE_CPPFLAGS *= $(shell dpkg-buildflags --get CPPFLAGS)
QMAKE_CFLAGS   *= $(shell dpkg-buildflags --get CFLAGS)
QMAKE_CXXFLAGS *= $(shell dpkg-buildflags --get CXXFLAGS)
QMAKE_LFLAGS   *= $(shell dpkg-buildflags --get LDFLAGS)
QMAKE_CXXFLAGS *= -fpermissive
QMAKE_CXXFLAGS += -g

LIBS += -ldl -lasound

SOURCES += main.cpp \
    kmreaudio.cpp \
    audio_adaptor.cpp \
    threadpool.cpp \
    audioserver.cpp \
    playbackworker.cpp \
    recordingworker.cpp \
    utils.cpp \
    common/utils/sockets.cpp \
    common/socket/UnixStream.cpp \
    common/socket/SocketStream.cpp

HEADERS += \
    kmreaudio.h \
    audio_adaptor.h \
    threadpool.h \
    audioserver.h \
    playbackworker.h \
    recordingworker.h \
    utils.h \
    common/utils/thread.h \
    common/utils/sockets.h \
    common/socket/UnixStream.h \
    common/socket/SocketStream.h \
    common/include/IOStream.h \
    myutils.h

INCLUDEPATH += common
INCLUDEPATH += common/include

# for wayland
#DEFINES += UKUI_WAYLAND

unix {
    MOC_DIR = .moc
    OBJECTS_DIR = .obj
}
