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

#include "controlmanager.h"

#include <QApplication>
#include <QTimer>
#include <QProcess>
#include <QThread>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
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
#include "utils.h"
#include "kmremanager.h"
#include "kmre/wm/displaysizemanager.h"
#include "kmre/wm/displaytypemanager.h"
#include "kmre/wm/gpuinfo.h"
#include "dbusclient.h"
#include "metatypes.h"

#define FREEDESKTOP_UPOWER                  "org.freedesktop.DBus.Properties"
#define UPOWER_INTERFACE                    "org.freedesktop.UPower"
#define UPOWER_PATH                         "/org/freedesktop/UPower"
#define UPOWER_SERVICE                      "org.freedesktop.UPower"
#define UPOWER_DEVICE_SERVICE               "org.freedesktop.UPower.Device"


ControlManager::ControlManager()
{
    mSignalManager = SignalManager::getInstance();
    initBackendThread();
    initConnections();

}

ControlManager::~ControlManager()
{
    mBackendThread->quit();
    mBackendThread->wait();
}

void ControlManager::initBackendThread()
{
    mBackendWorker = new BackendWorker(Utils::getUserName(), Utils::getUid());
    mBackendThread = new QThread(this);
    mBackendWorker->moveToThread(mBackendThread);
    connect(mBackendThread, &QThread::finished, mBackendWorker, &BackendWorker::deleteLater);
    connect(mBackendThread, &QThread::finished, mBackendThread, &QThread::deleteLater);
    
    mBackendThread->start();
}

void ControlManager::initConnections()
{
    //
    connect(mSignalManager, &SignalManager::requestRunningAppList, mBackendWorker, &BackendWorker::getRunningAppList);    
    //
    connect(mBackendWorker, &BackendWorker::sendRunningAppList, this, [=] (const QByteArray &array) {
        Q_UNUSED(array);
    }, Qt::QueuedConnection);
    //点击任务栏app的图标后，切换app   &   绑定重启app端请求
    connect(mSignalManager, SIGNAL(requestLaunchApp(QString, bool, int, int, int)), mBackendWorker, SLOT(launchApp(QString, bool, int, int, int)));
    //
    connect(mBackendWorker, &BackendWorker::launchFinished, this, [=] (const QString &pkgName, bool result) {
        Q_UNUSED(pkgName);
        Q_UNUSED(result);
    }, Qt::QueuedConnection);
    //绑定关闭应用的请求
    connect(mSignalManager, &SignalManager::requestCloseApp, mBackendWorker, &BackendWorker::closeApp);
    //绑定安卓返回按钮点击的请求
    connect(mSignalManager, &SignalManager::requestControlApp, mBackendWorker, &BackendWorker::controlApp);
    //告诉安卓去暂停所有安卓应用
    connect(mSignalManager, &SignalManager::requestPauseAllAndroidApps, mBackendWorker, &BackendWorker::pasueAllApps);
    connect(mSignalManager, &SignalManager::requestCloseAllApps, this, [=] () {
        //
    });
    //绑定设置Android属性的请求
    connect(mSignalManager, &SignalManager::requestSetSystemProp, mBackendWorker, &BackendWorker::setSystemProp);
    //绑定向安卓数据库添加一条记录的请求
    connect(mSignalManager, &SignalManager::requestAddFileRecord, mBackendWorker, &BackendWorker::insertOneRecordToAndroidDB);
    //绑定从安卓数据库删除一条记录的请求
    connect(mSignalManager, &SignalManager::requestRemoveFileRecord, mBackendWorker, &BackendWorker::removeOneRecordFromAndroidDB);
    //
    connect(mSignalManager, &SignalManager::requestAllFiles, mBackendWorker, &BackendWorker::requestAllFilesFromAndroidDB);

}

QVector<AppInfo> ControlManager::getInstalledAppList()
{
    QVector<AppInfo> appInfoList;
    QString locale = QLocale::system().name();
    QByteArray array = mBackendWorker->getInstalledAppListJsonStr();
    QJsonParseError err;

    QJsonDocument document = QJsonDocument::fromJson(array, &err);
    if (err.error == QJsonParseError::NoError && !document.isNull() && !document.isEmpty() && document.isArray()) {
        QJsonArray jsonArray = document.array();
        foreach (QJsonValue val, jsonArray) {
            QJsonObject subObject = val.toObject();
            QString pkgName = subObject.value("package_name").toString();
            if ((pkgName != "com.antutu.benchmark.full") && 
                (pkgName != "com.antutu.benchmark.full.lite")) {//资源包不生成 desktop 文件

                QString appName = subObject.value("app_name").toString();
                QString version = subObject.value("version_name").toString();
                appInfoList.append({pkgName, appName, version});
            }
        }
    }
    return appInfoList;
}

