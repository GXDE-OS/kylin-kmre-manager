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

#include "displaytypemanager.h"
#include "gpuinfo.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <QSettings>
#include <QStandardPaths>
#include <QFile>
#include <QProcess>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <fstream>

#include <string>
#include <sys/syslog.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <pciaccess.h>

#include "utils.h"
#include "dbusclient.h"

extern QString m_orgDisplayType;//声明显示类型的全局变量

#define CPUINFO_PATH "/proc/cpuinfo"
#define CONFIG_NO_GPU_AUTHENTICATION "CONFIG_NO_GPU_AUTHENTICATION=y"


#define DISPLAY_TYPE_DRM_STRING      "drm"
#define DISPLAY_TYPE_EMUGL_STRING    "emugl"
#define DISPLAY_TYPE_SW_STRING       "sw"

#define GRALLOC_MODULE_NAME_DRM      "drm"
#define GRALLOC_MODULE_NAME_GBM      "gbm"
#define GRALLOC_MODULE_NAME_INTEL    "intel"
#define GRALLOC_MODULE_NAME_EMUGL    "goldfish"
#define GRALLOC_MODULE_NAME_SW       "dri"

#define EGL_MODULE_NAME_DRM          "mesa"
#define EGL_MODULE_NAME_EMUGL        "emulation"
#define EGL_MODULE_NAME_SW           "swiftshader"

#define PCIC_DISPLAY    0x03
#define PCIS_DISPLAY_VGA        0x00
#define PCI_CLASS_DISPLAY_VGA           0x0300
#define PCI_CLASS_DISPLAY_DC           0x0380
#define PCI_CLASS_DISPLAY_3D           0x0302

static DisplayTypeManager::DisplayType oldDisplayType = DisplayTypeManager::DisplayType::DISPLAY_TYPE_UNKNOWN;

static bool mGpuCheckCompleted = false;
static GpuVendor mGpuVendorVGA = UNKNOWN_VGA;

inline int getCpuCoreCounts()
{
    //grep -c ^processor /proc/cpuinfo
    int cpuCounts = 0;

    QFile file("/proc/cpuinfo");
    if (file.open(QFile::ReadOnly)) {
        char buf[1024];
        qint64 lineLen = 0;
        do {
            lineLen = file.readLine(buf, sizeof(buf));
            QString s(buf);
            if (s.contains(':')) {
                QStringList list = s.split(':');
                if (list.size() == 2) {
                    if (list[0] == "processor" || list[0] == "processor\t") {
                        cpuCounts ++;
                    }
                }
            }
        } while (lineLen >= 0);

        file.close();
    }

    return cpuCounts;
}

static int cmd_get_result(const char *cmd, char *result, int maxlen)
{
    if (!result) {
        return -1;
    }

    if (!cmd || (strlen(cmd) == 0)) {// For fortify scan
        return -1;
    }

    FILE *pp = popen(cmd, "r");
    if (!pp) {
        printf("error, cannot popen cmd: %s\n", cmd);
        return -1;
    }

    int i = 0;
    char tmp[512] = {0};
    memset(tmp, 0, sizeof(tmp));

    while (fgets(tmp, sizeof(tmp), pp) != NULL) {
        if (tmp[strlen(tmp) - 1] == '\n') {
            tmp[strlen(tmp) - 1] = '\0';
        }
        if (strlen(tmp) == 0) {// For fortify scan
            continue;
        }
        //printf("%d.get return results: %s\n", i, tmp);
        strcpy(result + i, tmp);
        i += strlen(tmp);
        if (i >= maxlen) {
            printf("get enough results, return\n");
            break;
        }
        memset(tmp, 0, sizeof(tmp));
    }

    if (pp) {
        pclose(pp);
    }

    return i;
}

static bool directoryCheck(const char* path)
{
    struct stat sb;
    if (stat(path, &sb) < 0) {
        return false;
    }

    if (!S_ISDIR(sb.st_mode)) {
        return false;
    }

    return true;
}

static bool charDevCheck(const char* path)
{
    struct stat sb;
    if (stat(path, &sb) < 0) {
        return false;
    }

    if (!S_ISCHR(sb.st_mode)) {
        return false;
    }

    return true;

}

