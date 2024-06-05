/*
 * Copyright (c) KylinSoft Co., Ltd. 2016-2024.All rights reserved.
 *
 * Authors:
 *  Alan Xie    xiehuijun@kylinos.cn
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

#include "kmremanager.h"

#include <QApplication>
#include <QTimer>
#include <QProcess>
#include <QThread>
#include <QSessionManager>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QDebug>
#include <QScreen>
#include <QDir>
#include <sys/syslog.h>
#include <unistd.h>

#include "signalmanager.h"
#include "backendworker.h"
#include "camera_service.h"
#include "powermanager.h"
#include "utils.h"
#include "controlmanager.h"
#include "kmre/wm/displaysizemanager.h"
#include "kmre/wm/displaytypemanager.h"
#include "kmre/wm/gpuinfo.h"
#include "dbusclient.h"
#include "metatypes.h"


KmreManager::KmreManager()
{
    qRegisterMetaType<AndroidMeta>("AndroidMeta");
    qDBusRegisterMetaType<QList<QByteArray>>();
    qDBusRegisterMetaType<AndroidMeta>();
    qDBusRegisterMetaType<AndroidMetaList>();

    kmre::DBusClient::getInstance()->Prepare(Utils::getUserName(), getuid());
    mSignalManager = SignalManager::getInstance();
    mControlManager = ControlManager::getInstance();

    initCameraServiceThread();
    initDisplayInformation();
    initCpuInformation();
    setAppropriateQemuDensity();
    initHostPlatformProp();
    initDockerDns();

    mPowerManager = PowerManager::getInstance();

    initConnections();
}

KmreManager::~KmreManager()
{
    uninhibit();
}

// host support dynamic display size ?
bool KmreManager::isHostSupportDDS()
{
#ifdef UKUI_WAYLAND
    return false;
#else

    GpuVendor gpuVendor = DisplayTypeManager::getGpuVendor();
    if ((gpuVendor == NVIDIA_VGA) ||
        (gpuVendor == AMD_VGA) ||
        (gpuVendor == INTEL_VGA)) {
        return true;
    }

    return false;
#endif
}

void KmreManager::initHostPlatformProp()
{
    QString hwPlatform = Utils::getHWPlatformString();
    if(!hwPlatform.isEmpty()) {
        kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(
            Utils::getUserName(), getuid(), "ro.kmre.host.platform", hwPlatform);
    }

    kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(
        Utils::getUserName(), getuid(), "ro.kmre.host.support_dds", isHostSupportDDS() ? "true" : "false");

}

void KmreManager::initCameraServiceThread()
{
    mCameraServiceWorker = new CameraService;
    mCameraThread = new QThread(this);
    mCameraServiceWorker->moveToThread(mCameraThread);
    connect(mCameraThread, &QThread::finished, mCameraServiceWorker, &CameraService::deleteLater);
    mCameraThread->start();    
    connect(mCameraServiceWorker, &CameraService::sigCurrentCameraDevice, this, &KmreManager::currentCameraDevice);
    connect(this, &KmreManager::requestRunningCamreService, mCameraServiceWorker, &CameraService::startWorker);
    connect(mCameraThread, &QThread::started, this, [=] () {
        emit this->requestRunningCamreService();
    });
}

void KmreManager::initConnections()
{
    connect(qApp, SIGNAL(commitDataRequest(QSessionManager&)), 
        SLOT(commitData(QSessionManager&)), Qt::DirectConnection);

    QScreen *screen = QApplication::primaryScreen();
    Q_ASSERT(screen);
    connect(screen, &QScreen::geometryChanged, this, [=] (const QRect &geometry) {
        Q_UNUSED(geometry)
        this->setAppropriateQemuDensity();
    });

    // 容器环境已关闭
    if (!QDBusConnection::systemBus().connect("cn.kylinos.Kmre", "/cn/kylinos/Kmre", "cn.kylinos.Kmre",
            "Stopped", this, SLOT(onStopApplication(QString)))) {
        syslog(LOG_ERR, "connect 'Stopped' signal failed!");
    }
    //V10 Pro: 当收到系统注销的信号后,执行关闭docker容器的操作
    if (!QDBusConnection::sessionBus().connect("org.gnome.SessionManager", "/org/gnome/SessionManager", "org.gnome.SessionManager",
            "StartLogout", this, SLOT(onStartLogout()))) {
        syslog(LOG_ERR, "connect 'StartLogout' signal failed!");
    }
    
    syslog(LOG_DEBUG, "Connecting window signals...");
    if (!QDBusConnection::sessionBus().connect("cn.kylinos.Kmre.Window", "/cn/kylinos/Kmre/Window", "cn.kylinos.Kmre.Window", 
            "containerEnvBooted", this, SLOT(onContainerEnvBooted(bool, QString)))) {
        syslog(LOG_ERR, "connect 'containerEnvBooted' signal failed!");
    }
    if (!QDBusConnection::sessionBus().connect("cn.kylinos.Kmre.Window", "/cn/kylinos/Kmre/Window", "cn.kylinos.Kmre.Window", 
            "updateAppStatus", mControlManager, SLOT(onUpdateAppDesktopAndIcon(QString, int, int)))) {
        syslog(LOG_ERR, "connect 'updateAppStatus' signal failed!");
    }
    // 同步Android端文件，当type = 0时，表示接收的时所有安卓文件数目，android会进行多次发送，每次发送N条，如5条，直到所有数据发送完毕。  
    // totalNum: 需要接收的数据总条数    metas.length: 本次接收的数据总条数
    if (!QDBusConnection::sessionBus().connect("cn.kylinos.Kmre.Window", "/cn/kylinos/Kmre/Window", "cn.kylinos.Kmre.Window", 
            "syncFiles", mControlManager, SLOT(onSyncFiles(int, AndroidMetaList, int)))) {
        syslog(LOG_ERR, "connect 'syncFiles' signal failed!");
    }
}

void KmreManager::onStartLogout()
{
    syslog(LOG_DEBUG, "KmreManager: session manager allowsInteraction, ready to logout.");
    uninhibit();

    // autolaunch a dbus-daemon should be with a $DISPLAY for X11
    //QProcess::execute("dbus-send --print-reply --dest=cn.kylinos.Kmre.Pref /cn/kylinos/Kmre/Pref cn.kylinos.Kmre.Pref.quilt");
    if (Utils::isKmrePrefRunning()) {
        QDBusInterface interface("cn.kylinos.Kmre.Pref", "/cn/kylinos/Kmre/Pref", "cn.kylinos.Kmre.Pref", QDBusConnection::systemBus());
        interface.call("quilt");
    }

    QDBusMessage dm = QDBusMessage::createMethodCall("cn.kylinos.Kmre", "/cn/kylinos/Kmre", "cn.kylinos.Kmre", "StopContainer");
    dm << Utils::getUserName();
    dm << Utils::getUid().toInt();
    QDBusConnection::systemBus().call(dm);
}

// 退出程序
void KmreManager::onStopApplication(const QString &container)
{
    QString name = QString("kmre-%1-%2").arg(Utils::getUid()).arg(Utils::getUserName());
    if (name == container) {
        syslog(LOG_DEBUG, "[KmreManager] onStopApplication...");
        quit();
    }
}

void KmreManager::quit()
{
    uninhibit();
    exit(0);
    //QTimer::singleShot(0, QApplication::instance(), SLOT(quit()));
}

// 向android发送关闭app的请求
void KmreManager::closeApp(const QString &appName, const QString& pkgName, bool forceKill)
{
    mControlManager->closeApp(appName, pkgName, forceKill);
}

// 获取显示屏幕的相关信息，格式为json格式的字符串
QString KmreManager::getDisplayInformation()
{
    return mControlManager->getDisplayInformation();
}

// 获取虚拟盘尺寸和物理屏分辨率尺寸
void KmreManager::initDisplayInformation()
{
    QSize virtualDisplaySize;
    QSize physicalDisplaySize;

    virtualDisplaySize = DisplaySizeManager::getWindowDisplaySize();
    physicalDisplaySize = DisplaySizeManager::getPhysicalDisplaySize();

}

void KmreManager::initCpuInformation()
{
#if defined(__x86_64) || defined(__x86_64__) || defined(__amd64) || defined(__amd64__)
    /*
    CPU_TYPE_INTEL      ro.dalvik.vm.native.bridge=libhoudini.so
    CPU_TYPE_AMD        ro.dalvik.vm.native.bridge=libndk_translation.so
    CPU_TYPE_HYGON      ro.dalvik.vm.native.bridge=libndk_translation.so
    CPU_TYPE_ZHAOXIN    ro.dalvik.vm.native.bridge=libndk_translation.so
    */
    QString confName = QDir::homePath() + "/.config/kmre/kmre.ini";
    QString bridge;
    if (QFile::exists(confName)) {
        QSettings settings(confName, QSettings::IniFormat);
        settings.setIniCodec("UTF-8");
        settings.beginGroup("settings");
        bridge = settings.value("x86_arm_bridge", bridge).toString();
        settings.endGroup();
    }
    if (bridge == "houdini") {
        kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.dalvik.vm.native.bridge", "libhoudini.so");
    }
    else if (bridge == "ndk") {
        kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.dalvik.vm.native.bridge", "libndk_translation.so");
    }
    else {
        kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.dalvik.vm.native.bridge", "libhoudini.so");
    }
