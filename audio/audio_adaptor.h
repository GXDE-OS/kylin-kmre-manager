/*
 * This file was generated by qdbusxml2cpp version 0.8
 * Command line was: qdbusxml2cpp cn.kylinos.Kmre.Audio.xml -a audio_adaptor
 *
 * qdbusxml2cpp is Copyright (C) 2020 The Qt Company Ltd.
 *
 * This is an auto-generated file.
 * This file may have been hand-edited. Look for HAND-EDIT comments
 * before re-generating it.
 */

#ifndef AUDIO_ADAPTOR_H
#define AUDIO_ADAPTOR_H

#include <QtCore/QObject>
#include <QtDBus/QtDBus>
QT_BEGIN_NAMESPACE
class QByteArray;
template<class T> class QList;
template<class Key, class Value> class QMap;
class QString;
class QStringList;
class QVariant;
QT_END_NAMESPACE

/*
 * Adaptor class for interface cn.kylinos.Kmre.Audio
 */
class AudioAdaptor: public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "cn.kylinos.Kmre.Audio")
    Q_CLASSINFO("D-Bus Introspection", ""
"  <interface name=\"cn.kylinos.Kmre.Audio\">\n"
"    <method name=\"start\"/>\n"
"    <method name=\"stop\"/>\n"
"  </interface>\n"
        "")
public:
    AudioAdaptor(QObject *parent);
    virtual ~AudioAdaptor();

public: // PROPERTIES
public Q_SLOTS: // METHODS
    void start();
    void stop();
Q_SIGNALS: // SIGNALS
};

#endif