static bool driDeviceCheck()
{
    if (!directoryCheck("/dev/dri")) {
        return false;
    }

    if (!charDevCheck("/dev/dri/card0")) {
        return false;
    }

    if (!charDevCheck("/dev/dri/renderD128")) {
        return false;
    }

    return true;
}

static std::string getKernelRelease()
{
    std::string release;
    struct utsname un;

    memset(&un, 0, sizeof(un));

    if (uname(&un) == 0) {
        release = std::string(un.release);
    }



    return release;
}

static bool kernelHasConfigNoGpuAuthentication()
{
    std::string release;
    std::string configFilePath;
    std::string line;
    std::ifstream configFile;
    bool result = false;

    release = getKernelRelease();

    if (release.length() == 0 || release.empty()) {
        return result;
    }

    configFilePath = "/boot/config-" + release;

    configFile.open(configFilePath, std::ios::in);

    if (!configFile.is_open()) {
        return result;
    }

    while (std::getline(configFile, line)) {
        char* p = nullptr;
        if (line.length() == 0 || line.empty()) {
            continue;
        }
        p = (char*) line.c_str();

        if (strncmp(p, CONFIG_NO_GPU_AUTHENTICATION, strlen(CONFIG_NO_GPU_AUTHENTICATION)) == 0) {
            result = true;
            break;
        }
    }

    configFile.close();

    return result;
}

static GpuVendor IterateVgaPciDevice(struct pci_device *dev)
{
    // It's a VGA device
    uint32_t pci_class_display_type = dev->device_class & 0x00ffff00;
    if (pci_class_display_type == (PCI_CLASS_DISPLAY_VGA << 8) ||
            pci_class_display_type == (PCI_CLASS_DISPLAY_DC << 8) ||
            pci_class_display_type == (PCI_CLASS_DISPLAY_3D << 8)) {
        const char *dev_name;
        const char *vend_name;

        vend_name = pci_device_get_vendor_name(dev);
        dev_name = pci_device_get_device_name(dev);
        if (dev_name == NULL) {
            dev_name = "Device unknown";
        }
        //pci bus 0x0004 cardnum 0x00 function 0x00: vendor 0x0709 device 0x0001, class:0x30000, CardVendor  0x0709 card 0x0001, CLASS  0x03 0x00 0x00  REVISION 0x01
        /*printf("pci bus 0x%04x cardnum 0x%02x function 0x%02x:"
               " vendor 0x%04x device 0x%04x, class:0x%04x,"
               " CardVendor  0x%04x card 0x%04x, CLASS  0x%02x 0x%02x 0x%02x  REVISION 0x%02x\n",
               dev->bus,
               dev->dev,
               dev->func,
               dev->vendor_id,
               dev->device_id,
               dev->device_class,
               dev->subvendor_id,
               dev->subdevice_id,
               (dev->device_class >> 16) & 0x0ff,
               (dev->device_class >>  8) & 0x0ff,
               (dev->device_class >>  0) & 0x0ff,
               dev->revision);
        */

        char card_vendor_id[24] = {0};
        snprintf(card_vendor_id, sizeof(card_vendor_id), "0x%04x", dev->subvendor_id);
        // 通过vendor_id判断显卡类型
        if (dev->vendor_id == 0x1002) {
            return AMD_VGA;
        }
        else if (dev->vendor_id == 0x10de) {
            return NVIDIA_VGA;
        }
        else if (dev->vendor_id == 0x8086) {
            return INTEL_VGA;
        }

        // 如果通过vendor_id判断显卡类型出现问题，则继续通过vend_name和dev_name判断显卡类型
        if (vend_name != NULL) {
            //printf("vend_name: %s dev_name: %s\n", vend_name, dev_name);
            if (strcasestr(vend_name, "nvidia")) {
                return NVIDIA_VGA;
            }
            else if (strcasestr(vend_name, "AMD")) {
                return AMD_VGA;
            }
            else if (strcasestr(vend_name, "ATI")) {
                return AMD_VGA;
            }
            else if (strcasestr(vend_name, "Intel")) {//Integrated graphics card must check as the last one
                return INTEL_VGA;
            }
        }
        else {
            //printf("dev_name: %s\n", dev_name);
            if (strcasestr(dev_name, "nvidia")) {
                return NVIDIA_VGA;
            }
            else if (strcasestr(dev_name, "AMD")) {
                return AMD_VGA;
            }
            else if (strcasestr(dev_name, "ATI")) {
                return AMD_VGA;
            }
            else if (strcasestr(dev_name, "Intel")) {//Integrated graphics card must check as the last one
                return INTEL_VGA;
            }
        }
    }

    return UNKNOWN_VGA;
}