#endif
}

// 根据当前分辨率设置合适的qemu.sf.lcd_density
void KmreManager::setAppropriateQemuDensity()
{
    QSize virtualDisplaySize = DisplaySizeManager::getWindowDisplaySize();//根据当前分辨率计算合适的density
    Q_UNUSED(virtualDisplaySize);
    int qemuDensity = 0;
    qemuDensity = DisplaySizeManager::getQemuDensity();
    if (qemuDensity > 0) {//android属性设置在开了安全的情况下，耗时会增加3-4倍
        kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "qemu.sf.lcd_density", QString::number(qemuDensity));
        kmre::DBusClient::getInstance()->SetPropOfContainer(Utils::getUserName(), getuid(), "qemu.sf.lcd_density", QString::number(qemuDensity));
    }
}

void KmreManager::controlApp(int id, const QString &pkgName, int event_type, int event_value)
{
    mControlManager->controlApp(id, pkgName, event_type, event_value);
}

// 请求向android数据库增加一条记录
void KmreManager::addOneRecord(const QString &path, const QString &mime_type)
{
    mControlManager->addOneRecord(path, mime_type);
}

// 请求从android数据库删除一条记录
void KmreManager::removeOneRecord(const QString &path, const QString &mime_type)
{
    mControlManager->removeOneRecord(path, mime_type);
}

