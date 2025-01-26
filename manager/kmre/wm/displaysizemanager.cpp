/*
 * Copyright (c) KylinSoft Co., Ltd. 2016-2024.All rights reserved.
 *
 * Authors:
 *  Ma Chao    machao@kylinos.cn
 *  Alan Xie   xiehuijun@kylinos.cn
 *  Clom       huangcailong@kylinos.cn
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

#include "displaysizemanager.h"

#include <QApplication>
#include <QScreen>
#include <QString>
#include <QSettings>
#include <QFile>
#include <QGSettings>
#include <QDebug>

#include <unistd.h>
#include <sys/syslog.h>

#include "dbusclient.h"
#include "utils.h"

#define DEFAULT_WINDOW_WIDTH 720//540
#define DEFAULT_WINDOW_HEIGHT 1280//960
#define DEFAULT_PHYSICAL_WIDTH 1920
#define DEFAULT_PHYSICAL_HEIGHT 1080
#define DEFAULT_VIRTUAL_DENSITY 160
#define DEFAULT_PHYSICAL_DENSITY 160
#define DEFAULT_DENSITY_240 240
#define DEFAULT_DENSITY_320 320
#define DEFAULT_DENSITY_360 360

#define WINDOW_DISPLAY_CONF "/usr/share/kmre/kmre-display.ini"

enum
{
    ASPECT_RATIO_TYPE_UNKNOWN = -1,
    ASPECT_RATIO_TYPE_16_9,
    ASPECT_RATIO_TYPE_16_10,
    ASPECT_RATIO_TYPE_3_2,
    ASPECT_RATIO_TYPE_4_3,
    ASPECT_RATIO_TYPE_5_4,
};

QSize DisplaySizeManager::mVirtualDisplaySize(0, 0);
QSize DisplaySizeManager::mPhysicalDisplaySize(0, 0);
int DisplaySizeManager::mVirtualDensity = 0;
int DisplaySizeManager::mPhysicalDensity = 0;
int DisplaySizeManager::mQemuDensity = 0;

static QSize oldVirtualDisplaySize(0, 0);
static QSize oldPhysicalDisplaySize(0, 0);

using namespace kmre;

QSize getWindowDisplaySizeFromDBus()
{
    QString windowWidthStr;
    QString windowHeightStr;
    int width;
    int height;

    /* From running container property */
    windowWidthStr = DBusClient::getInstance()->GetPropOfContainer(Utils::getUserName(), getuid(), "ro.kmre.window_display.width");
    windowHeightStr = DBusClient::getInstance()->GetPropOfContainer(Utils::getUserName(), getuid(), "ro.kmre.window_display.height");

    width = windowWidthStr.toInt();
    height = windowHeightStr.toInt();

    if (width > 0 && height > 0) {
        return QSize(width, height);
    }

    /* From container default property */

    return QSize(0, 0);
}

QSize getPhysicalDisplaySizeFromDBus()
{
    QString phyWidthStr;
    QString phyHeightStr;

    int width;
    int height;

    phyWidthStr = DBusClient::getInstance()->GetPropOfContainer(Utils::getUserName(), getuid(), "ro.kmre.physical_display.width");
    phyHeightStr = DBusClient::getInstance()->GetPropOfContainer(Utils::getUserName(), getuid(), "ro.kmre.physical_display.height");

    width = phyWidthStr.toInt();
    height = phyHeightStr.toInt();

    if (width > 0 && height > 0) {
        return QSize(width, height);
    }

    return QSize(0, 0);
}