static bool fbMatched(const char* fbName)
{
    FILE* fp = NULL;
    char line[256] = {0};
    bool matched = false;

    fp = fopen("/proc/fb", "r");
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strcasestr(line, fbName)) {
            matched = true;
            break;
        }
    }

    fclose(fp);

    return matched;
}

static bool isVirtualGraphicCard()
{
    return (fbMatched("virtiodrmfb") || fbMatched("qxldrmfb"));
}

static bool isNvidiaGraphicCard()
{
    struct stat sb;
    if (stat("/dev/nvidiactl", &sb) != 0) {
        return false;
    }

    if (S_ISCHR(sb.st_mode)) {
        auto ret = Utils::execCmd("lspci -nnk | grep -i vga -A3 | grep 'in use'", 200);
        //syslog(LOG_DEBUG, "ret:'%s'", ret.toStdString().c_str());
        return ret.contains("nvidia");
    }

    return false;

}

static GpuVendor getGpuManufacture()
{
    if (mGpuCheckCompleted) {
        return mGpuVendorVGA;
    }

    GpuVendor nRet = UNKNOWN_VGA;

    if (isNvidiaGraphicCard()) {
        nRet = NVIDIA_VGA;
    }
    else {
        nRet = GpuInfo::getGpuVendorFromDrm();
    }

    // 通过 libpciaccess 判断显卡类型
    if (nRet == UNKNOWN_VGA) {
        struct pci_device_iterator *iter;
        struct pci_device *dev;
        int ret = pci_system_init();
        if (ret != 0) {
            printf("Couldn't initialize PCI system\n");
        }
        else {
            iter = pci_slot_match_iterator_create(NULL);

            while ((dev = pci_device_next(iter)) != NULL) {
                nRet = IterateVgaPciDevice(dev);
                if (nRet >= 0) {
                    break;
                }
            }

            pci_system_cleanup();
        }
    }
    mGpuCheckCompleted = true;
    mGpuVendorVGA = nRet;
    return nRet;
}

// 获取国产显卡的具体类型
static GpuModel getGpuModelType()
{
    struct pci_device_iterator *iter;
    struct pci_device *dev;
    GpuModel nRet = UNKNOWN_GPU;
    int ret;
    int gpu = -1;

    gpu = getGpuManufacture();

    if (gpu >= 0) {
        nRet = OTHER_GPU;
    }

    return nRet;
}

DisplayTypeManager::DisplayType getDisplayTypeFromDBus()
{
    QString grallocModule;

    grallocModule = kmre::DBusClient::getInstance()->GetPropOfContainer(Utils::getUserName(), getuid(), "ro.hardware.gralloc");

    if (grallocModule == QString(GRALLOC_MODULE_NAME_DRM)) {
        return DisplayTypeManager::DisplayType::DISPLAY_TYPE_DRM;
    } else if (grallocModule == QString(GRALLOC_MODULE_NAME_GBM)) {
        return DisplayTypeManager::DisplayType::DISPLAY_TYPE_DRM;
    } else if (grallocModule == QString(GRALLOC_MODULE_NAME_INTEL)) {
        return DisplayTypeManager::DisplayType::DISPLAY_TYPE_DRM;
    } else if (grallocModule == QString(GRALLOC_MODULE_NAME_EMUGL)) {
        return DisplayTypeManager::DisplayType::DISPLAY_TYPE_EMUGL;
    } else if (grallocModule == QString(GRALLOC_MODULE_NAME_SW)) {
        return DisplayTypeManager::DisplayType::DISPLAY_TYPE_SW;
    }

    return DisplayTypeManager::DisplayType::DISPLAY_TYPE_UNKNOWN;
}