void KmreManager::commandToGetAllFiles(int type)
{
    mControlManager->commandToGetAllFiles(type);
}

AndroidMetaList KmreManager::getAllFiles(const QString &uri, bool reverse_order)
{
    return mControlManager->getAllFiles(uri, reverse_order);
}

bool KmreManager::filesIsEmpty()
{
    return mControlManager->filesIsEmpty();
}

void KmreManager::setSystemProp(int type, const QString &propName, const QString &propValue)
{
    mControlManager->setSystemProp(type, propName, propValue);
}

QString KmreManager::getSystemProp(int type, const QString &propName)
{
    return mControlManager->getSystemProp(type, propName);
}

// bool KmreManager::installApp(const QString &filePath, const QString &pkgName, 
//                                 const QString &appName, const QString &appName_ZH)
// {
//     return mControlManager->installApp(filePath, pkgName, appName, appName_ZH);
// }

// bool KmreManager::uninstallApp(const QString &pkgName)
// {
//     return mControlManager->uninstallApp(pkgName);
// }

// 设置摄像头设备
void KmreManager::setCameraDevice(const QString &device)
{
    if (mCameraServiceWorker) {
        mCameraServiceWorker->setCameraDevice(device);
    }
}

// 获取当前使用的设置摄像头设备
QString KmreManager::getCameraDevice()
{
    if (mCameraServiceWorker) {
        return mCameraServiceWorker->getCameraDevice();
    }

    return "/dev/video0";
}

