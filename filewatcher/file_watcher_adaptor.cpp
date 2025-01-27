/*
 * This file was generated by qdbusxml2cpp version 0.8
 * Command line was: qdbusxml2cpp cn.kylinos.Kmre.FileWatcher.xml -a file_watcher_adaptor
 *
 * qdbusxml2cpp is Copyright (C) 2020 The Qt Company Ltd.
 *
 * This is an auto-generated file.
 * Do not edit! All changes made to it will be lost.
 */

#include "file_watcher_adaptor.h"
#include <QtCore/QMetaObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

/*
 * Implementation of adaptor class FileWatcherAdaptor
 */

FileWatcherAdaptor::FileWatcherAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    // constructor
    setAutoRelaySignals(true);
}

FileWatcherAdaptor::~FileWatcherAdaptor()
{
    // destructor
}

void FileWatcherAdaptor::start()
{
    // handle method call cn.kylinos.Kmre.FileWatcher.start
    QMetaObject::invokeMethod(parent(), "start");
}

void FileWatcherAdaptor::stop()
{
    // handle method call cn.kylinos.Kmre.FileWatcher.stop
    QMetaObject::invokeMethod(parent(), "stop");
}