DisplayTypeManager::DisplayType getDisplayTypeFromSettings()
{
    DisplayTypeManager::DisplayType type = DisplayTypeManager::DisplayType::DISPLAY_TYPE_UNKNOWN;
    QString confPath;
    QString displayTypeString;

    confPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.config/kmre/kmre.ini";

    if (QFile::exists(confPath)) {
        QSettings settings(confPath, QSettings::IniFormat);
        settings.setIniCodec("UTF-8");
        settings.beginGroup("display");
        displayTypeString = settings.value("display_type", QString("")).toString();
        settings.endGroup();
    }

    if (displayTypeString == QString(DISPLAY_TYPE_DRM_STRING)) {
        type = DisplayTypeManager::DisplayType::DISPLAY_TYPE_DRM;
    } else if (displayTypeString == QString(DISPLAY_TYPE_EMUGL_STRING)) {
        type = DisplayTypeManager::DisplayType::DISPLAY_TYPE_EMUGL;
    } else if (displayTypeString == QString(DISPLAY_TYPE_SW_STRING)) {
        type = DisplayTypeManager::DisplayType::DISPLAY_TYPE_SW;
    }

    return type;
}

static bool kmreConfigEnabled(const QString& group, const QString& key)
{
    bool enabled = false;
    QString confPath;

    confPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.config/kmre/kmre.ini";

    if (QFile::exists(confPath)) {
        QStringList groups;
        QStringList keys;
        QSettings settings(confPath, QSettings::IniFormat);
        settings.setIniCodec("UTF-8");

        groups = settings.childGroups();
        if (groups.contains(group)) {
            settings.beginGroup(group);
            keys = settings.childKeys();
            if (keys.contains(key)) {
                enabled = settings.value(key, false).toBool();
            }
            settings.endGroup();
        }
    }

    return enabled;
}

#if defined(__x86_64) || defined(__x86_64__) || defined(__amd64) || defined(__amd64__)
static bool useIntelGrallocSpecifically()
{
    return kmreConfigEnabled("display", "use_intel_gralloc");
}
#endif

static bool useDrmGrallocSpecifically()
{
    return kmreConfigEnabled("display", "use_drm_gralloc");
}

static bool detectOpenGLES31AEP()
{
    return kmreConfigEnabled("display", "detect_gles_31_aep");
}

static bool isDisplayTypeValid(DisplayTypeManager::DisplayType type)
{
    QList<DisplayTypeManager::DisplayType> types = DisplayTypeManager::getAccessableDisplayType();
    foreach (DisplayTypeManager::DisplayType t, types) {
        if (type == t) {
            return true;
        }
    }

    return false;
}

DisplayTypeManager::DisplayType DisplayTypeManager::mDisplayType = DisplayTypeManager::DISPLAY_TYPE_UNKNOWN;
bool DisplayTypeManager::mIsDrmEnabled = driDeviceCheck();
// don't use kernelHasConfigNoGpuAuthentication any more to avoid error in somewhere
bool DisplayTypeManager::mHasNoGPUAuthentication = false;//kernelHasConfigNoGpuAuthentication();
bool DisplayTypeManager::mCouldUsesDrmMode = GpuInfo::couldUsesDrmMode();
#if defined(__x86_64) || defined(__x86_64__) || defined(__amd64) || defined(__amd64__)
bool DisplayTypeManager::mCouldUseIntelGralloc = GpuInfo::couldUseIntelGralloc();
#endif
GpuVendor DisplayTypeManager::mGpuVendor = UNKNOWN_VGA;
GpuModel DisplayTypeManager::mGpuModel = UNKNOWN_GPU;
CpuType DisplayTypeManager::mCpuType = CPU_TYPE_UNKNOWN;

QList<DisplayTypeManager::DisplayType> DisplayTypeManager::getAccessableDisplayType()
{
    QList<DisplayTypeManager::DisplayType> types;

    types.append(DisplayType::DISPLAY_TYPE_EMUGL);
    if (mIsDrmEnabled && (mHasNoGPUAuthentication || mCouldUsesDrmMode)) {
        types.append(DisplayType::DISPLAY_TYPE_DRM);
    }

    return types;
}

DisplayTypeManager::DisplayType DisplayTypeManager::getDisplayType()
{
    DisplayType type = DISPLAY_TYPE_UNKNOWN;

    if (mDisplayType != DISPLAY_TYPE_UNKNOWN) {
        goto out;
    }

    if (mIsDrmEnabled && (mHasNoGPUAuthentication || mCouldUsesDrmMode)) {
        type = getDisplayTypeFromDBus();
        if (type != DISPLAY_TYPE_UNKNOWN) {
            mDisplayType = type;
            goto out;
        }

        type = getDisplayTypeFromSettings();
        if (isDisplayTypeValid(type)) {
            mDisplayType = type;
            goto out;
        }
        // using 'drm' mode default
        else if (type == DISPLAY_TYPE_UNKNOWN) {
            mDisplayType = DISPLAY_TYPE_DRM;
            goto out;
        }
    }
    
    mDisplayType = DISPLAY_TYPE_EMUGL;

out:
    updateDisplayType();
    return mDisplayType;
}