void KmreManager::commitData(QSessionManager &manager)
{
    if (manager.allowsInteraction()) {
        syslog(LOG_DEBUG, "[KmreManager] commitData...");
        onStartLogout();
        uninhibit();
        // The container maybe already stopped, and all other services had already processed 'Stopped' signal (exit service), so must not stop again!
        //stopServices();
        manager.release();
        //manager.cancel();
        exit(0);
        
    } else {
        // we did not get permission to interact, then
        // do something reasonable instead
    }
}
// If container maybe already stopped, do not call this method!
void KmreManager::stopServices()
{
    syslog(LOG_INFO, "[KmreManager] stopServices...");
    QDBusInterface appStream("cn.kylinos.Kmre.Window", "/cn/kylinos/Kmre/Window", "cn.kylinos.Kmre.Window", QDBusConnection::sessionBus());
    appStream.call("stop");
    QDBusInterface audio("cn.kylinos.Kmre.Audio", "/cn/kylinos/Kmre/Audio", "cn.kylinos.Kmre.Audio", QDBusConnection::sessionBus());
    audio.call("stop");
    QDBusInterface fileWatcher("cn.kylinos.Kmre.FileWatcher", "/cn/kylinos/Kmre/FileWatcher", "cn.kylinos.Kmre.FileWatcher", QDBusConnection::sessionBus());
    fileWatcher.call("stop");
    QDBusInterface sensor("com.kylin.Kmre.sensor", "/", "com.kylin.Kmre.sensor", QDBusConnection::sessionBus());
    sensor.call("stop");
    QDBusInterface gps("com.kylin.Kmre.gpsserver", "/", "com.kylin.Kmre.gpsserver", QDBusConnection::sessionBus());
    gps.call("stop");
}

void KmreManager::initDockerDns()
{
    QStringList tmpDnsList;
    QStringList dnsList = Utils::getDnsListFromSysConfig();
    
    for (const auto &dns : dnsList) {
        syslog(LOG_DEBUG, "[%s] dns: '%s'", __func__, dns.toStdString().c_str());
        if (!dns.startsWith("127")) {
            tmpDnsList << dns;
        }
    }

    QString dns1 = "8.8.8.8";
    QString dns2 = "114.114.114.114";

    if (tmpDnsList.size() == 1) {
        dns1 = tmpDnsList.at(0);
    }
    else if (tmpDnsList.size() > 1) {
        dns1 = tmpDnsList.at(0);
        dns2 = tmpDnsList.at(1);
    }

    kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "sys.custom.dns1", dns1);
    kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "sys.custom.dns2", dns2);
}

//放开锁屏cookie
void KmreManager::uninhibit()
{
    QDBusInterface interface("org.gnome.SessionManager", 
                            "/org/gnome/SessionManager", 
                            "org.gnome.SessionManager", 
                            QDBusConnection::sessionBus());
    interface.call("Uninhibit", 0);
}

void KmreManager::onContainerEnvBooted(bool status, const QString &errInfo)
{
    static bool completed = false;
    if (completed) {
        return;
    }
    completed = true;
    
    if (status) {
        syslog(LOG_INFO, "[%s] Container Env boot successfully", __func__);
        mPowerManager->onUpdateBatteryState();
        mControlManager->updateAllAppNameInDesktop();
    }
    else {
        syslog(LOG_INFO, "[%s] Container Env boot failed! (error msg: '%s')", 
            __func__, errInfo.toStdString().c_str());
    }
}
