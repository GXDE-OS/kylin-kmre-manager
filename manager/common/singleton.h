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

#pragma once

namespace kmre {

template<class T>
class Singleton 
{
public:
    static T& getInstance() {
        static T instance;
        return instance;
    }

    Singleton(const Singleton&) = delete;
    Singleton& operator = (const Singleton&) = delete;

    virtual ~Singleton() {}

protected:
    Singleton(){}

private:
    
};

template<class T>
class SingletonP
{
public:
    static T* getInstance() {
        if (pInstance == nullptr) {
            pInstance = new T();
        }
        return pInstance;
    }

    static void destroy() {
        if (pInstance) {
            delete pInstance;
            pInstance = nullptr;
        }
    }

    SingletonP(const SingletonP&) = delete;
    SingletonP& operator = (const SingletonP&) = delete;

protected:
    SingletonP(){}
    virtual ~SingletonP() {}

private:
    static T* pInstance;
};

template<class T> 
T* SingletonP<T>::pInstance = nullptr;

}