QString DisplayTypeManager::getDisplayTypeName(DisplayType type)
{
    if (DISPLAY_TYPE_DRM == type) {
        return QString("drm");
    } else if (DISPLAY_TYPE_SW == type) {
        return QString("sw");
    } else if (DISPLAY_TYPE_EMUGL == type) {
        return QString("emugl");
    }

    return QString("unknown");
}


GpuVendor DisplayTypeManager::getGpuVendor()
{
    if (mGpuVendor != UNKNOWN_VGA) {
        return mGpuVendor;
    }

    mGpuVendor = getGpuManufacture();

    return mGpuVendor;
}

QString DisplayTypeManager::getGpuVendorName(GpuVendor vendor)
{
    QString name = "UNKNOWN";

    switch(vendor) {
    case NVIDIA_VGA:
        name = "NVIDIA";
        break;
    case AMD_VGA:
        name = "AMD";
        break;
    case INTEL_VGA:
        name = "INTEL";
        break;
    case UNKNOWN_VGA:
    case OTHER_VGA:
    default:
        name = "UNKNOWN";
        break;
    }
    return name;
}

// 获取国产显卡的具体类型
GpuModel DisplayTypeManager::getGpuModel()
{
    if (mGpuModel != UNKNOWN_GPU) {
        return mGpuModel;
    }

    mGpuModel = getGpuModelType();

    return mGpuModel;
}

QString DisplayTypeManager::getGpuModelName(GpuModel model)
{
    return "";
}
CpuType DisplayTypeManager::getCpuType()
{
    if (mCpuType != CPU_TYPE_UNKNOWN) {
        return mCpuType;
    }

    int type = CPU_TYPE_UNKNOWN;
    char line[512] = {0};

    FILE* fp = NULL;
    fp = fopen(CPUINFO_PATH, "r");
    if (!fp) {
        mCpuType = type;
        return mCpuType;
    }

#if defined(__x86_64) || defined(__x86_64__) || defined(__amd64) || defined(__amd64__)
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strcasestr(line, "GenuineIntel") ||
            strcasestr(line, "Intel")) {
            type = CPU_TYPE_INTEL;
            break;
        }

        if (strcasestr(line, "AuthenticAMD") ||
            strcasestr(line, "AMD")) {
            type = CPU_TYPE_AMD;
            break;
        }
    }

    mCpuType = type;
#endif

    fclose(fp);

    return mCpuType;
}

QString DisplayTypeManager::getCpuTypeName(CpuType type)
{
    QString name = "UNKNOWN";
    QString modelName = getCpuModelName();

    switch(type) {
    case CPU_TYPE_INTEL:
        name = "INTEL";
        break;
    case CPU_TYPE_AMD:
        name = "AMD";
        break;
    case CPU_TYPE_UNKNOWN:
    default:
        name = modelName.isEmpty() ? "UNKNOWN" : modelName;
        break;
    }
    return name;
}

QString DisplayTypeManager::getCpuModelName()
{
    static QString cpuModelName = "";
    if (!cpuModelName.isEmpty()) {
        return cpuModelName;
    }

    QFile file("/proc/cpuinfo");
    if (file.open(QFile::ReadOnly)) {
        char buf[1024];

        while (file.readLine(buf, sizeof(buf)) >= 0) {
            QString s(buf);
            if (s.contains("model name")) {
                cpuModelName = s.mid(s.indexOf(":") + 1).trimmed();
                break;
            }
            else if (s.contains("vendor_id")) {
                cpuModelName = s.mid(s.indexOf(":") + 1).trimmed();
            }
        }

        file.close();
    }

    return cpuModelName;
}

bool DisplayTypeManager::isGpuSupported()
{
    return true;// don't check gpu now
}

bool DisplayTypeManager::isCpuSupported()
{
    return true;// don't check cpu now
}

bool DisplayTypeManager::supportsThreadedOpenGL()
{
    return GpuInfo::supportsThreadedOpenGL();
}

