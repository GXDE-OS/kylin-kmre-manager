/*
 * Copyright (c) KylinSoft Co., Ltd. 2016-2024.All rights reserved.
 *
 * Authors:
 *  Kobe Lee    lixiang@kylinos.cn
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

#include <QApplication>
#include <QtDBus>
#include <QFile>
#include <xcb/xcb.h>
#include <sys/syslog.h>

#include "kmre/wm/gpuinfo.h"
#include "kmremanager.h"
#include "kmre_manager.h"
#include "utils.h"

#define SERVICE_INTERFACE "cn.kylinos.Kmre.Manager"
#define SERVICE_PATH "/cn/kylinos/Kmre/Manager"
#define LOG_IDENT "KMRE_kylin-kmre-manager"

QString m_orgDisplayType;//声明显示类型的全局变量
const QString LockFilePath = QDir::temp().absoluteFilePath("kylin-kmre-manager.lock");
const QString EnvFilePath = QDir::temp().absoluteFilePath("kylin-kmre-env");

void initOrgDisplayType()
{
#if defined(__x86_64) || defined(__x86_64__) || defined(__amd64) || defined(__amd64__)
    QDBusInterface interface("cn.kylinos.Kmre", "/cn/kylinos/Kmre", "cn.kylinos.Kmre", QDBusConnection::systemBus());
    QDBusReply<QString> reply = interface.call("GetDefaultPropOfContainer", Utils::getUserName(), Utils::getUid().toInt(), "ro.hardware.egl");
    if (reply.isValid()) {
        m_orgDisplayType = reply.value();//定义显示类型的全局变量
    }
#endif
}

bool xcb_is_connected()
{
    int m_primaryScreenNumber = 0;
    xcb_connection_t *m_connection = nullptr;
    QString m_displayName = QString::fromLocal8Bit(qgetenv("DISPLAY").constData());
    if (m_displayName.isEmpty()) {
        m_displayName = ":0";
    }
    m_connection = xcb_connect(m_displayName.toStdString().c_str(), &m_primaryScreenNumber);//m_connection = xcb_connect(NULL, NULL);m_connection = QX11Info::connection();
    if (!m_connection || xcb_connection_has_error(m_connection)) {
        syslog(LOG_DEBUG, "XcbConnection: Could not connect to display %s", m_displayName.toStdString().c_str());
        if (m_connection) {
            xcb_disconnect(m_connection);
        }
        return false;
    }

    if (m_connection) {
        //xcb_flush(m_connection);
        xcb_disconnect(m_connection);
    }

    return true;
}

void set_and_save_env()
{
    QFile env_file(EnvFilePath);
    if (env_file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        env_file.setPermissions(QFileDevice::Permissions(0x6666));

        if (!qputenv("QT_QPA_PLATFORMTHEME", "ukui")) {
            syslog(LOG_ERR, "Set env 'QT_QPA_PLATFORMTHEME' failed!");
        }
        env_file.write("QT_QPA_PLATFORMTHEME=ukui\n");

        bool useSoftwareRender = false;
        QString virtType = Utils::getHostVirtType();
        if (!(virtType.isEmpty() || virtType == "none")) {// 虚拟机环境
            syslog(LOG_INFO, "Detected virtual machine.");
            useSoftwareRender = true;
        }

        if (useSoftwareRender) {
            syslog(LOG_INFO, "Using software render now!");
            #ifdef __x86_64__
            if (QFile::exists("/usr/lib/x86_64-linux-gnu/dri/swrast_dri.so")) {
                if (!qputenv("MESA_LOADER_DRIVER_OVERRIDE", "swrast")) {
                    syslog(LOG_ERR, "swrast Set env 'MESA_LOADER_DRIVER_OVERRIDE' failed!");
                }
                env_file.write("MESA_LOADER_DRIVER_OVERRIDE=swrast\n");
            } else {
                syslog(LOG_INFO, "lib dri no found :/usr/lib/x86_64-linux-gnu/dri/i965_dri.so!");
            }
            #endif
            #ifdef __aarch64__
            if (QFile::exists("/usr/lib/aarch64-linux-gnu/dri/swrast_dri.so")) {
                if (!qputenv("MESA_LOADER_DRIVER_OVERRIDE", "swrast")) {
                    syslog(LOG_ERR, "swrast Set env 'MESA_LOADER_DRIVER_OVERRIDE' failed!");
                }
                env_file.write("MESA_LOADER_DRIVER_OVERRIDE=swrast\n");
            } else {
                syslog(LOG_INFO, "lib dri no found :/usr/lib/aarch64-linux-gnu/dri/swrast_dri.so!");
            }            
            #endif
        }
        else if (GpuInfo::preferI965()) {
            if (QFile::exists("/usr/lib/x86_64-linux-gnu/dri/i965_dri.so")) {
                if (!qputenv("MESA_LOADER_DRIVER_OVERRIDE", "i965")) {
                    syslog(LOG_ERR, "i965 Set env 'MESA_LOADER_DRIVER_OVERRIDE' failed!");
                }
                env_file.write("MESA_LOADER_DRIVER_OVERRIDE=i965\n");
            } else {
                syslog(LOG_INFO, "lib dri no found :/usr/lib/x86_64-linux-gnu/dri/i965_dri.so!");
            }
        }
        
        if (GpuInfo::isIntelIrisGpu()) {
            if (!qputenv("KMRE_ENV_DRI_DRIVER", "iris")) {
                syslog(LOG_ERR, "iris Set env 'KMRE_ENV_DRI_DRIVER' failed!");
            }
            env_file.write("KMRE_ENV_DRI_DRIVER=iris\n");
        } 
        
        env_file.close();
    }
    else {
        syslog(LOG_ERR, "Can't open env file!");
    }
}

/***********************************************************
   Function:       main
   Description:    程序入口
   Calls:
   Called By:
   Input:
   Output:
   Return:
   Others:
 ************************************************************/