AppInfo ControlManager::getAppInfo(QString pkgName)
{
    QVector<AppInfo> appInfoList = getInstalledAppList();
    for (auto appInfo : appInfoList) {
        if (appInfo.pkgName == pkgName) {
            return appInfo;
        }
    }
    return {"","",""};
}

void ControlManager::onUpdateAppDesktopAndIcon(const QString &pkgName, int status, int type)
{
    Q_UNUSED(type);
    syslog(LOG_DEBUG, "[%s] pkgName:'%s', status = %d", __func__, pkgName.toStdString().c_str(), status);

    if (status == 0) {// installed
        AppInfo appInfo = getAppInfo(pkgName);
        if (!appInfo.pkgName.isEmpty()) {
            mBackendWorker->updateDekstopAndIcon(appInfo);
        }
    }
    else if (status == 1) {// uninstalled
        mBackendWorker->removeDekstopAndIcon(pkgName);
    }
}

void ControlManager::updateAllAppNameInDesktop()
{
    QString locale = QLocale::system().name();
    syslog(LOG_DEBUG, "Update all app's name in desktop file. locale = '%s'", locale.toStdString().c_str());
    QDir desktopsDir(QDir::homePath() + "/.local/share/applications");
    if (!desktopsDir.exists()) {
        syslog(LOG_WARNING, "Desktop file directory isn't exist!");
        return;
    }

    QStringList fileType;
    fileType << "gxme-*.desktop";
    QStringList desktopFiles = desktopsDir.entryList(fileType, 
            QDir::Files | QDir::Readable | QDir::Writable, QDir::Name);
    QVector<AppInfo> appInfoList = getInstalledAppList();

    for (const auto& desktopFile : desktopFiles) {
        //syslog(LOG_DEBUG, "Update app name in desktop file: %s", desktopFile.toStdString().c_str());

        QString pkgName = desktopFile;
        pkgName.chop(QString(".desktop").length());
        pkgName.remove(0, QString("gxme-").length());
        QString appName = "";

        for (const auto& appInfo : appInfoList) {
            if (appInfo.pkgName == pkgName) {
                appName = appInfo.appName;
                break;
            }
        }

        //syslog(LOG_DEBUG, "App name: %s", appName.toStdString().c_str());
        if (appName.isEmpty()) {
            syslog(LOG_WARNING, "Can't find app info in app list, pkgName: %s", pkgName.toStdString().c_str());
            continue;
        }
    
        const QString desktopFilePath = desktopsDir.absoluteFilePath(desktopFile);
        //syslog(LOG_DEBUG, "Desktop file path: %s", desktopFilePath.toStdString().c_str());

        QFile file(desktopFilePath);
        if (!file.open(QIODevice::ReadWrite | QIODevice::Text)) {
            continue;
        }
        
        QTextStream stream(&file);
        QString newContent = "";
        QString line;

        stream.setCodec("UTF-8");
        while (!stream.atEnd()) {
            line = stream.readLine();
            if (line.startsWith("Name=")) {
                newContent.append("Name=" + appName + "\n");
            }
            else if (line.startsWith("Name[zh_CN]=")) {
                newContent.append("Name[zh_CN]=" + appName + "\n");
            }
            else if (line.startsWith("Comment=")) {
                newContent.append("Comment=" + appName + "\n");
            }
            else if (line.startsWith("Comment[zh_CN]=")) {
                newContent.append("Comment[zh_CN]=" + appName + "\n");
            }
            else {
                newContent.append(line + "\n");
            }
        }

        if (!newContent.isEmpty()) {
            //syslog(LOG_DEBUG, "Desktop file new content: %s", newContent.toStdString().c_str());
            file.resize(0);
            stream.seek(0);
            stream << newContent;
            stream.flush();
        }
        file.close();
    }
}

// 请求获取已运行的android应用列表
void ControlManager::getRunningAppList()
{
    emit mSignalManager->requestRunningAppList();
}

// 向android发送关闭app的请求
void ControlManager::closeApp(const QString &appName, const QString& pkgName, bool forceKill)
{
    Q_UNUSED(forceKill);
    // send appName to android to close and kill process.
    emit mSignalManager->requestCloseApp(appName, pkgName);
}