bool DisplayTypeManager::supportsOpenGLES31AEP()
{
    return GpuInfo::supportsOpenGLES31AEP();
}

void DisplayTypeManager::updateDisplayType()
{
    if (mDisplayType == DISPLAY_TYPE_UNKNOWN) {
        return;
    }

    if (mDisplayType != oldDisplayType) {
        oldDisplayType = mDisplayType;

        if ((getGpuVendor() == INTEL_VGA) || (getGpuVendor() == AMD_VGA)) {
            int primaryMajor = -1;
            int primaryMinor = -1;
            int renderMajor = -1;
            int renderMinor = -1;

            if (GpuInfo::getDrmDeviceNodeNumber(&primaryMajor, &primaryMinor, &renderMajor, &renderMinor) &&
                primaryMajor >= 0 && primaryMinor >= 0 && renderMajor >= 0 && renderMinor >= 0) {
                kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.drm.primary.major", QString::number(primaryMajor));
                kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.drm.primary.minor", QString::number(primaryMinor));
                kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.drm.render.major", QString::number(renderMajor));
                kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.drm.render.minor", QString::number(renderMinor));
            }

            if ((mDisplayType == DISPLAY_TYPE_DRM) &&
                    supportsOpenGLES31AEP() &&
                    detectOpenGLES31AEP()) {
                kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.kmre.detect_gles_31_aep", "true");
            } else {
                kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.kmre.detect_gles_31_aep", "false");
            }
        }

        if (mDisplayType == DISPLAY_TYPE_DRM) {
            kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.hardware.egl", EGL_MODULE_NAME_DRM);
#if defined(__x86_64) || defined(__x86_64__) || defined(__amd64) || defined(__amd64__)
            if (mCouldUseIntelGralloc) {
                kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.hardware.gralloc", GRALLOC_MODULE_NAME_INTEL);
            } else if ((getGpuVendor() == INTEL_VGA) && useIntelGrallocSpecifically()) {
                kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.hardware.gralloc", GRALLOC_MODULE_NAME_INTEL);
            } else {
                if (useDrmGrallocSpecifically()) {
                    kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.hardware.gralloc", GRALLOC_MODULE_NAME_DRM);
                } else {
                    kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.hardware.gralloc", GRALLOC_MODULE_NAME_GBM);
                }
            }
#else
            if (useDrmGrallocSpecifically()) {
                kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.hardware.gralloc", GRALLOC_MODULE_NAME_DRM);
            } else {
                kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.hardware.gralloc", GRALLOC_MODULE_NAME_GBM);
            }
#endif
			kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.kernel.qemu", "0");
            kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.kernel.qemu.gles", "0");
			kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "sys.goldfish.post", "false");
#ifdef UKUI_WAYLAND
            int primaryMajor = -1;
            int primaryMinor = -1;
            int renderMajor = -1;
            int renderMinor = -1;
            if (GpuInfo::getDrmDeviceNodeNumber(&primaryMajor, &primaryMinor, &renderMajor, &renderMinor) &&
                primaryMajor >= 0 && primaryMinor >= 0 && renderMajor >= 0 && renderMinor >= 0) {
                kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.drm.primary.major", QString::number(primaryMajor));
                kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.drm.primary.minor", QString::number(primaryMinor));
                kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.drm.render.major", QString::number(renderMajor));
                kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.drm.render.minor", QString::number(renderMinor));
            }
#endif
        } else if (mDisplayType == DISPLAY_TYPE_EMUGL) {
            kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.hardware.egl", EGL_MODULE_NAME_EMUGL);
            kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.hardware.gralloc", GRALLOC_MODULE_NAME_EMUGL);
            kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.kernel.qemu", "1");
            kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.kernel.qemu.gles", "1");
			kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "sys.goldfish.post", "true");
        } else if (mDisplayType == DISPLAY_TYPE_SW) {
            kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.hardware.egl", EGL_MODULE_NAME_SW);
            kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.hardware.gralloc", GRALLOC_MODULE_NAME_SW);
			kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.kernel.qemu", "0");
            kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "ro.kernel.qemu.gles", "0");
			kmre::DBusClient::getInstance()->SetDefaultPropOfContainer(Utils::getUserName(), getuid(), "sys.goldfish.post", "false");
        }
    }
}

