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

#include "utils.h"
#include "dbusclient.h"

#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/utsname.h>
#include <sys/syslog.h>

#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDomElement>
#include <QProcess>
#include <QDebug>

static int g_lockFd = -1;

//获取调用该程序的pw_shell
QString Utils::getShellProgram()
{
    QString shell = "";
    struct passwd pwd;
    struct passwd *result = nullptr;
    char *buf = nullptr;

    int bufSize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (bufSize == -1) {
        bufSize = 16384;
    }
    buf = new char[bufSize]();

    getpwuid_r(getuid(), &pwd, buf, bufSize, &result);
    if (result && pwd.pw_shell) {
        shell = pwd.pw_shell;
    }

    if (buf) {
        delete[] buf;
    }
    return shell;
}

/***********************************************************
   Function:       getUserName
   Description:    获取用户名
   Calls:
   Called By:
   Input:
   Output:
   Return:
   Others:
 ************************************************************/
const QString& Utils::getUserName()
{
    static QString userName = "";

    if (!userName.isEmpty()) {
        return userName;
    }

    struct passwd  pwd;
    struct passwd* result = 0;
    char buf[1024];

    memset(&buf, 0, sizeof(buf));
    uint32_t uid = getuid();
    (void)getpwuid_r(uid, &pwd, buf, 1024, &result);

    if (result && pwd.pw_name) {
        userName = pwd.pw_name;
    }
    else {
        try {
            userName = std::getenv("USER");
        } 
        catch (...) {
            try {
                userName = std::getenv("USERNAME");
            }
            catch (...) {
                syslog(LOG_ERR, "[%s] Get user name failed!", __func__);
                char name[16] = {0};
                snprintf(name, sizeof(name), "%u", getuid());
                userName = name;
            }
        }
    }

    userName.replace('\\', "_");// 在某些云环境，用户名中会有'\'字符，需将该字符转换为'_'字符
    return userName;
}

const QString& Utils::getUid()
{
    int uid = -1;
    static QString userId = "";

    if (!userId.isEmpty()) {
        return userId;
    }

    try {
        uid = getuid();
    }
    catch (...) {
        syslog(LOG_ERR, "[%s] Get user id failed!", __func__);
    }
    
    userId = QString::number(uid);
    return userId;
}

QString Utils::readFileContent(const QString &path)
{
    QFile file(path);
    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "Read file content failed to open" << path;
            return "";
        }

        //QTextStream ts(&file);
        //QString str = ts.readAll();
        const QString &str = QString::fromUtf8(file.readAll());
        file.close();

        return str;
    }

    return "";
}

bool Utils::copyFile(const QString &srcFile, const QString &destFile)
{
    if (removeFile(destFile)) {
        return QFile::copy(srcFile, destFile);
    }
    return false;
}

bool Utils::removeFile(const QString &file)
{
    QFile fp(file);
    if (fp.exists()) {
        if (!fp.remove()) {
            qWarning() << "Failed to remove:" << file;
            return false;
        }
    }
    return true;
}

bool Utils::ifFileContainInfo(char * filePath, char * info)
{
    char line[512] = {0};
    FILE* fp = NULL;

    fp = fopen(filePath, "r");
    if (!fp) {
        syslog(LOG_ERR, "Failed to read filePath %s.",filePath);
        return false;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strcasestr(line, info)) {
            fclose(fp);
            return true;
        }
    }

    fclose(fp);

    return false;
}

// 判断执行配置功能的dbus进程是否存在 (/usr/bin/kylin-kmre-preferences)
bool Utils::isKmrePrefRunning()
{
    QProcess p;
    p.start("ps", QStringList() << "-e" << "-o" << "comm");
    p.waitForFinished();
    const QString output = p.readAllStandardOutput();
    for (const auto &line : output.split('\n')) {
        if (line == "kylin-kmre-pref") {//15 bytes
            return true;
        }
    }

    return false;
}

QStringList Utils::getDnsListFromSysConfig()
{
    QFile file("/etc/resolv.conf");
    QStringList dnsList;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine();
            if ((!line.startsWith("#")) && line.contains("nameserver")) {
                QStringList dnsInfo = line.split("\t", QString::SkipEmptyParts);
                if (dnsInfo.size() == 2) {
                    QString dns = dnsInfo.at(1).trimmed();
                    if (dns.split(".", QString::SkipEmptyParts).size() == 4) {
                        dnsList << dns;
                    }
                }
                else {
                    dnsInfo = line.split(" ", QString::SkipEmptyParts);
                    if (dnsInfo.size() == 2) {
                        QString dns = dnsInfo.at(1).trimmed();
                        if (dns.split(".", QString::SkipEmptyParts).size() == 4) {
                            dnsList << dns;
                        }
                    }
                }
            }
        }
        file.close();
    }

    return std::move(dnsList);
}

#define COMMON_HW_PLATFORM_STR  "common"
#define COMMON_OS_PLATFORM_STR  "pc"

QString gHWPlatformString = "";
QString gOSPlatformString = "";

static void checkPlatform()
{
    gHWPlatformString = COMMON_HW_PLATFORM_STR;
    gOSPlatformString = COMMON_OS_PLATFORM_STR;

    syslog(LOG_DEBUG, "[%s] HW Platform:'%s', OS Platform:'%s'", 
        __func__, gHWPlatformString.toStdString().c_str(), gOSPlatformString.toStdString().c_str());
}

