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

#ifndef DISPLAYTYPEMANAGER_H
#define DISPLAYTYPEMANAGER_H

#include <QList>
#include <QString>


enum GpuVendor {
    UNKNOWN_VGA = -1,
    NVIDIA_VGA = 0,
    AMD_VGA = 1,
    INTEL_VGA = 3,
    OTHER_VGA = 99
};

enum GpuModel {
    UNKNOWN_GPU = -1,
    OTHER_GPU = 4
};

enum CpuType {
    CPU_TYPE_UNKNOWN = -1,
    CPU_TYPE_INTEL,
    CPU_TYPE_AMD,
    CPU_TYPE_MAX
};

class DisplayTypeManager
{
public:
    enum DisplayType {
        DISPLAY_TYPE_UNKNOWN = -1,
        DISPLAY_TYPE_DRM,
        DISPLAY_TYPE_SW,
        DISPLAY_TYPE_EMUGL,
    };

    static DisplayType getDisplayType();
    static QString getDisplayTypeName(DisplayType type);
    static QList<DisplayType> getAccessableDisplayType();
    static GpuVendor getGpuVendor();
    static QString getGpuVendorName(GpuVendor vendor);
    static GpuModel getGpuModel();
    static QString getGpuModelName(GpuModel model);
    static CpuType getCpuType();
    static QString getCpuTypeName(CpuType type);
    static QString getCpuModelName();
    static bool supportsThreadedOpenGL();
    static bool supportsOpenGLES31AEP();
    static bool isCpuSupported();
    static bool isGpuSupported();

private:
    static void updateDisplayType();

    static bool mIsDrmEnabled;
    static bool mHasNoGPUAuthentication;
    static bool mCouldUsesDrmMode;
    static DisplayType mDisplayType;
    static GpuVendor mGpuVendor;
    static GpuModel mGpuModel;
    static CpuType mCpuType;
#if defined(__x86_64) || defined(__x86_64__) || defined(__amd64) || defined(__amd64__)
    static bool mCouldUseIntelGralloc;
#endif
};

#endif // DISPLAYTYPEMANAGER_H