int main(int argc, char *argv[])
{
    openlog(LOG_IDENT, LOG_NDELAY | LOG_NOWAIT | LOG_PID, LOG_USER);
    if (!Utils::tryLockFile(LockFilePath, 100)) {
        syslog(LOG_INFO, "Process has already run.");
        return 0;
    }

    set_and_save_env();

    QString pwShell = Utils::getShellProgram();
    if (pwShell.isNull() || pwShell.isEmpty()) {
        syslog(LOG_DEBUG, "main: pwShell is empty.");
        return 1;
    }
    //避免一些systemd服务在图形还未启动时候调用该程序导致段错误
    if (pwShell == "/bin/false" || pwShell == "/usr/sbin/nologin") {
        syslog(LOG_DEBUG, "main: pwShell is empty.");
        return 1;
    }

    if (!xcb_is_connected()) {
        return 1;
    }

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("TianJin Kylin");
    QCoreApplication::setApplicationName("kylin-kmre-manager");
    QCoreApplication::setApplicationVersion("3.0");
    QGuiApplication::setFallbackSessionManagementEnabled(false);

    if (!QDBusConnection::sessionBus().isConnected()) {
        syslog(LOG_ERR, "Cannot connect to the D-Bus session bus.\n"
                        "To start it, run:\n"
                        "\teval `dbus-launch --auto-syntax`\n");
        return 1;
    }

    syslog(LOG_INFO, "kylin-kmre-manager is starting.");

    // set env
    if (QFile::exists("/var/lib/kmre/kmre-global-env")) {
        QSettings settings("/var/lib/kmre/kmre-global-env", QSettings::IniFormat);
        settings.setIniCodec("UTF-8");
        foreach (QString key, settings.allKeys()) {
            QString value = settings.value(key).toString();
            qputenv(key.toStdString().c_str(), value.toLatin1());//value.toLocal8Bit()
            //const QString env1 = QString::fromLocal8Bit(qgetenv("MESA_LOADER_DRIVER_OVERRIDE"));
        }
    }

    initOrgDisplayType();

    KmreManager* kmreManager = KmreManager::getInstance();
    //km.connect(&app, SIGNAL(aboutToQuit()), SIGNAL(aboutToQuit()));
    ManagerAdaptor *adapter = new ManagerAdaptor(kmreManager);
    Q_UNUSED(adapter)
    QDBusConnection connection = QDBusConnection::sessionBus();
    if (!connection.registerService(SERVICE_INTERFACE) || !connection.registerObject(SERVICE_PATH, kmreManager)) {
        //qDebug() << "Failed to register dbus service: " << connection.lastError().message();
        return 1;
    }

    return app.exec();
}
