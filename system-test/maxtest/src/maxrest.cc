/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2026-10-04
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

#include <maxtest/maxrest.hh>

using namespace std;

//
// MaxRest::SystemTestImp
//

class MaxRest::SystemTestImp : public MaxRest::Imp
{
public:
    SystemTestImp(TestConnections* pTest)
        : m_test(*pTest)
        , m_maxscale(*pTest->maxscale)
    {
    }

    SystemTestImp(TestConnections* pTest, mxt::MaxScale* pMaxscale)
        : m_test(*pTest)
        , m_maxscale(*pMaxscale)
    {
    }

    TestConnections& test() const override final
    {
        return m_test;
    }

    void raise(bool fail_on_error, const std::string& message) const override final
    {
        if (fail_on_error)
        {
            ++m_test.global_result;
        }

        throw runtime_error(message);
    }

    mxt::CmdResult execute_curl_command(const std::string& curl_command) const override final
    {
        return m_maxscale.ssh_output(curl_command, false);
    }

private:
    TestConnections& m_test;
    mxt::MaxScale&   m_maxscale;
};

//
// MaxRest::LocalImp
//

class MaxRest::LocalImp : public MaxRest::Imp
{
public:
    LocalImp()
    {
    }

    TestConnections& test() const override final
    {
        throw std::runtime_error("A local MaxRest does not have a TestConnections instance (yet).");
    }

    void raise(bool fail_on_error, const std::string& message) const override final
    {
        throw runtime_error(message);
    }

    mxt::CmdResult execute_curl_command(const std::string& curl_command) const override final
    {
        mxt::CmdResult rv;
        FILE* pFile = popen(curl_command.c_str(), "r");

        if (pFile)
        {
            rv.rc = 0;

            int c;
            while ((c = fgetc(pFile)) != EOF)
            {
                rv.output += (char)c;
            }

            pclose(pFile);
        }
        else
        {
            ostringstream ss;
            ss << "Could not popen '%s': " << mxb_strerror(errno);

            rv.output = ss.str();
        }

        return rv;
    }
};

//
// MaxRest
//
MaxRest::MaxRest()
    : m_sImp(new LocalImp())
{
}

MaxRest::MaxRest(TestConnections* pTest)
    : m_sImp(new SystemTestImp(pTest))
{
}

MaxRest::MaxRest(TestConnections* pTest, mxt::MaxScale* pMaxscale)
    : m_sImp(new SystemTestImp(pTest, pMaxscale))
{
}


MaxRest::Server::Server(const MaxRest& maxrest, json_t* pObject)
    : name       (maxrest.get<string>(pObject, "id", Presence::MANDATORY))
    , address    (maxrest.get<string>(pObject, "attributes/parameters/address"))
    , port       (maxrest.get<int64_t>(pObject, "attributes/parameters/port"))
    , connections(maxrest.get<int64_t>(pObject, "attributes/statistics/connections"))
    , state      (maxrest.get<string>(pObject, "attributes/state"))
{
}

mxb::Json MaxRest::v1_servers(const string& id) const
{
    string path("servers");
    path += "/";
    path += id;

    return curl_get(path);
}

mxb::Json MaxRest::v1_servers() const
{
    return curl_get("servers");
}

mxb::Json MaxRest::v1_services(const string& id) const
{
    string path("services");
    path += "/";
    path += id;

    return curl_get(path);
}

mxb::Json MaxRest::v1_services() const
{
    return curl_get("services");
}

void MaxRest::v1_maxscale_modules(const string& module,
                                      const string& command,
                                      const string& instance,
                                      const std::vector<string>& params) const
{
    string path("maxscale/modules");

    path += "/";
    path += module;
    path += "/";
    path += command;
    path += "?";
    path += instance;

    if (!params.empty())
    {
        for (const auto& param : params)
        {
            path += "\\&";
            path += param;
        }
    }

    curl_post(path);
}

MaxRest::Server MaxRest::show_server(const std::string& id) const
{
    mxb::Json object = v1_servers(id);
    json_t* pData = get_object(object.get_json(), "data", Presence::MANDATORY);
    return Server(*this, pData);
}

vector<MaxRest::Server> MaxRest::list_servers() const
{
    return get_array<Server>(v1_servers().get_json(), "data", Presence::MANDATORY);
}

json_t* MaxRest::get_object(json_t* pObject, const string& key, Presence presence) const
{
    json_t* pValue = json_object_get(pObject, key.c_str());

    if (!pValue && (presence == Presence::MANDATORY))
    {
        raise("Mandatory key '" + key + "' not present.");
    }

    return pValue;
}

json_t* MaxRest::get_leaf_object(json_t* pObject, const string& key, Presence presence) const
{
    auto i = key.find("/");

    if (i == string::npos)
    {
        pObject = get_object(pObject, key, presence);
    }
    else
    {
        string head = key.substr(0, i);
        string tail = key.substr(i + 1);

        pObject = get_object(pObject, head, Presence::MANDATORY);
        pObject = get_leaf_object(pObject, tail, presence);
    }

    return pObject;
}

mxb::Json MaxRest::curl_get(const string& path) const
{
    return curl(GET, path);
}

mxb::Json MaxRest::curl_post(const string& path) const
{
    return curl(POST, path);
}

mxb::Json MaxRest::curl(Command command, const string& path) const
{
    string url = "http://127.0.0.1:8989/v1/" + path;
    string curl_command = "curl -s -u admin:mariadb ";

    switch (command)
    {
    case GET:
        curl_command += "-X GET ";
        break;

    case POST:
        curl_command += "-X POST ";
        break;
    }

    curl_command += url;

    auto result = m_sImp->execute_curl_command(curl_command);

    if (result.rc != 0)
    {
        raise("Invocation of curl failed: " + result.output);
    }

    mxb::Json rv;

    if (!result.output.empty() && !rv.load_string(result.output))
    {
        raise("JSON parsing failed: " + rv.error_msg());
    }

    return rv;
}

template<>
string MaxRest::get<string>(json_t* pObject, const string& key, Presence presence) const
{
    json_t* pValue = get_leaf_object(pObject, key, presence);

    if (pValue && !json_is_string(pValue))
    {
        raise("Key '" + key + "' is present, but value is not a string.");
    }

    return pValue ? json_string_value(pValue) : "";
}

template<>
int64_t MaxRest::get<int64_t>(json_t* pObject, const string& key, Presence presence) const
{
    json_t* pValue = get_leaf_object(pObject, key, presence);

    if (pValue && !json_is_integer(pValue))
    {
        raise("Key '" + key + "' is present, but value is not an integer.");
    }

    return pValue ? json_integer_value(pValue) : 0;
}
