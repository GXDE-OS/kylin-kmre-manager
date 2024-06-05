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

#ifndef KMRE_UTILS_H_
#define KMRE_UTILS_H_

#include <boost/asio.hpp>
#include <map>
#include <memory>
#include <mutex>

namespace kmre {

template <typename stream_protocol2>
class BaseConnectionWorker {
public:
    virtual void add_connection_and_read_data(std::shared_ptr<boost::asio::basic_stream_socket<stream_protocol2>> const&socket) = 0;
};



template <class Connection>
class ConnectionBox {
public:
    ConnectionBox() {}
    ~ConnectionBox()
    {
        clear();
    }

    void add(std::shared_ptr<Connection> const& connection)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_connections.insert({connection->id(), connection});
    }

    void remove(uint32_t id)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_connections.erase(id);
    }

    void clear()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_connections.clear();
    }

private:
    ConnectionBox(ConnectionBox const&) = delete;
    ConnectionBox& operator=(ConnectionBox const&) = delete;

    std::mutex m_mutex;
    std::map<int, std::shared_ptr<Connection>> m_connections;
};

}  // namespace kmre

#endif // KMRE_UTILS_H_