QString Utils::getHWPlatformString()
{
    if (gHWPlatformString.isEmpty()) {
        checkPlatform();
    }
    return gHWPlatformString;
}

void Utils::releaseLockFile()
{
    if (g_lockFd >= 0) {
        flock(g_lockFd, LOCK_UN);
        close(g_lockFd);
        g_lockFd = -1;
    }
}

bool Utils::tryLockFile(const QString& lockFilePath, int msecond)
{
    umask(0000);
    g_lockFd = open(lockFilePath.toStdString().c_str(), O_RDWR | O_CLOEXEC | O_CREAT, 0666);
    if (g_lockFd < 0) {
        syslog(LOG_ERR, "Failed to open lock file(%s)", lockFilePath.toStdString().c_str());
        return false;
    }

    if (flock(g_lockFd, LOCK_EX | LOCK_NB) != 0) {
        if (errno == EWOULDBLOCK) {
            syslog(LOG_ERR, " The lock file has been already locked by another process. Try again later.");
            usleep(msecond * 1000);
            if (flock(g_lockFd, LOCK_EX | LOCK_NB) == 0) {
                return true;
            }
        } 
        else {
            syslog(LOG_ERR, "Failed to lock file! errno = '%d'", errno);
        }
        return false;
    }

    return true;
}

QString Utils::execCmd(const QString &cmd, int msec)
{
    QByteArray data;
    QProcess process;
    QStringList options;

    options << "-c" << cmd;
    process.start("/bin/bash", options);
    process.waitForFinished(msec);
    process.waitForReadyRead(msec);
    data = process.readAllStandardOutput();

    return std::move(QString(data));
}

/**
 * @brief 获取操作系统宿主机的虚拟机类型
 * 
 * @return QString 获取失败返回空，获取成功返回一个字符串，字符串内容如下：
 *         [none, qemu, kvm, zvm, vmware, hyper-v, oracle virtualbox, xen, bochs, \
 *          uml, parallels, bhyve, qnx, arcn, openvz, lxc, lxc-libvirt, systemd-nspawn,\
 *          docker, podman, rkt, wsl]
 *         其中 none 表示运行在物理机环境中；其他字符串代表具体的虚拟环境类型。
 */
QString Utils::getHostVirtType()
{
    QString virtType;
    QStringList options;
    QProcess process;

    process.start("systemd-detect-virt", options, QIODevice::ReadOnly);
    if (process.waitForFinished(3000)) {
        process.waitForReadyRead();
        virtType = QString(process.readAllStandardOutput());
        virtType = virtType.remove("\n").trimmed();

        if (virtType == "microsoft") {
            virtType = "hyper-v";
        }
        else if (virtType == "oracle") {
            virtType = "oracle virtualbox";
        }
    }

    return virtType;
}

/**
 * @brief 获取操作系统宿主机的云平台类型
 * 
 * @return QString 返回空或者一个字符串。
 *                 其中空表示运行在物理机或未知的云平台环境中；其他字符串代表不同的云平台。
 */
QString Utils::getHostCloudPlatform()
{
    QString cloudPlatform;

    if (!access("/usr/local/ctyun/clink/Mirror/Registry/Default", F_OK)) {
        cloudPlatform = "ctyun";
    }

    if (geteuid() == 0) {// root 用户，可以用dmidecode
        QStringList options;
        QProcess process;

        if (cloudPlatform.isEmpty()) {
            options.clear();
            options << "-s chassis-manufacturer";
            process.start("dmidecode", options, QIODevice::ReadOnly);
            if (process.waitForFinished(2000)) {
                process.waitForReadyRead();
                QString output = QString(process.readAllStandardOutput());
                output = output.remove("\n").trimmed();

                if (output == "Huawei Inc.") {// 华为云
                    cloudPlatform = "huawei";
                }
            }
        }

        if (cloudPlatform.isEmpty()) {
            options.clear();
            options << "-s chassis-asset-tag";
            process.start("dmidecode", options, QIODevice::ReadOnly);
            if (process.waitForFinished(2000)) {
                process.waitForReadyRead();
                QString output = QString(process.readAllStandardOutput());
                output = output.remove("\n").trimmed();

                if (output == "HUAWEICLOUD") {// 华为云
                    cloudPlatform = "huawei";
                }
            }
        }
    }
    else { // 普通用户，只能读取文件
        if (cloudPlatform.isEmpty()) {
            QFile file("/sys/devices/virtual/dmi/id/chassis_vendor");
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QString data = QString(file.readAll());
                data = data.remove("\n").trimmed();

                if (data == "Huawei Inc.") {// 华为云
                    cloudPlatform = "huawei";
                }
                file.close();
            }
        }

        if (cloudPlatform.isEmpty()) {
            QFile file("/sys/devices/virtual/dmi/id/chassis_asset_tag");
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QString data = QString(file.readAll());
                data = data.remove("\n").trimmed();

                if (data == "HUAWEICLOUD") {// 华为云
                    cloudPlatform = "huawei";
                }
                file.close();
            }
        }
    }

    return cloudPlatform;
}