static int getAspectRatioType(int width, int height)
{
    int ratioType = ASPECT_RATIO_TYPE_UNKNOWN;

    if (width < 1024 || height < 720) {
        return ASPECT_RATIO_TYPE_UNKNOWN;
    }

    if (width == 3840 && height == 2160) {
        ratioType = ASPECT_RATIO_TYPE_16_9;
    } else if (width == 2560 && height == 1440) {
        ratioType = ASPECT_RATIO_TYPE_16_9;
    } else if (width == 1920 && height == 1200) {
        ratioType = ASPECT_RATIO_TYPE_16_10;
    } else if (width == 1920 && height == 1080) {
        ratioType = ASPECT_RATIO_TYPE_16_9;
    } else if (width == 1680 && height == 1050) {
        ratioType = ASPECT_RATIO_TYPE_16_10;
    } else if (width == 1600 && height == 1024) {
        ratioType = ASPECT_RATIO_TYPE_3_2;
    } else if (width == 1400 && height == 1050) {
        ratioType = ASPECT_RATIO_TYPE_4_3;
    } else if (width == 1600 && height == 900) {
        ratioType = ASPECT_RATIO_TYPE_16_9;
    } else if (width == 1280 && height == 1024) {
        ratioType = ASPECT_RATIO_TYPE_5_4;
    } else if (width == 1440 && height == 900) {
        ratioType = ASPECT_RATIO_TYPE_16_10;
    } else if (width == 1400 && height == 900) {
        ratioType = ASPECT_RATIO_TYPE_3_2;
    } else if (width == 1280 && height == 960) {
        ratioType = ASPECT_RATIO_TYPE_4_3;//ASPECT_RATIO_TYPE_3_2
    } else if (width == 1440 && height == 810) {
        ratioType = ASPECT_RATIO_TYPE_16_9;
    } else if (width == 1368 && height == 768) {
        ratioType = ASPECT_RATIO_TYPE_16_9;
    } else if (width == 1366 && height == 768) {
        ratioType = ASPECT_RATIO_TYPE_16_9;
    } else if (width == 1360 && height == 768) {
        ratioType = ASPECT_RATIO_TYPE_16_9;
    } else if (width == 1280 && height == 800) {
        ratioType = ASPECT_RATIO_TYPE_16_10;
    } else if (width == 1152 && height == 864) {
        ratioType = ASPECT_RATIO_TYPE_4_3;
    } else if (width == 1280 && height == 720) {
        ratioType = ASPECT_RATIO_TYPE_16_9;
    } else if (width == 1024 && height == 768) {
        ratioType = ASPECT_RATIO_TYPE_4_3;
    } else if (width == 960 && height == 720) {
        ratioType = ASPECT_RATIO_TYPE_4_3;
    }

    return ratioType;
}

static QSize calculateWindowDisplaySize(int width, int height)
{
    QSize screenSize = QSize(width, height);
    QSize winSize = QSize(720, 1280);

    if ((screenSize == QSize(1920, 1080)) || 
        (screenSize == QSize(1680, 1050)) || 
        (screenSize == QSize(1600, 900))  || 
        (screenSize == QSize(1440, 900))  || 
        (screenSize == QSize(1600, 1024))) {
        winSize = QSize(720, 1280);
    }
    else if (width == 1280 && height == 1024) {
        winSize = QSize(720, 900);
    }
    else if ((screenSize == QSize(1366, 768)) || 
        (screenSize == QSize(1280, 960)) || 
        (screenSize == QSize(1280, 800)) || 
        (screenSize == QSize(1280, 720)) || 
        (screenSize == QSize(1440, 810)) || 
        (screenSize == QSize(1368, 768)) || 
        (screenSize == QSize(1360, 768)) || 
        (screenSize == QSize(1400, 900))) {
        winSize = QSize(540, 960);
    }
    else if ((screenSize == QSize(1024, 768)) || 
        (screenSize == QSize(1400, 1050)) || 
        (screenSize == QSize(1152, 864))) {
        winSize = QSize(540, 720);
    }
    else {
        if (width < 1024 || height < 720) {
            winSize = QSize(540, 960);
        }
        else if (width > 1920 && height > 1080) {
            winSize = QSize(720, 1280);
        }

    }

    syslog(LOG_DEBUG, "[%s] Window display width = %d, height = %d", 
        __func__, winSize.width(), winSize.height());

    return winSize;
}

