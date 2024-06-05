# Copyright (c) KylinSoft Co., Ltd. 2016-2024.All rights reserved.
#
# Authors:
#  Ma Chao    machao@kylinos.cn
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

TARGET = kylin-kmre-filewatcher
TEMPLATE = app

QT -= gui
QT += dbus core

CONFIG += c++11
CONFIG -= app_bundle
CONFIG += link_pkgconfig

PKGCONFIG += glib-2.0 gio-2.0 zlib

target.source += $$TARGET
target.path = /usr/bin
icon_files.files += icons/*
icon_files.path = /usr/share/kmre/icons/
dbus_service.files += ../data/cn.kylinos.Kmre.FileWatcher.service
dbus_service.path = /usr/share/dbus-1/services/

INSTALLS += target \
    dbus_service \
    icon_files

!system(/usr/bin/qdbusxml2cpp cn.kylinos.Kmre.FileWatcher.xml -a file_watcher_adaptor): error("Failed to generate qdbus source!")

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += main.cpp \
    file-inotify/dbus-client.cpp \
    file-inotify/file-inotify-service.cpp \
    file-inotify/file-inotify-watcher.cpp \
    file-inotify/utils.cpp \
    file-watcher.cpp \
    file_watcher_adaptor.cpp \
    directory-check-worker.cpp \
    storage-check-worker.cpp \
    custom.cpp \
    lock-file.cpp

HEADERS += \
    file-inotify/dbus-client.h \
    file-inotify/file-inotify-service.h \
    file-inotify/file-inotify-watcher.h \
    file-inotify/utils.h \
    file-watcher.h \
    file_watcher_adaptor.h \
    directory-check-worker.h \
    storage-check-worker.h \
    custom.h \
    lock-file.h

TRANSLATIONS += resources/translations/kylin-kmre-filewatcher_zh_CN.ts
!system($$PWD/resources/translations/generate_translations_pm.sh): error("Failed to generate pm")

RESOURCES += \
    resources/res.qrc

unix {
    MOC_DIR = .moc
    OBJECTS_DIR = .obj
    RCC_DIR = .rcc
}