// 获取显示屏幕的相关信息，格式为json格式的字符串
QString ControlManager::getDisplayInformation()
{
    QString info;
    QJsonObject obj;
    QSize virtualDisplaySize;
    QSize physicalDisplaySize;
    int virtualDensity;
    int physicalDensity;
    DisplayTypeManager::DisplayType displayType;
    QString displayTypeName;
    GpuVendor gpuVendor;
    QString gpuVendorName;
    GpuModel gpuModel;
    QString gpuModelName;
    CpuType cpuType;
    QString cpuTypeName;
    QStringList displayTypes;
    bool supportsThreadedOpenGL;
    bool supportsOpenGLES31AEP;

    virtualDisplaySize = DisplaySizeManager::getWindowDisplaySize();
    physicalDisplaySize = DisplaySizeManager::getPhysicalDisplaySize();

    virtualDensity = DisplaySizeManager::getWindowDensity();
    physicalDensity = DisplaySizeManager::getPhysicalDensity();

    displayType = DisplayTypeManager::getDisplayType();
    displayTypeName = DisplayTypeManager::getDisplayTypeName(displayType);

    gpuVendor = DisplayTypeManager::getGpuVendor();
    gpuVendorName = DisplayTypeManager::getGpuVendorName(gpuVendor);

    gpuModel = DisplayTypeManager::getGpuModel();
    gpuModelName = DisplayTypeManager::getGpuModelName(gpuModel);

    cpuType = DisplayTypeManager::getCpuType();
    cpuTypeName = DisplayTypeManager::getCpuTypeName(cpuType);

    QList<DisplayTypeManager::DisplayType> types = DisplayTypeManager::getAccessableDisplayType();
    foreach (DisplayTypeManager::DisplayType t, types) {
        if (t == DisplayTypeManager::DisplayType::DISPLAY_TYPE_EMUGL) {
            displayTypes.append("emugl");
        }
        else if (t == DisplayTypeManager::DisplayType::DISPLAY_TYPE_DRM) {
            displayTypes.append("drm");
        }
    }

    supportsThreadedOpenGL = DisplayTypeManager::supportsThreadedOpenGL();
    supportsOpenGLES31AEP = DisplayTypeManager::supportsOpenGLES31AEP();

    obj.insert("window_display.width", QJsonValue(virtualDisplaySize.width()));
    obj.insert("window_display.height", QJsonValue(virtualDisplaySize.height()));
    obj.insert("physical_display.width", QJsonValue(physicalDisplaySize.width()));
    obj.insert("physical_display.height", QJsonValue(physicalDisplaySize.height()));
    obj.insert("window_display.density", QJsonValue(virtualDensity));
    obj.insert("physical_display.density", QJsonValue(physicalDensity));
    obj.insert("display_type", QJsonValue(displayTypeName));
    obj.insert("gpu_vendor", QJsonValue(gpuVendorName));
    obj.insert("gpu_model", QJsonValue(gpuModelName));
    obj.insert("gpu_supported", QJsonValue(DisplayTypeManager::isGpuSupported()));
    obj.insert("cpu_type", QJsonValue(cpuTypeName));
    obj.insert("cpu_supported", QJsonValue(DisplayTypeManager::isCpuSupported()));
    obj.insert("display_types", QJsonValue(displayTypes.join(",")));
    obj.insert("supports_threaded_opengl", QJsonValue(supportsThreadedOpenGL));
    obj.insert("supports_opengles_31_aep", QJsonValue(supportsOpenGLES31AEP));

    info = QString(QJsonDocument(obj).toJson());

    return info;
}

void ControlManager::controlApp(int id, const QString &pkgName, int event_type, int event_value)
{
    emit mSignalManager->requestControlApp(id, pkgName, event_type, event_value);
}

// 请求向android数据库增加一条记录
void ControlManager::addOneRecord(const QString &path, const QString &mime_type)
{
    emit mSignalManager->requestAddFileRecord(path, mime_type);
}

// 请求从android数据库删除一条记录
void ControlManager::removeOneRecord(const QString &path, const QString &mime_type)
{
    emit mSignalManager->requestRemoveFileRecord(path, mime_type);
}
/*
*   请求从android获取所有文件数据
    type 0: dump all mediafile info
        1: image files
        2: video files
        3: audio files
        4: document files
*/
void ControlManager::commandToGetAllFiles(int type)
{
    emit mSignalManager->requestAllFiles(type);
}

