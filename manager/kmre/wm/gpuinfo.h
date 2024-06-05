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

#ifndef KMRE_GPUINFO_H
#define KMRE_GPUINFO_H

#include "displaytypemanager.h"

class GpuInfo
{
public:
    static bool preferI965();
    static bool isIntelIrisGpu();
    static bool couldUsesDrmMode();
    static GpuVendor getGpuVendorFromDrm();
    static bool getDrmDeviceNodeNumber(int* primaryMajor, int* primaryMinor,
                                       int* renderMajor, int* renderMinor);
    // supportsThreadedOpenGL() is for Qt window.
    static bool supportsThreadedOpenGL();

    static bool supportsOpenGLES31AEP();


#if defined(__x86_64) || defined(__x86_64__) || defined(__amd64) || defined(__amd64__)
    static bool couldUseIntelGralloc();
#endif

private:
    static bool mGpuCheckCompleted;
    static GpuVendor mGpuVendor;
};

#endif // KMRE_GPUINFO_H
