/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2026-04-08
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

#include <maxscale/ccdefs.hh>

#include <algorithm>
#include <mutex>
#include <new>
#include <set>
#include <string>
#include <unordered_map>

#include <maxscale/cn_strings.hh>
#include <maxscale/jansson.hh>
#include <maxscale/users.hh>

namespace
{

const char STR_BASIC[] = "basic";
const char STR_ADMIN[] = "admin";

// Generates SHA2-512 hashes
constexpr const char* ADMIN_SALT = "$6$MXS";

// Generates MD5 hashes, only used for authentication of old users
constexpr const char* OLD_ADMIN_SALT = "$1$MXS";

using Guard = std::lock_guard<std::mutex>;
}

namespace maxscale
{

Users::Users(const Users& rhs)
    : m_data(rhs.copy_contents())
{
}

Users& Users::operator=(const Users& rhs)
{
    // Get a copy of the rhs.data to avoid locking both mutexes simultaneously.
    auto rhs_data = rhs.copy_contents();
    Guard guard(m_lock);
    m_data = std::move(rhs_data);
    return *this;
}

Users::Users(Users&& rhs) noexcept
    : m_data(std::move(rhs.m_data))     // rhs should be a temporary, and no other thread can access it. No
                                        // lock.
{
}

Users& Users::operator=(Users&& rhs) noexcept
{
    Guard guard(m_lock);
    m_data = std::move(rhs.m_data);     // same as above
    return *this;
}

bool Users::add(const std::string& user, const std::string& password, user_account_type perm)
{
    return add_hashed(user, hash(password), perm);
}

bool Users::remove(const std::string& user)
{
    Guard guard(m_lock);
    bool rval = false;

    auto it = m_data.find(user);
    if (it != m_data.end())
    {
        m_data.erase(it);
        rval = true;
    }

    return rval;
}

bool Users::get(const std::string& user, UserInfo* output) const
{
    Guard guard(m_lock);
    auto it = m_data.find(user);
    bool rval = false;

    if (it != m_data.end())
    {
        rval = true;
        if (output)
        {
            *output = it->second;
        }
    }

    return rval;
}

std::vector<UserInfo> Users::get_all() const
{
    std::vector<UserInfo> rval;
    Guard guard(m_lock);
    rval.reserve(m_data.size());

    for (const auto& [k, v] : m_data)
    {
        rval.push_back(v);
    }

    return rval;
}

bool Users::authenticate(const std::string& user, const std::string& password)
{
    bool rval = false;
    UserInfo info;

    if (get(user, &info))
    {
        // The second character tell us which hashing function to use
        auto crypted = info.password[1] == ADMIN_SALT[1] ? hash(password) : old_hash(password);
        rval = info.password == crypted;
    }

    return rval;
}

int Users::admin_count() const
{
    return std::count_if(m_data.begin(), m_data.end(), is_admin);
}

bool
Users::check_permissions(const std::string& user, const std::string& password, user_account_type perm) const
{
    Guard guard(m_lock);
    auto it = m_data.find(user);
    bool rval = false;

    if (it != m_data.end() && it->second.permissions == perm)
    {
        rval = true;
    }

    return rval;
}

bool Users::set_permissions(const std::string& user, user_account_type perm)
{
    Guard guard(m_lock);
    auto it = m_data.find(user);
    bool rval = false;

    if (it != m_data.end())
    {
        rval = true;
        it->second.permissions = perm;
    }

    return rval;
}

json_t* Users::diagnostics() const
{
    Guard guard(m_lock);
    json_t* rval = json_array();

    for (const auto& [key, val] : m_data)
    {
        json_array_append_new(rval, val.to_json(UserInfo::NO_PW));
    }

    return rval;
}

bool Users::empty() const
{
    Guard guard(m_lock);
    return m_data.empty();
}

Users::UserMap Users::copy_contents() const
{
    Guard guard(m_lock);
    return m_data;
}

json_t* Users::to_json() const
{
    json_t* arr = json_array();
    Guard guard(m_lock);

    for (const auto& [key, val] : m_data)
    {
        json_array_append_new(arr, val.to_json(UserInfo::WITH_PW));
    }

    return arr;
}

bool Users::is_last_user(const std::string& user) const
{
    Guard guard(m_lock);
    return m_data.size() == 1 && m_data.find(user) != m_data.end();
}

bool Users::add_hashed(const std::string& user, const std::string& password, user_account_type perm)
{
    Guard guard(m_lock);
    return m_data.insert(std::make_pair(user, UserInfo(user, password, perm))).second;
}

bool Users::is_admin(const std::unordered_map<std::string, UserInfo>::value_type& value)
{
    return value.second.permissions == USER_ACCOUNT_ADMIN;
}

bool Users::load_json(json_t* json)
{
    bool ok = true;
    // This function is always called in a single-threaded context
    size_t i;
    json_t* value;

    json_array_foreach(json, i, value)
    {
        json_t* name = json_object_get(value, CN_NAME);
        json_t* type = json_object_get(value, CN_ACCOUNT);
        json_t* password = json_object_get(value, CN_PASSWORD);

        if (name && json_is_string(name)
            && type && json_is_string(type)
            && password && json_is_string(password)
            && json_to_account_type(type) != USER_ACCOUNT_UNKNOWN)
        {
            add_hashed(json_string_value(name),
                       json_string_value(password),
                       json_to_account_type(type));
        }
        else
        {
            MXB_ERROR("Corrupt JSON value in users file: %s", mxs::json_dump(value).c_str());
            ok = false;
        }
    }

    return ok;
}

std::string Users::hash(const std::string& password)
{
    const int CACHE_MAX_SIZE = 1000;
    static std::unordered_map<std::string, std::string> hash_cache;
    auto it = hash_cache.find(password);

    if (it != hash_cache.end())
    {
        return it->second;
    }
    else
    {
        if (hash_cache.size() > CACHE_MAX_SIZE)
        {
            auto bucket = rand() % hash_cache.bucket_count();
            mxb_assert(bucket < hash_cache.bucket_count());
            hash_cache.erase(hash_cache.cbegin(bucket)->first);
        }

        auto new_hash = mxs::crypt(password, ADMIN_SALT);
        hash_cache.insert(std::make_pair(password, new_hash));
        return new_hash;
    }
}

std::string Users::old_hash(const std::string& password)
{
    return mxs::crypt(password, OLD_ADMIN_SALT);
}
}

