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

QT += gui core dbus svg xml

TEMPLATE = app
TARGET = kylin-kmre-manager

target.source += $$TARGET
target.path = /usr/bin

dbus_service.files += ../data/cn.kylinos.Kmre.Manager.service
dbus_service.path = /usr/share/dbus-1/services/

INSTALLS += target \
    dbus_service

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

!system($$PWD/gen_proto_and_qdbus_src.sh): error("Failed to generate protobuf or qdbus source!")

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += c++14
CONFIG += qt warn_on
CONFIG += release
CONFIG += link_pkgconfig
PKGCONFIG += xcb xcb-dri3 libdrm gsettings-qt egl glesv2

INCLUDEPATH += /usr/include/boost/
LIBS += -lboost_system -lboost_filesystem -lboost_log -lboost_thread
LIBS += -ldl -rdynamic -lpciaccess

QMAKE_CPPFLAGS *= $(shell dpkg-buildflags --get CPPFLAGS)
QMAKE_CFLAGS   *= $(shell dpkg-buildflags --get CFLAGS)
QMAKE_CXXFLAGS *= $(shell dpkg-buildflags --get CXXFLAGS)
QMAKE_LFLAGS   *= $(shell dpkg-buildflags --get LDFLAGS)
QMAKE_CFLAGS_DEBUG += -g
QMAKE_CXXFLAGS *= -fpermissive

INCLUDEPATH += common/ communication/ camera/ dbus/ kmre/ network/ power/ manager/

SourceDir = ./
for(var, SourceDir) {
    SOURCES += $$files($$join(var, , , /*.c), true)
    SOURCES += $$files($$join(var, , , /*.cc), true)
    SOURCES += $$files($$join(var, , , /*.cpp), true)
    HEADERS += $$files($$join(var, , , /*.h), true)
}

# for wayland
#DEFINES += UKUI_WAYLAND

unix {
    MOC_DIR = $$PWD/.moc
    OBJECTS_DIR = $$PWD/.obj
}