static qreal getScreenScalingFactor()
{
    //qreal screenScalingFactor = 1.0;

    qreal screenScalingFactor = QApplication::primaryScreen()->devicePixelRatio();
    // Kylin 的 schemas
    if (QGSettings::isSchemaInstalled("org.ukui.SettingsDaemon.plugins.xsettings")) {
        QGSettings scaleSettings("org.ukui.SettingsDaemon.plugins.xsettings", "/org/ukui/settings-daemon/plugins/xsettings/");
        if (scaleSettings.keys().contains("scalingFactor")) {
            screenScalingFactor = scaleSettings.get("scaling-factor").toDouble();
        }
        if (screenScalingFactor <= 0.0) {
            screenScalingFactor = 1.0;
        }
    }
    // DDE/GXDE 的 schemas
    if (QGSettings::isSchemaInstalled("com.deepin.xsettings")) {
        QGSettings scaleSettings("com.deepin.xsettings", "/com/deepin/xsettings/");
        if (scaleSettings.keys().contains("scale-factor")) {
            screenScalingFactor = scaleSettings.get("scale-factor").toDouble();
        }
        if (screenScalingFactor <= 0.0) {
            screenScalingFactor = 1.0;
        }
    }
    return screenScalingFactor;
}

QSize DisplaySizeManager::getPreferedWindowDisplaySize()
{
    QSize phySize = getPhysicalDisplaySize();
    QSize winSize;

    if (phySize.width() > 0 && phySize.height() > 0) {
        // phySize的宽和高分别是显示器分辨率的高和宽
        winSize = calculateWindowDisplaySize(phySize.height(), phySize.width());
        if (winSize.width() >= 720 && winSize.height() >= 1280) {
            mQemuDensity = 320;
        }
        else {
            mQemuDensity = 240;
        }

    }
    else {
        mQemuDensity = 320;
        winSize = QSize(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
    }

    return winSize;
}

int DisplaySizeManager::getWindowDensity()
{
    int density = 0;

    if (mVirtualDensity > 0) {
        return mVirtualDensity;
    }

    if (QFile::exists(WINDOW_DISPLAY_CONF)) {
        QSettings *m_settings = nullptr;
        m_settings = new QSettings(WINDOW_DISPLAY_CONF, QSettings::IniFormat);
        m_settings->setIniCodec("UTF-8");
        m_settings->beginGroup("window");
        density = m_settings->value("window_density", DEFAULT_DENSITY_320).toInt();
        if (density < DEFAULT_DENSITY_320) {
            density = DEFAULT_DENSITY_320;
        }
        m_settings->endGroup();
        delete m_settings;
        m_settings = nullptr;
    }

    if (density > 0) {
        mVirtualDensity = density;
        return mVirtualDensity;
    }

    mVirtualDensity = DEFAULT_DENSITY_320;

    return mVirtualDensity;

}

int DisplaySizeManager::getPhysicalDensity()
{
    int density = 0;

    if (mPhysicalDensity > 0) {
        return mPhysicalDensity;
    }

    if (QFile::exists(WINDOW_DISPLAY_CONF)) {
        QSettings *m_settings = nullptr;
        m_settings = new QSettings(WINDOW_DISPLAY_CONF, QSettings::IniFormat);
        m_settings->setIniCodec("UTF-8");
        m_settings->beginGroup("window");
        density = m_settings->value("physical_density", DEFAULT_DENSITY_320).toInt();
        if (density < DEFAULT_DENSITY_320) {
            density = DEFAULT_DENSITY_320;
        }
        m_settings->endGroup();
        delete m_settings;
        m_settings = nullptr;
    }

    if (density > 0) {
        mPhysicalDensity = density;
        return mPhysicalDensity;
    }

    QSize phySize = getPhysicalDisplaySize();
    if (phySize.height() == 3840 || phySize.height() == 2560) {
        mPhysicalDensity = DEFAULT_DENSITY_360;
    }
    else {
        mPhysicalDensity = DEFAULT_DENSITY_320;
    }

    return mPhysicalDensity;
}


QSize DisplaySizeManager::getWindowDisplaySize()
{
    mVirtualDisplaySize = getPreferedWindowDisplaySize();

    return mVirtualDisplaySize;
}

QSize DisplaySizeManager::getPhysicalDisplaySize()
{
    QScreen* screen = QApplication::primaryScreen();

    if (!screen) {
        mPhysicalDisplaySize = QSize(DEFAULT_PHYSICAL_WIDTH, DEFAULT_PHYSICAL_HEIGHT);
    } else {
        qreal dpi = screen->devicePixelRatio();
        mPhysicalDisplaySize = screen->size() * dpi;
        mPhysicalDisplaySize = mPhysicalDisplaySize / getScreenScalingFactor();
    }

    /* Set physical size as a portrait orientation one,  now its orientation is the same as virtual display's. */
    if (mPhysicalDisplaySize.width() > mPhysicalDisplaySize.height()) {
        mPhysicalDisplaySize = QSize(mPhysicalDisplaySize.height(), mPhysicalDisplaySize.width());
    }

//out:
    updatePhysicalDisplaySize();
    syslog(LOG_DEBUG, "[%s] Physical display size: width = %d, height = %d", 
        __func__, mPhysicalDisplaySize.width(),  mPhysicalDisplaySize.height());

    return mPhysicalDisplaySize;
}


void DisplaySizeManager::updateVirtualDisplaySize()
{
    if (mVirtualDisplaySize.width() > 0 && mVirtualDisplaySize.height() > 0) {
        if (mVirtualDisplaySize != oldVirtualDisplaySize) {
            oldVirtualDisplaySize = mVirtualDisplaySize;
            DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.kmre.window_display.width", QString::number(mVirtualDisplaySize.width()));
            DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.kmre.window_display.height", QString::number(mVirtualDisplaySize.height()));
            DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.kmre.window_display.density", QString::number(mVirtualDensity));
            DBusClient::getInstance()->SetPropOfContainer(Utils::getUserName(), getuid(), "ro.kmre.window_display.width", QString::number(mVirtualDisplaySize.width()));
            DBusClient::getInstance()->SetPropOfContainer(Utils::getUserName(), getuid(), "ro.kmre.window_display.height", QString::number(mVirtualDisplaySize.height()));
            DBusClient::getInstance()->SetPropOfContainer(Utils::getUserName(), getuid(), "ro.kmre.window_display.density", QString::number(mVirtualDensity));
        }
    }
}

void DisplaySizeManager::updatePhysicalDisplaySize()
{
    if (mPhysicalDisplaySize.width() > 0 && mPhysicalDisplaySize.height() > 0) {
        if (mPhysicalDisplaySize != oldPhysicalDisplaySize) {
            oldPhysicalDisplaySize = mPhysicalDisplaySize;
            DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.kmre.physical_display.width", QString::number(mPhysicalDisplaySize.width()));
            DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.kmre.physical_display.height", QString::number(mPhysicalDisplaySize.height()));
            DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.kmre.physical_display.density", QString::number(mPhysicalDensity));
            /*if (mPhysicalDisplaySize.height() == 3840 || mPhysicalDisplaySize.height() == 2560) {
                DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "persist.sf.lcd_density", QString::number(320));
            }
            else {
                DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "persist.sf.lcd_density", QString::number(240));
            }*/
            DBusClient::getInstance()->SetPropOfContainer(Utils::getUserName(), getuid(), "ro.kmre.physical_display.width", QString::number(mPhysicalDisplaySize.width()));
            DBusClient::getInstance()->SetPropOfContainer(Utils::getUserName(), getuid(), "ro.kmre.physical_display.height", QString::number(mPhysicalDisplaySize.height()));
            DBusClient::getInstance()->SetPropOfContainer(Utils::getUserName(), getuid(), "ro.kmre.physical_display.density", QString::number(mPhysicalDensity));
        }
    }
}

int DisplaySizeManager::getQemuDensity()
{
    return mQemuDensity;
}