using mxs::Users;

bool users_change_password(Users* users, const char* user, const char* password)
{
    mxs::UserInfo info;
    return users->get(user, &info) && users->remove(user) && users->add(user, password, info.permissions);
}

bool users_is_admin(Users* users, const char* user, const char* password)
{
    return users->check_permissions(user, password ? password : "", mxs::USER_ACCOUNT_ADMIN);
}

const char* account_type_to_str(mxs::user_account_type type)
{
    switch (type)
    {
    case mxs::USER_ACCOUNT_BASIC:
        return STR_BASIC;

    case mxs::USER_ACCOUNT_ADMIN:
        return STR_ADMIN;

    default:
        return "unknown";
    }
}

mxs::user_account_type json_to_account_type(json_t* json)
{
    std::string str = json_string_value(json);

    if (str == STR_BASIC)
    {
        return mxs::USER_ACCOUNT_BASIC;
    }
    else if (str == STR_ADMIN)
    {
        return mxs::USER_ACCOUNT_ADMIN;
    }

    return mxs::USER_ACCOUNT_UNKNOWN;
}

json_t* mxs::UserInfo::to_json(IncludePW include_pw) const
{
    json_t* obj = json_object();

    json_object_set_new(obj, CN_NAME, json_string(name.c_str()));
    json_object_set_new(obj, CN_ACCOUNT, json_string(account_type_to_str(permissions)));

    if (include_pw == WITH_PW)
    {
        json_object_set_new(obj, CN_PASSWORD, json_string(password.c_str()));
    }

    return obj;
}
