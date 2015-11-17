///////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2014 Tavendo GmbH
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
///////////////////////////////////////////////////////////////////////////////

#include <boost/lexical_cast.hpp>
#include <stdexcept>

namespace autobahn {

inline wamp_event_impl::wamp_event_impl()
    : m_arguments(EMPTY_ARGUMENTS)
    , m_kw_arguments(EMPTY_KW_ARGUMENTS)
    , m_details(EMPTY_DETAILS)
    , m_zone()
{
}

inline std::size_t wamp_event_impl::number_of_arguments() const
{
    return m_arguments.type == msgpack::type::ARRAY ? m_arguments.via.array.size : 0;
}

inline std::size_t wamp_event_impl::number_of_kw_arguments() const
{
    return m_kw_arguments.type == msgpack::type::MAP ? m_kw_arguments.via.map.size : 0;
}

template <typename T>
inline T wamp_event_impl::argument(std::size_t index) const
{
    if (m_arguments.type != msgpack::type::ARRAY || m_arguments.via.array.size <= index) {
        throw std::out_of_range("no argument at index " + boost::lexical_cast<std::string>(index));
    }
    return m_arguments.via.array.ptr[index].as<T>();
}

template <typename List>
inline List wamp_event_impl::arguments() const
{
    return m_arguments.as<List>();
}

template <typename List>
inline void wamp_event_impl::get_arguments(List& args) const
{
    m_arguments.convert(args);
}

template <typename... T>
inline void wamp_event_impl::get_each_argument(T&... args) const
{
    auto args_tuple = std::make_tuple(std::ref(args)...);
    m_arguments.convert(args_tuple);
}

template <typename T>
inline T wamp_event_impl::kw_argument(const std::string& key) const
{
    if (m_kw_arguments.type != msgpack::type::MAP) {
        throw msgpack::type_error();
    }
    for (std::size_t i = 0; i < m_kw_arguments.via.map.size; ++i) {
        const msgpack::object_kv& kv = m_kw_arguments.via.map.ptr[i];
        if (kv.key.type == msgpack::type::STR && key.size() == kv.key.via.str.size
                && key.compare(0, key.size(), kv.key.via.str.ptr, kv.key.via.str.size) == 0)
        {
            return kv.val.as<T>();
        }
    }
    throw std::out_of_range(key + " keyword argument doesn't exist");
}

template <typename T>
inline T wamp_event_impl::kw_argument(const char* key) const
{
    if (m_kw_arguments.type != msgpack::type::MAP) {
        throw msgpack::type_error();
    }
    std::size_t key_size = strlen(key);
    for (std::size_t i = 0; i < m_kw_arguments.via.map.size; ++i) {
        const msgpack::object_kv& kv = m_kw_arguments.via.map.ptr[i];
        if (kv.key.type == msgpack::type::STR && key_size == kv.key.via.str.size
                && memcmp(key, kv.key.via.str.ptr, key_size) == 0)
        {
            return kv.val.as<T>();
        }
    }
    throw std::out_of_range(std::string(key) + " keyword argument doesn't exist");
}

template <typename T>
inline T wamp_event_impl::kw_argument_or(const std::string& key, const T& fallback) const
{
    if (m_kw_arguments.type != msgpack::type::MAP) {
        throw msgpack::type_error();
    }
    for (std::size_t i = 0; i < m_kw_arguments.via.map.size; ++i) {
        const msgpack::object_kv& kv = m_kw_arguments.via.map.ptr[i];
        if (kv.key.type == msgpack::type::STR && key.size() == kv.key.via.str.size
                && key.compare(0, key.size(), kv.key.via.str.ptr, kv.key.via.str.size) == 0)
        {
            return kv.val.as<T>();
        }
    }
    return fallback;
}

template <typename T>
inline T wamp_event_impl::kw_argument_or(const char* key, const T& fallback) const
{
    if (m_kw_arguments.type != msgpack::type::MAP) {
        throw msgpack::type_error();
    }
    std::size_t key_size = strlen(key);
    for (std::size_t i = 0; i < m_kw_arguments.via.map.size; ++i) {
        const msgpack::object_kv& kv = m_kw_arguments.via.map.ptr[i];
        if (kv.key.type == msgpack::type::STR && key_size == kv.key.via.str.size
                && memcmp(key, kv.key.via.str.ptr, key_size) == 0)
        {
            return kv.val.as<T>();
        }
    }
    throw fallback;
}

template <typename Map>
inline Map wamp_event_impl::kw_arguments() const
{
    return m_kw_arguments.as<Map>();
}

template <typename Map>
inline void wamp_event_impl::get_kw_arguments(Map& kw_args) const
{
    m_kw_arguments.convert(kw_args);
}

template <typename T>
inline T wamp_event_impl::detail(const std::string& key) const
{
    if (m_details.type != msgpack::type::MAP) {
        throw msgpack::type_error();
    }
    for (std::size_t i = 0; i < m_details.via.map.size; ++i) {
        const msgpack::object_kv& kv = m_details.via.map.ptr[i];
        if (kv.key.type == msgpack::type::STR && key.size() == kv.key.via.str.size
                && key.compare(0, key.size(), kv.key.via.str.ptr, kv.key.via.str.size) == 0)
        {
            return kv.val.as<T>();
        }
    }
    throw std::out_of_range(key + " keyword argument doesn't exist");
}

template <typename T>
inline T wamp_event_impl::detail(const char* key) const
{
    if (m_details.type != msgpack::type::MAP) {
        throw msgpack::type_error();
    }
    std::size_t key_size = strlen(key);
    for (std::size_t i = 0; i < m_details.via.map.size; ++i) {
        const msgpack::object_kv& kv = m_details.via.map.ptr[i];
        if (kv.key.type == msgpack::type::STR && key_size == kv.key.via.str.size
                && memcmp(key, kv.key.via.str.ptr, key_size) == 0)
        {
            return kv.val.as<T>();
        }
    }
    throw std::out_of_range(std::string(key) + " keyword argument doesn't exist");
}

template <typename T>
inline T wamp_event_impl::detail_or(const std::string& key, const T& fallback) const
{
    if (m_details.type != msgpack::type::MAP) {
        throw msgpack::type_error();
    }
    for (std::size_t i = 0; i < m_details.via.map.size; ++i) {
        const msgpack::object_kv& kv = m_details.via.map.ptr[i];
        if (kv.key.type == msgpack::type::STR && key.size() == kv.key.via.str.size
                && key.compare(0, key.size(), kv.key.via.str.ptr, kv.key.via.str.size) == 0)
        {
            return kv.val.as<T>();
        }
    }
    return fallback;
}

template <typename T>
inline T wamp_event_impl::detail_or(const char* key, const T& fallback) const
{
    if (m_details.type != msgpack::type::MAP) {
        throw msgpack::type_error();
    }
    std::size_t key_size = strlen(key);
    for (std::size_t i = 0; i < m_details.via.map.size; ++i) {
        const msgpack::object_kv& kv = m_details.via.map.ptr[i];
        if (kv.key.type == msgpack::type::STR && key_size == kv.key.via.str.size
                && memcmp(key, kv.key.via.str.ptr, key_size) == 0)
        {
            return kv.val.as<T>();
        }
    }
    throw fallback;
}

template <typename Map>
inline Map wamp_event_impl::details() const
{
    return m_details.as<Map>();
}

template <typename Map>
inline void wamp_event_impl::get_details(Map& kw_args) const
{
    m_details.convert(kw_args);
}

inline void wamp_event_impl::set_arguments(const msgpack::object& arguments)
{
    m_arguments = arguments;
}

inline void wamp_event_impl::set_kw_arguments(const msgpack::object& kw_arguments)
{
    m_kw_arguments = kw_arguments;
}

inline void wamp_event_impl::set_details(const msgpack::object& details)
{
    m_details = details;
}

inline void wamp_event_impl::set_zone( msgpack::unique_ptr<msgpack::zone>& zone)
{
    m_zone = std::move(zone);
}

} // namespace autobahn
