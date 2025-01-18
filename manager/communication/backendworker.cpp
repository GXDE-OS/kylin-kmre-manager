/*
 * Copyright (c) KylinSoft Co., Ltd. 2016-2024.All rights reserved.
 *
 * Authors:
 *  Kobe Lee    lixiang@kylinos.cn
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

#include "backendworker.h"
#include "utils.h"
#include "dbusclient.h"
#include "controlmanager.h"

#include <QApplication>
#include <QFile>
#include <QDir>
#include <QThread>
#include <QProcess>
#include <QDebug>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusInterface>
#include <QSvgGenerator>
#include <QImage>
#include <QPainter>
#include <functional>
#include <iostream>
#include <dlfcn.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/stat.h>

static bool convertPngToSvg(const QString &pngPath, const QString &svgPath)
{
    if (pngPath.isEmpty()) {
        return false;
    }
    if (!QFile::exists(pngPath)) {
        return false;
    }

    QFile fp(pngPath);
    fp.open(QIODevice::ReadOnly);
    QByteArray datas = fp.readAll();
    fp.close();

    QImage image;
    if (!image.loadFromData(datas)) {
        syslog(LOG_ERR, "[%s]Failed to load image from %s", __func__, qPrintable(pngPath));
        return false;
    }
    image = image.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QSvgGenerator generator;
    generator.setFileName(svgPath);
    generator.setSize(image.size());
    generator.setViewBox(QRect(0, 0, image.width(), image.height()));
    generator.setTitle("SVG");
    generator.setDescription("Kmre SVG File");
    QPainter painter;
    painter.begin(&generator);
    painter.drawImage(QPoint(0,0), image);
    painter.end();

    return true;
}

class GetFunction
{
public:
    GetFunction(QString name) {
        mName = name;
    }

    ~GetFunction() {
        if (mModuleHandle) {
            dlclose(mModuleHandle);
            mModuleHandle = nullptr;
        }
    }

    void* operator() () {
        do {
            if (!QFile::exists(mLibPath)) {
                break;
            }

            mModuleHandle = dlopen(mLibPath.toStdString().c_str(), RTLD_LAZY);
            if (!mModuleHandle) {
                break;
            }

            void* func = dlsym(mModuleHandle, mName.toStdString().c_str());
            if (dlerror() != nullptr) {
                break;
            }

            return func;
        }while(0);
        syslog(LOG_ERR, "Get function '%s' failed from library!", mName.toStdString().c_str());
        return nullptr;
    }

private:
    QString mName;
    void *mModuleHandle = nullptr;
    const QString mLibPath = "/usr/lib/libkmre.so";
};

BackendWorker::BackendWorker(const QString &userName, const QString &userId, QObject *parent) :
    QObject(parent)
  , m_loginUserName(userName)
  , m_loginUserId(userId)
{
}

BackendWorker::~BackendWorker()
{
}

void BackendWorker::closeApp(const QString &appName, const QString &pkgName)
{
    if (appName.isEmpty() && pkgName.isEmpty()) {
        syslog(LOG_ERR, "[%s] Close app failed! Invalid parameters!", __func__); 
        return;
    }

    bool ret = false;
    GetFunction  getFunction("close_app");
    void* close_app = getFunction();
    if (close_app) {
        ret = ((bool(*)(char*, char*))close_app)(
            const_cast<char *>(appName.toStdString().c_str()), 
            const_cast<char *>(pkgName.toStdString().c_str()));
    }

    emit this->closeFinished(pkgName, ret);
    syslog(ret ? LOG_DEBUG : LOG_ERR, "[%s] Close app %s", __func__, ret ? "succeed." : "failed!");
}

// 程序启动时获取android环境已启动的app列表,并对这些启动的app分别创建对应的隐藏框
void BackendWorker::getRunningAppList()
{
    char *appListStr = nullptr;
    GetFunction  getFunction("get_running_applist");
    void* get_running_applist = getFunction();
    if (get_running_applist) {
        appListStr = ((char *(*)())get_running_applist)();
    }
    
    if (appListStr) {
        QByteArray byteArray(appListStr, std::string(appListStr).length());
        emit sendRunningAppList(byteArray);
    }
    else {
        syslog(LOG_ERR, "[%s] Get running app list failed!", __func__);
    }
}

void BackendWorker::pasueAllApps()
{
    bool ret = false;
    GetFunction  getFunction("pause_all_apps");
    void* pause_all_apps = getFunction();
    if (pause_all_apps) {
        ret = ((bool(*)())pause_all_apps)();
    }

    if (!ret) {
        syslog(LOG_ERR, "[%s] Pause all apps failed!", __func__);
    }
}

// event_type   0：返回；1：音量增加；2：音量减小；3：亮度增大；4：亮度减小；5：切到后台；6：切到前台; 7:关闭应用；8：截图; 9：弹虚拟键盘, 10：开启录屏，11：关闭录屏
void BackendWorker::controlApp(int id, const QString &pkgName, int event_type, int event_value)
{
    bool ret = false;
    GetFunction  getFunction("control_app");
    void* control_app = getFunction();
    if (control_app) {
        ret = ((bool(*)(int, char *, int, int))control_app)(id, const_cast<char *>(pkgName.toStdString().c_str()), event_type, event_value);
    }

    if (!ret) {
        syslog(LOG_ERR, "[%s] Control app with event type(%d) failed!", __func__, event_type);
    }
}

void BackendWorker::insertOneRecordToAndroidDB(const QString &path, const QString &mime_type)
{
    if (path.isEmpty() || mime_type.isEmpty()) {
        syslog(LOG_ERR, "[%s] invalid parameter!", __func__);
        return;
    }

    bool ret = false;
    GetFunction  getFunction("insert_file");
    void* insert_file = getFunction();
    if (insert_file) {
        ret = ((bool(*)(char *, char *))insert_file)(
            const_cast<char *>(path.toStdString().c_str()), 
            const_cast<char *>(mime_type.toStdString().c_str()));
    }
    if (!ret) {
        syslog(LOG_ERR, "[%s] insert_file failed!", __func__);
    }
}

void BackendWorker::removeOneRecordFromAndroidDB(const QString &path, const QString &mime_type)
{
    if (path.isEmpty() || mime_type.isEmpty()) {
        syslog(LOG_ERR, "[%s] invalid parameter!", __func__);
        return;
    }

    bool ret = false;
    GetFunction  getFunction("remove_file");
    void* remove_file = getFunction();
    if (remove_file) {
        ret = ((bool(*)(char *, char *))remove_file)(
            const_cast<char *>(path.toStdString().c_str()), 
            const_cast<char *>(mime_type.toStdString().c_str()));
    }
    if (!ret) {
        syslog(LOG_ERR, "[%s] remove_file failed!", __func__);
    }
}

void BackendWorker::requestAllFilesFromAndroidDB(int type)
{
    bool ret = false;
    GetFunction  getFunction("request_media_files");
    void* request_media_files = getFunction();
    if (request_media_files) {
        ret = ((bool(*)(int))request_media_files)(type);
    }
    if (!ret) {
        syslog(LOG_ERR, "[%s] request_media_files failed!", __func__);
    }
}

bool BackendWorker::updateDekstopAndIcon(const AppInfo &appInfo)
{
    syslog(LOG_DEBUG, "[%s] pkgName:'%s', appName:'%s', version:'%s'", 
        __func__, appInfo.pkgName.toStdString().c_str(), appInfo.appName.toStdString().c_str(), 
        appInfo.version.toStdString().c_str());
    if (generateDesktop(appInfo)) {
        generateIcon(appInfo.pkgName);
        return true;
    }

    return false;
}

void BackendWorker::removeDekstopAndIcon(const QString &pkgName)
{
    //delete icon
    const QString &iconFile = QString("%1/.local/share/icons/%2.png").arg(QDir::homePath()).arg(pkgName);
    QFile fp(iconFile);
    if (fp.exists()) {
        if (!fp.remove()) {
            qCritical() << "Failed to remove icon file:" << iconFile;
        }
    }

    //delete desktop file
    const QString destDesktopFile = QString("%1/.local/share/applications/gxme-%2.desktop").arg(QDir::homePath()).arg(pkgName);
    QFile fp2(destDesktopFile);
    if (fp2.exists()) {
        if (!fp2.remove()) {
            qCritical() << "Failed to remove desktop file:" << destDesktopFile;
        }
    }   
}

bool BackendWorker::generateDesktop(const AppInfo &appInfo)
{
    if (appInfo.pkgName.isEmpty()) {
        return false;
    }

    const QString desktopsPath = QString("%1/.local/share/applications").arg(QDir::homePath());
    if (!QDir().exists(desktopsPath)) {
        QDir().mkdir(desktopsPath);
    }

    //generate desktop file
    // ~/.local/share/applications
    const QString destDesktopFile = desktopsPath + QString("/gxme-%1.desktop").arg(appInfo.pkgName);
    // QFile desktopFp(destDesktopFile);
    // if (desktopFp.exists()) {
    //     desktopFp.remove();
    // }
    
    QFile file(destDesktopFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream out(&file);
    out << "[Desktop Entry]\n"
        "Terminal=false\n"
        "Type=Application\n"
        "StartupNotify=false\n"
        "Keywords=Android;App;Apk\n"
        "Categories=Android;App;Apk\n"
        "X-GXDE-KMREAPP=true\n"
        "X-GXDE-KMRE-PKGNAME=" + appInfo.pkgName + "\n"
        "Name=" + appInfo.appName + "\n"
        "Name[zh_CN]=" + appInfo.appName + "\n"
        "Comment=" + appInfo.appName + "\n"
        "Comment[zh_CN]=" + appInfo.appName + "\n"
        "Exec=/usr/bin/startapp " + appInfo.pkgName + " " + appInfo.version + "\n"
        "Icon=" + QDir::homePath() + "/.local/share/icons/" + appInfo.pkgName + ".svg"
        << endl;
    
    out.flush();
    file.close();

    chmod(destDesktopFile.toStdString().c_str(), 0777);

    return true;
}

//copy icon file from /tmp
bool BackendWorker::generateIcon(const QString &pkgName)
{
    if (pkgName.isEmpty()) {
        return false;
    }

    const QString iconsPath = QString("%1/.local/share/icons").arg(QDir::homePath());
    if (!QDir().exists(iconsPath)) {
        QDir().mkdir(iconsPath);
    }

    QString realPkgName = pkgName;
    QString errMessage;

    const QString linuxSvgPath = QString("%1/%2.svg").arg(iconsPath).arg(pkgName);
    const QString containerIconPath = QString("/var/lib/kmre/kmre-%1-%2/data/local/icons/%3.png")
            .arg(m_loginUserId).arg(m_loginUserName).arg(realPkgName);
    //如果沒有从apk解压到icon，则在安装完成后从android目录讲对应apk的icon拷贝出来使用（注意：安装完成后，需要等待一些时间再做拷贝操作，因为测试发现android里面图片没有在安装完成后立即生成）
    QFile cfp(containerIconPath);
    if (cfp.exists()) {
        QFile svgFp(linuxSvgPath);
        if (svgFp.exists()) {
            svgFp.remove();
        }

        // covert png to svg
        if (convertPngToSvg(containerIconPath, linuxSvgPath)) {
            const QString readline = "<g id=\"移动数据角标_复制\">\
                                    <g opacity=\"0.05\"><circle cx=\"80\" cy=\"80\" r=\"15\"/></g>\
                                    <g opacity=\"0.08\"><circle cx=\"80\" cy=\"80\" r=\"15.33\"/></g>\
                                    <g opacity=\"0.08\"><circle cx=\"80\" cy=\"80\" r=\"15.67\"/></g>\
                                    <g opacity=\"0.08\"><circle cx=\"80\" cy=\"80\" r=\"16\"/></g>\
                                    <circle cx=\"80\" cy=\"80\" r=\"15\" fill=\"#fff\"/>\
                                    <path d=\"M83.94,72A1.05,1.05,0,0,1,85,73.06V86.94A1.05,1.05,0,0,1,83.94,88H76.06A1.05,1.05,0,0,1,75,86.94V73.06A1.05,1.05,0,0,1,76.06,72h7.88m0-1H76.06A2.06,2.06,0,0,0,74,73.06V86.94A2.06,2.06,0,0,0,76.06,89h7.88A2.06,2.06,0,0,0,86,86.94V73.06A2.06,2.06,0,0,0,83.94,71Z\" fill=\"#575757\"/>\
                                    <circle cx=\"80\" cy=\"85\" r=\"1\" fill=\"#575757\"/></g>";
            // set android flag to svg
            QString cmd1 = QString("sed -i '$i %1' %2").arg(readline).arg(linuxSvgPath);
            system(cmd1.toStdString().c_str());
            QString cmd2 = QString("sed -i 's/\"96\"/\"83\"/g' %1").arg(linuxSvgPath);
            system(cmd2.toStdString().c_str());
            QString cmd3 = QString("sed -i 's/\"128\"/\"83\"/g' %1").arg(linuxSvgPath);
            system(cmd3.toStdString().c_str());
            QString cmd4 = QString("sed -i 's/\"256\"/\"83\"/g' %1").arg(linuxSvgPath);
            system(cmd4.toStdString().c_str());
            QString cmd5 = QString("sed -i 's/viewBox=\"0 0 128 128\"/viewBox=\"0 0 96 96\"/g' %1").arg(linuxSvgPath);
            system(cmd5.toStdString().c_str());
            QString cmd6 = QString("sed -i 's/viewBox=\"0 0 256 256\"/viewBox=\"0 0 96 96\"/g' %1").arg(linuxSvgPath);
            system(cmd6.toStdString().c_str());

            return true;
        }
        else {
            errMessage = QString("convert png to svg failed!");
        }
    }
    else {
        errMessage = QString("icon file '%1' isn't exist!").arg(containerIconPath);
    }

    syslog(LOG_ERR, "[BackendWorker] generate app icon failed! error message: %s", errMessage.toStdString().c_str());
    return false;
}

QByteArray BackendWorker::getInstalledAppListJsonStr()
{
    char* appListStr = nullptr;
    GetFunction  getFunction("get_installed_applist");
    void* get_installed_applist = getFunction();
    if (get_installed_applist) {
        appListStr = ((char *(*)())get_installed_applist)();
    }

    if (appListStr) {
        QByteArray byteArray(appListStr, std::string(appListStr).length());
        return byteArray;
    }
    else {
        syslog(LOG_ERR, "[%s] Get installed app list failed!", __func__);
        return QString("[]").toUtf8();
    }
}

bool BackendWorker::installApp(const QString &filePath, const QString &pkgName, const QString &appName)
{
    if (filePath.isEmpty()) {
        syslog(LOG_ERR, "[%s] invalid parameter!", __func__);
        return false;
    }

    QFileInfo fi(filePath);
    if (!fi.exists()) {
        syslog(LOG_ERR, "[%s] file(%s) isn't exist!", __func__, filePath.toStdString().c_str());
        return false;
    }
    QString fileName = fi.fileName();
    QString destFile = QString("/var/lib/kmre/kmre-%1-%2/data/local/tmp/%3")
                            .arg(Utils::getUid()).arg(Utils::getUserName()).arg(fileName);
    if (!Utils::copyFile(filePath, destFile)) {
        syslog(LOG_ERR, "[%s] copy file(%s) failed!", __func__, filePath.toStdString().c_str());
        return false;
    } 

    bool ret = false;
    GetFunction  getFunction("install_app");
    void* install_app = getFunction();
    if (install_app) {
        //filename:apk名称，如 com.tencent.mm_8.0.0.apk
        //appname:应用名,如 微信
        //pkgname:包名，如 com.tencent.mm
        ret = ((bool(*)(char *, char*, char *))install_app)(
            const_cast<char *>(fileName.toStdString().c_str()), 
            const_cast<char *>(appName.toStdString().c_str()), 
            const_cast<char *>(pkgName.toStdString().c_str()));
    }
    if (!ret) {
        Utils::removeFile(destFile);
        syslog(LOG_ERR, "[%s] installApp failed!", __func__);
    }
    else {
        syslog(LOG_INFO, "[%s] installApp success", __func__);
    }

    return ret;
}

bool BackendWorker::uninstallApp(const QString &pkgName)
{
    if (pkgName.isEmpty()) {
        syslog(LOG_ERR, "[%s] invalid parameter!", __func__);
        return false;
    }

    int ret = -10;
    GetFunction  getFunction("uninstall_app");
    void* uninstall_app = getFunction();
    if (uninstall_app) {
        ret = ((int(*)(char *))uninstall_app)(const_cast<char *>(pkgName.toStdString().c_str()));
    }
    syslog(LOG_ERR, "[%s] uninstallApp %s! errno: %d", __func__, (ret == 0) ? "success" : "failed", ret);

    return (!ret);
}

//  设置Andriod属性
void BackendWorker::setSystemProp(int type, const QString &propName, const QString &propValue)
{
    if (propName.isEmpty() || propValue.isEmpty()) {
        syslog(LOG_ERR, "[%s] invalid parameter!", __func__);
        return;
    }

    bool ret = false;
    GetFunction  getFunction("set_system_prop");
    void* set_system_prop = getFunction();
    if (set_system_prop) {
        ret = ((bool(*)(int, char *, char *))set_system_prop)(
            type, 
            const_cast<char *>(propName.toStdString().c_str()), 
            const_cast<char *>(propValue.toStdString().c_str()));
    }
    if (!ret) {
        syslog(LOG_ERR, "[%s] setSystemProp '%s' failed!", __func__, propName.toStdString().c_str());
    }
}

// 获取Andriod属性
QString BackendWorker::getSystemProp(int type, const QString &propName)
{
    if (propName.isEmpty()) {
        syslog(LOG_ERR, "[%s] invalid parameter!", __func__);
        return "";
    }

    char* value = nullptr;
    GetFunction  getFunction("get_system_prop");
    void* get_system_prop = getFunction();
    if (get_system_prop) {
        value = ((char* (*)(int, char *))get_system_prop)(type, const_cast<char *>(propName.toStdString().c_str()));
    }
    if (!value) {
        syslog(LOG_ERR, "[%s] getSystemProp '%s' failed!", __func__, propName.toStdString().c_str());
    }

    return value ? value : "";
}

bool BackendWorker::hasEnvBootcompleted()
{
    QString boot_completed = kmre::DBusClient::getInstance()->GetPropOfContainer(Utils::getUserName(), getuid(), "sys.kmre.boot_completed");
    return (boot_completed == "1");
}
