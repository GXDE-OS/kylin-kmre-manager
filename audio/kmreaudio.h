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

#ifndef KMRE_AUDIO_H
#define KMRE_AUDIO_H

#include <QObject>

class AudioServer;

class KmreAudio : public QObject
{
    Q_OBJECT
public:
    KmreAudio(QObject * parent = 0);
    ~KmreAudio();

    void init();

public slots:
    void start();
    void stop();
    void onStopApplication(const QString &container);

private:
    AudioServer *m_server = nullptr;
};

#endif // KMRE_AUDIO_H