AndroidMetaList ControlManager::getAllFiles(const QString &uri, bool reverse_order)
{
    Q_UNUSED(reverse_order)

    if (uri == "kmre:///picture") {
        //if (reverse_order) {
        //    std::reverse(m_imageMetas.begin(), m_imageMetas.end());
        //}
        return m_imageMetas;
    }
    else if (uri == "kmre:///video") {
        return m_videoMetas;
    }
    else if (uri == "kmre:///audio") {
        return m_audioMetas;
    }
    else if (uri == "kmre:///document") {
        return m_documentMetas;
    }
    else if (uri == "kmre:///other") {
        return m_otherMetas;
    }

    return AndroidMetaList();

//    int index = 0;
//    int max_count = 5;
//    return m_imageMetas.mid(index, max_count);
}

bool ControlManager::filesIsEmpty()
{
    return (m_imageMetas.isEmpty() && 
            m_videoMetas.isEmpty() && 
            m_audioMetas.isEmpty() && 
            m_documentMetas.isEmpty() &&
            m_otherMetas.isEmpty());
}

void ControlManager::onSyncFiles(int type, AndroidMetaList metas, int totalNum)
{
    /* type 0: dump all mediafile info
            1: insert file
            2: delete file
    */
    if (type == 0) {
        m_totalNum = totalNum;
        if (m_currentNum == 0) {//第一次接收安卓总列表数据，先清空变量
            m_imageMetas.clear();
            m_videoMetas.clear();
            m_audioMetas.clear();
            m_documentMetas.clear();
            m_otherMetas.clear();
        }
        m_currentNum += metas.length();
    }

    foreach (AndroidMeta meta, metas) {
        if (type == 0 || type == 1) {//type=0: all datas   type=1: add data
            if (meta.mimeType.indexOf("image") != -1) {
                if (!m_imageMetas.contains(meta)) {
                    m_imageMetas << meta;
                }
            }
            else if (meta.mimeType.indexOf("video") != -1) {
                if (!m_videoMetas.contains(meta)) {
                    m_videoMetas << meta;
                }
            }
            else if (meta.mimeType.indexOf("audio") != -1) {
                if (!m_audioMetas.contains(meta)) {
                    m_audioMetas << meta;
                }
            }
            else if ((meta.mimeType.indexOf("application") != -1) || 
                    (meta.mimeType.indexOf("text") != -1)) {
                if (!m_documentMetas.contains(meta)) {
                    m_documentMetas << meta;
                }
            }
            else {
                if (!m_otherMetas.contains(meta)) {
                    m_otherMetas << meta;
                }
            }

            if (type == 0) {
                if (m_currentNum >= m_totalNum) {//经过多次接收列表数据后，当接收到的列表数据总数与预计要接收的总数相同时，表示本轮获取总列表数据完成
                    emit KmreManager::getInstance()->filesMessage(type, metas);
                    m_totalNum = 0;
                    m_currentNum = 0;
                }
            }
            else {//type=1
                emit KmreManager::getInstance()->filesMessage(type, metas);
            }

            if ((!meta.pkgName.isEmpty()) && (!meta.mimeType.isEmpty())) {
                QString entryPath = QString("/var/lib/kmre/data/kmre-%1-%2/")
                                        .arg(Utils::getUid())
                                        .arg(Utils::getUserName());
                QString dataPath = entryPath + "/KmreData/";
                QString fileType = meta.mimeType.split("/", QString::SkipEmptyParts).front();
                if (fileType == "audio") 
                    fileType = "音乐";
                else if (fileType == "video") 
                    fileType = "视频";
                else if (fileType == "image") 
                    fileType = "图片";
                else if ((fileType == "application") || (fileType == "text")) 
                    fileType = "文档";
                else
                    fileType = "其他";
                
                QDir dir;
                QString fileName = meta.path.split("/", QString::SkipEmptyParts).back();
                QString filePath = meta.path.remove("/storage/emulated/0", Qt::CaseInsensitive);
                QString srcFilePath = entryPath + filePath;

                QString destPath = dataPath + meta.pkgName + "/" + fileType + "/";
                if (dir.mkpath(destPath)) {
                    QString newFilePath = destPath + fileName;
                    syslog(LOG_DEBUG, "[%s] link file: '%s' to new path: '%s'", 
                            __func__, srcFilePath.toStdString().c_str(), newFilePath.toStdString().c_str());
                    if (link(srcFilePath.toStdString().c_str(), newFilePath.toStdString().c_str()) != 0) {
                        syslog(LOG_ERR, "[%s] link file '%s' failed!", __func__, newFilePath.toStdString().c_str());
                    }
                }
                else {
                    syslog(LOG_ERR, "[%s] Make path '%s' failed!", __func__, destPath.toStdString().c_str());
                }

                // -------------------------
                QString destPath2 = dataPath + fileType + "/" + meta.pkgName + "/";
                if (dir.mkpath(destPath2)) {
                    QString newFilePath = destPath2 + fileName;
                    syslog(LOG_DEBUG, "[%s] link file: '%s' to new path: '%s'", 
                            __func__, srcFilePath.toStdString().c_str(), newFilePath.toStdString().c_str());
                    if (link(srcFilePath.toStdString().c_str(), newFilePath.toStdString().c_str()) != 0) {
                        syslog(LOG_ERR, "[%s] link file '%s' failed!", __func__, newFilePath.toStdString().c_str());
                    }
                }
                else {
                    syslog(LOG_ERR, "[%s] Make path '%s' failed!", __func__, destPath2.toStdString().c_str());
                }
            }

        }
        else if (type == 2) {//remove data
            if (meta.mimeType.indexOf("image") != -1) {
                foreach (auto item, m_imageMetas) {
                    if (item.path == meta.path) {
                        m_imageMetas.removeOne(item);
                        break;
                    }
                }
            }
            else if (meta.mimeType.indexOf("video") != -1) {
                foreach (auto item, m_videoMetas) {
                    if (item.path == meta.path) {
                        m_videoMetas.removeOne(item);
                        break;
                    }
                }
            }
            else if (meta.mimeType.indexOf("audio") != -1) {
                foreach (auto item, m_audioMetas) {
                    if (item.path == meta.path) {
                        m_audioMetas.removeOne(item);
                        break;
                    }
                }
            }
            else if ((meta.mimeType.indexOf("application") != -1) ||
                    (meta.mimeType.indexOf("text") != -1)) {
                foreach (auto item, m_documentMetas) {
                    if (item.path == meta.path) {
                        m_documentMetas.removeOne(item);
                        break;
                    }
                }
            }
            else {
                foreach (auto item, m_otherMetas) {
                    if (item.path == meta.path) {
                        m_otherMetas.removeOne(item);
                        break;
                    }
                }
            }
            /*for (int i=0; i<m_imageMetas.size(); ++i) {
                if (m_imageMetas.at(i).path == meta.path) {
                    m_imageMetas.removeAt(i);
                    break;
                }
            }*/

            emit KmreManager::getInstance()->filesMessage(type, metas);

            if ((!meta.pkgName.isEmpty()) && (!meta.mimeType.isEmpty())) {
                QString entryPath = QString("/var/lib/kmre/data/kmre-%1-%2/")
                                        .arg(Utils::getUid())
                                        .arg(Utils::getUserName());
                QString dataPath = entryPath + "/KmreData/";

                QString fileType = meta.mimeType.split("/", QString::SkipEmptyParts).front();
                if (fileType == "audio") 
                    fileType = "音乐";
                else if (fileType == "video") 
                    fileType = "视频";
                else if (fileType == "image") 
                    fileType = "图片";
                else if ((fileType == "application") || (fileType == "text")) 
                    fileType = "文档";
                else
                    fileType = "其他";
                
                QString fileName = meta.path.split("/", QString::SkipEmptyParts).back();

                QString destPath = dataPath + meta.pkgName + "/" + fileType + "/";
                QString filePath = destPath + fileName;
                //syslog(LOG_DEBUG, "[%s] remove file: '%s'", __func__, filePath.toStdString().c_str());
                if (QFile(filePath).exists()) {
                    if (!QFile(filePath).remove()) {
                        syslog(LOG_ERR, "[%s] Remove file '%s' failed!", __func__, destPath.toStdString().c_str());
                    }
                }

                // -------------------------
                QString destPath2 = dataPath + fileType + "/" + meta.pkgName + "/";
                QString filePath2 = destPath2 + fileName;
                //syslog(LOG_DEBUG, "[%s] remove file: '%s'", __func__, filePath.toStdString().c_str());
                if (QFile(filePath2).exists()) {
                    if (!QFile(filePath2).remove()) {
                        syslog(LOG_ERR, "[%s] Remove file '%s' failed!", __func__, destPath2.toStdString().c_str());
                    }
                }
            }

        }
    }
}

void ControlManager::setSystemProp(int type, const QString &propName, const QString &propValue)
{
    emit mSignalManager->requestSetSystemProp(type, propName, propValue);
}

QString ControlManager::getSystemProp(int type, const QString &propName)
{
    return mBackendWorker->getSystemProp(type, propName);
}

bool ControlManager::installApp(const QString &filePath, const QString &pkgName, 
                                const QString &appName, const QString &appName_ZH)
{
    return mBackendWorker->installApp(filePath, pkgName, appName);
}

bool ControlManager::uninstallApp(const QString &pkgName)
{
    return mBackendWorker->uninstallApp(pkgName);
}

