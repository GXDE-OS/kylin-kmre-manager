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

#ifndef DISPLAYSIZEMANAGER_H
#define DISPLAYSIZEMANAGER_H

#include <QSize>

class DisplaySizeManager
{
public:
    static QSize getWindowDisplaySize();
    static QSize getPhysicalDisplaySize();
    static int getWindowDensity();
    static int getPhysicalDensity();
    static int getQemuDensity();

private:
    static QSize getPreferedWindowDisplaySize();
    static void updateVirtualDisplaySize();
    static void updatePhysicalDisplaySize();

    static QSize mVirtualDisplaySize;
    static QSize mPhysicalDisplaySize;
    static int mVirtualDensity;
    static int mPhysicalDensity;
    static int mQemuDensity;
};

#endif // DISPLAYSIZEMANAGER_H
