/*
 * Copyright (c) 2020 MariaDB Corporation Ab
 * Copyright (c) 2023 MariaDB plc, Finnish Branch
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2027-09-09
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

#include "nosqlbase.hh"
#include "nosqlcommand.hh"
#include "nosqldatabase.hh"
#include <mysqld_error.h>

using namespace std;

namespace nosql
{

//
// Error classes
//
void Exception::append_write_error(ArrayBuilder& write_errors, int index) const
{
    DocumentBuilder write_error;
    write_error.append(kvp(key::INDEX, index));
    write_error.append(kvp(key::CODE, m_code));
    write_error.append(kvp(key::ERRMSG, what()));

    write_errors.append(write_error.extract());
}

NoError::NoError(int32_t n)
    : m_n(n)
{
}

NoError::NoError(int32_t n, bool updated_existing)
    : m_n(n)
    , m_updated_existing(updated_existing)
{
}

NoError::NoError(unique_ptr<Id>&& sUpserted)
    : m_n(1)
    , m_updated_existing(false)
    , m_sUpserted(std::move(sUpserted))
{
}

void NoError::populate(nosql::DocumentBuilder& doc)
{
    nosql::DocumentBuilder writeConcern;
    writeConcern.append(kvp(key::W, 1));
    writeConcern.append(kvp(key::WTIMEOUT, 0));

    if (m_n != -1)
    {
        doc.append(kvp(key::N, m_n));
    }

    if (m_updated_existing)
    {
        doc.append(kvp(key::UPDATED_EXISTING, m_updated_existing));
    }

    if (m_sUpserted)
    {
        m_sUpserted->append(doc, key::UPSERTED);
    }

    doc.append(kvp(key::SYNC_MILLIS, 0));
    doc.append(kvp(key::WRITTEN_TO, bsoncxx::types::b_null()));
    doc.append(kvp(key::WRITE_CONCERN, writeConcern.extract()));
    doc.append(kvp(key::ERR, bsoncxx::types::b_null()));
}

GWBUF SoftError::create_response(const Command& command) const
{
    DocumentBuilder doc;
    create_response(command, doc);

    return command.create_response(doc.extract(), Command::IsError::YES);
}

void SoftError::create_response(const Command& command, DocumentBuilder& doc) const
{
    doc.append(kvp(key::OK, 0));
    if (command.response_kind() == Command::ResponseKind::REPLY)
    {
        // TODO: Turning on the error bit in the OP_REPLY is not sufficient, but "$err"
        // TODO: must be set as well. Figure out why, because it should not be needed.
        doc.append(kvp("$err", what()));
    }
    doc.append(kvp(key::ERRMSG, what()));
    doc.append(kvp(key::CODE, m_code));
    doc.append(kvp(key::CODE_NAME, nosql::error::name(m_code)));
}

void ConcreteLastError::populate(DocumentBuilder& doc)
{
    doc.append(nosql::kvp(nosql::key::ERR, m_err));
    doc.append(nosql::kvp(nosql::key::CODE, m_code));
    doc.append(nosql::kvp(nosql::key::CODE_NAME, nosql::error::name(m_code)));
}

unique_ptr<LastError> SoftError::create_last_error() const
{
    return std::make_unique<ConcreteLastError>(what(), m_code);
}

GWBUF HardError::create_response(const Command& command) const
{
    DocumentBuilder doc;
    create_response(command, doc);

    return command.create_response(doc.extract(), Command::IsError::YES);
}

void HardError::create_response(const Command&, DocumentBuilder& doc) const
{
    doc.append(kvp("$err", what()));
    doc.append(kvp(key::CODE, m_code));
}

unique_ptr<LastError> HardError::create_last_error() const
{
    return std::make_unique<ConcreteLastError>(what(), m_code);
}

MariaDBError::MariaDBError(const ComERR& err)
    : Exception("Protocol command failed due to MariaDB error.", error::COMMAND_FAILED)
    , m_mariadb_code(err.code())
    , m_mariadb_message(err.message())
{
}

GWBUF MariaDBError::create_response(const Command& command) const
{
    DocumentBuilder doc;
    create_response(command, doc);

    return command.create_response(doc.extract(), Command::IsError::YES);
}

void MariaDBError::create_response(const Command& command, DocumentBuilder& doc) const
{
    bool create_default = true;

    switch (m_mariadb_code)
    {
    case ER_ACCESS_DENIED_ERROR:
    case ER_COLUMNACCESS_DENIED_ERROR:
    case ER_DBACCESS_DENIED_ERROR:
    case ER_TABLEACCESS_DENIED_ERROR:
        create_authorization_error(command, doc);
        create_default = false;
        break;

    case ER_CONNECTION_KILLED:
        {
            // This may be due to an authentication error, but that information is lost
            // except in the message itself.

            static const char AUTHENTICATION_TO[] = "Authentication to ";

            if (m_mariadb_message.substr(0, sizeof(AUTHENTICATION_TO) - 1) == AUTHENTICATION_TO)
            {
                // Ok, we have something like "Authentication to 'Server1' failed: 1045, #28000: ..."

                auto message = m_mariadb_message.substr(sizeof(AUTHENTICATION_TO));

                static const char FAILED[] = "' failed: ";

                auto i = message.find(FAILED);

                if (i != string::npos)
                {
                    i += sizeof(FAILED) - 1;
                    auto j = message.find(",", i);

                    if (j != string::npos)
                    {
                        // So, in the string "...failed: 1045,...", i points at "1" and j at ",".
                        auto s = message.substr(i, j);

                        if (atoi(s.c_str()) == ER_ACCESS_DENIED_ERROR)
                        {
                            // Ok, it was an authentication error that lead to the
                            // connection being closed.
                            create_authorization_error(command, doc);
                            create_default = false;
                        }
                    }
                }
            }
        }
    }

    if (create_default)
    {
        string json = command.to_json();
        string sql = command.last_statement();

        DocumentBuilder mariadb;
        mariadb.append(kvp(key::CODE, m_mariadb_code));
        mariadb.append(kvp(key::MESSAGE, m_mariadb_message));
        mariadb.append(kvp(key::COMMAND, json));
        mariadb.append(kvp(key::SQL, sql));

        doc.append(kvp("$err", what()));
        auto protocol_code = error::from_mariadb_code(m_mariadb_code);;
        doc.append(kvp(key::CODE, protocol_code));
        doc.append(kvp(key::CODE_NAME, error::name(protocol_code)));
        doc.append(kvp(key::MARIADB, mariadb.extract()));

        MXB_ERROR("Protocol command failed due to MariaDB error: "
                  "json = \"%s\", code = %d, message = \"%s\", sql = \"%s\"",
                  json.c_str(), m_mariadb_code, m_mariadb_message.c_str(), sql.c_str());
    }
}

unique_ptr<LastError> MariaDBError::create_last_error() const
{
    class MariaDBLastError : public ConcreteLastError
    {
    public:
        MariaDBLastError(const string& err,
                         int32_t mariadb_code,
                         const string& mariadb_message)
            : ConcreteLastError(err, error::from_mariadb_code(mariadb_code))
            , m_mariadb_code(mariadb_code)
            , m_mariadb_message(mariadb_message)
        {
        }

        void populate(DocumentBuilder& doc) override
        {
            ConcreteLastError::populate(doc);

            DocumentBuilder mariadb;
            mariadb.append(kvp(key::CODE, m_mariadb_code));
            mariadb.append(kvp(key::MESSAGE, m_mariadb_message));

            doc.append(kvp(key::MARIADB, mariadb.extract()));
        }

    private:
        int32_t m_mariadb_code;
        string  m_mariadb_message;
    };

    return std::make_unique<ConcreteLastError>(what(), m_code);
}

void MariaDBError::create_authorization_error(const Command& command, DocumentBuilder& doc) const
{
    string json = command.to_json();
    string sql = command.last_statement();

    DocumentBuilder mariadb;
    mariadb.append(kvp(key::CODE, m_mariadb_code));
    mariadb.append(kvp(key::MESSAGE, m_mariadb_message));
    mariadb.append(kvp(key::COMMAND, json));
    mariadb.append(kvp(key::SQL, sql));

    ostringstream ss;
    ss << "not authorized on " << command.database().name() << " to execute command "
       << command.to_json();

    doc.append(kvp(key::OK, 0));
    doc.append(kvp(key::ERRMSG, ss.str()));
    doc.append(kvp(key::CODE, error::UNAUTHORIZED));
    doc.append(kvp(key::CODE_NAME, error::name(error::UNAUTHORIZED)));

    doc.append(kvp(key::MARIADB, mariadb.extract()));
}

//
// namespace nosql::error
//
int error::from_mariadb_code(int code)
{
    // TODO: Expand the range of used codes.

    switch (code)
    {
    case 0:
        return OK;

    default:
        return COMMAND_FAILED;
    }
}

const char* error::name(int protocol_code)
{
    switch (protocol_code)
    {
#define NOSQL_ERROR(symbol, code, name) case symbol: { return name; }
#include "nosqlerror.hh"
#undef NOSQL_ERROR

    default:
        mxb_assert(!true);
        return "";
    }
}

}

//
// namespace nosql, free functions
//
string nosql::escape_essential_chars(string&& from)
{
    auto it = from.begin();
    auto end = from.end();

    while (it != end && *it != '\'' && *it != '\\')
    {
        ++it;
    }

    if (it == end)
    {
        return from;
    }

    string to(from.begin(), it);

    if (*it == '\'')
    {
        to.push_back('\'');
    }
    else
    {
        to.push_back('\\');
    }

    to.push_back(*it++);

    while (it != end)
    {
        auto c = *it;

        switch (c)
        {
        case '\\':
            to.push_back('\\');
            break;

        case '\'':
            to.push_back('\'');
            break;

        default:
            break;
        }

        to.push_back(c);

        ++it;
    }

    return to;
}

bool nosql::is_valid_database_name(const std::string& name)
{
    // Backtick - '`' - is actually a valid character for database names, but
    // as it is 1) likely that it is exceedingly rare that it would be used
    // in a database name and 2) if allowed, it would have to be encoded, we
    // reject it.
    return !name.empty() && name.find_first_of(" /\\.\"$`") == string::npos;
}

std::string nosql::element_to_value(const bsoncxx::types::bson_value::view& x,
                                    ValueFor value_for,
                                    const std::string& op)
{
    std::ostringstream ss;

    switch (x.type())
    {
    case bsoncxx::type::k_double:
        double_to_string(x.get_double(), ss);
        break;

    case bsoncxx::type::k_utf8:
        {
            const auto& utf8 = x.get_utf8();
            string_view s(utf8.value.data(), utf8.value.size());

            switch (value_for)
            {
            case ValueFor::JSON:
                ss << "'\"" << s << "\"'";
                break;

            case ValueFor::JSON_NESTED:
            case ValueFor::SQL:
                ss << "\"" << s << "\"";
            }
        }
        break;

    case bsoncxx::type::k_int32:
        ss << x.get_int32();
        break;

    case bsoncxx::type::k_int64:
        ss << x.get_int64();
        break;

    case bsoncxx::type::k_binary:
        {
            auto b = x.get_binary();

            string_view s(reinterpret_cast<const char*>(b.bytes), b.size);

            ss << "'" << s << "'";
        }
        break;

    case bsoncxx::type::k_bool:
        ss << x.get_bool();
        break;

    case bsoncxx::type::k_date:
        ss << x.get_date();
        break;

    case bsoncxx::type::k_array:
        {
            ss << "JSON_ARRAY(";

            bsoncxx::array::view a = x.get_array();

            bool first = true;
            for (auto element : a)
            {
                if (first)
                {
                    first = false;
                }
                else
                {
                    ss << ", ";
                }

                ss << element_to_value(element.get_value(), ValueFor::JSON_NESTED, op);
            }

            ss << ")";
        }
        break;

    case bsoncxx::type::k_document:
        {
            ss << "JSON_OBJECT(";

            bsoncxx::document::view d = x.get_document();

            bool first = true;
            for (auto element : d)
            {
                if (first)
                {
                    first = false;
                }
                else
                {
                    ss << ", ";
                }

                ss << "\"" << element.key() << "\", "
                   << element_to_value(element.get_value(), ValueFor::JSON_NESTED, op);
            }

            ss << ")";
        }
        break;

    case bsoncxx::type::k_null:
        switch (value_for)
        {
        case ValueFor::JSON:
        case ValueFor::JSON_NESTED:
            ss << "null";
            break;

        case ValueFor::SQL:
            ss << "'null'";
        }
        break;

    case bsoncxx::type::k_regex:
        {
            std::ostringstream ss2;

            auto r = x.get_regex();
            if (r.options.length() != 0)
            {
                ss2 << "(?" << r.options << ")";
            }

            ss2 << r.regex;

            ss << "REGEXP '" << escape_essential_chars(ss2.str()) << "'";
        }
        break;

    case bsoncxx::type::k_minkey:
        ss << std::numeric_limits<int64_t>::min();
        break;

    case bsoncxx::type::k_maxkey:
        ss << std::numeric_limits<int64_t>::max();
        break;

    case bsoncxx::type::k_code:
        ss << "'" << x.get_code().code << "'";
        break;

    case bsoncxx::type::k_undefined:
        throw SoftError("cannot compare to undefined", error::BAD_VALUE);

    default:
        {
            ss << "cannot convert a " << bsoncxx::to_string(x.type()) << " to a value for comparison";

            throw nosql::SoftError(ss.str(), nosql::error::BAD_VALUE);
        }
    }

    return ss.str();
}

std::string nosql::element_to_string(const bsoncxx::types::bson_value::view& x)
{
    std::ostringstream ss;

    switch (x.type())
    {
    case bsoncxx::type::k_array:
        {
            bool first = true;
            ss << "[";
            bsoncxx::array::view array = x.get_array();
            for (const auto& item : array)
            {
                if (first)
                {
                    first = false;
                }
                else
                {
                    ss << ", ";
                }

                ss << element_to_string(item.get_value());
            }
            ss << "]";
        }
        break;

    case bsoncxx::type::k_bool:
        ss << x.get_bool();
        break;

    case bsoncxx::type::k_code:
        ss << x.get_code().code;
        break;

    case bsoncxx::type::k_date:
        ss << x.get_date();
        break;

    case bsoncxx::type::k_decimal128:
        ss << x.get_decimal128().value.to_string();
        break;

    case bsoncxx::type::k_document:
        ss << escape_essential_chars(std::move(bsoncxx::to_json(x.get_document())));
        break;

    case bsoncxx::type::k_double:
        ss << element_to_value(x, ValueFor::JSON);
        break;

    case bsoncxx::type::k_int32:
        ss << x.get_int32();
        break;

    case bsoncxx::type::k_int64:
        ss << x.get_int64();
        break;

    case bsoncxx::type::k_null:
        ss << "null";
        break;

    case bsoncxx::type::k_oid:
        ss << "{\"$oid\":\"" << x.get_oid().value.to_string() << "\"}";
        break;

    case bsoncxx::type::k_regex:
        ss << x.get_regex().regex;
        break;

    case bsoncxx::type::k_symbol:
        ss << x.get_symbol().symbol;
        break;

    case bsoncxx::type::k_utf8:
        {
            const auto& view = x.get_utf8().value;
            std::string value(view.data(), view.length());
            ss << escape_essential_chars(std::move(value));
        }
        break;

    case bsoncxx::type::k_minkey:
        ss << "{\"$minKey\":1}";
        break;

    case bsoncxx::type::k_maxkey:
        ss << "{\"$maxKey\":1}";
        break;

    case bsoncxx::type::k_undefined:
        throw SoftError("cannot compare to undefined", error::BAD_VALUE);
        break;

    case bsoncxx::type::k_binary:
    case bsoncxx::type::k_codewscope:
    case bsoncxx::type::k_dbpointer:
    case bsoncxx::type::k_timestamp:
        {
            ss << "A " << bsoncxx::to_string(x.type()) << " cannot be converted to a string.";
            throw SoftError(ss.str(), error::BAD_VALUE);
        }
        break;
    }

    return ss.str();
}

mxb::Json nosql::bson_to_json(const bsoncxx::types::value& x)
{
    mxb::json::Undefined rv;

    switch (x.type())
    {
    case bsoncxx::type::k_array:
        {
            rv = mxb::json::Array();

            bsoncxx::array::view array = x.get_array();
            for (const auto& item : array)
            {
                rv.add_array_elem(bson_to_json(item.get_value()));
            }
        }
        break;

    case bsoncxx::type::k_bool:
        rv = mxb::json::Boolean(x.get_bool());
        break;

    case bsoncxx::type::k_document:
        {
            rv = mxb::json::Object();

            bsoncxx::document::view doc = x.get_document();

            for (const auto& element : doc)
            {
                rv.set_object(std::string(element.key()), bson_to_json(element.get_value()));
            }
        }
        break;

    case bsoncxx::type::k_double:
        rv = mxb::json::Real(x.get_double());
        break;

    case bsoncxx::type::k_int32:
        rv = mxb::json::Integer(x.get_int32());
        break;

    case bsoncxx::type::k_int64:
        rv = mxb::json::Integer(x.get_int64());
        break;

    case bsoncxx::type::k_null:
        rv = mxb::json::Null();
        break;

    case bsoncxx::type::k_utf8:
        rv = mxb::json::String(static_cast<std::string_view>(x.get_utf8()));
        break;

    case bsoncxx::type::k_undefined:
        break;

    case bsoncxx::type::k_code:
    case bsoncxx::type::k_date:
    case bsoncxx::type::k_regex:
    case bsoncxx::type::k_symbol:
    case bsoncxx::type::k_minkey:
    case bsoncxx::type::k_maxkey:
    case bsoncxx::type::k_oid:
    case bsoncxx::type::k_decimal128:
    case bsoncxx::type::k_binary:
    case bsoncxx::type::k_codewscope:
    case bsoncxx::type::k_dbpointer:
    case bsoncxx::type::k_timestamp:
    default:
        {
            std::stringstream ss;
            ss << "Cannot convert a " << bsoncxx::to_string(x.type()) << " to a Json value.";
            throw SoftError(ss.str(), error::BAD_VALUE);
        }
        break;
    }

    return rv;
}

template<>
bool nosql::element_as(const bsoncxx::document::element& element,
                       Conversion conversion,
                       double* pT)
{
    bool rv = true;

    auto type = element.type();

    if (conversion == Conversion::STRICT && type != bsoncxx::type::k_double)
    {
        rv = false;
    }
    else
    {
        switch (type)
        {
        case bsoncxx::type::k_int32:
            *pT = element.get_int32();
            break;

        case bsoncxx::type::k_int64:
            *pT = element.get_int64();
            break;

        case bsoncxx::type::k_double:
            *pT = element.get_double();
            break;

        default:
            rv = false;
        }
    }

    return rv;
}

template<>
bool nosql::element_as(const bsoncxx::document::element& element,
                       Conversion conversion,
                       int32_t* pT)
{
    bool rv = true;

    auto type = element.type();

    if (conversion == Conversion::STRICT && type != bsoncxx::type::k_int32)
    {
        rv = false;
    }
    else
    {
        switch (type)
        {
        case bsoncxx::type::k_int32:
            *pT = element.get_int32();
            break;

        case bsoncxx::type::k_int64:
            *pT = element.get_int64();
            break;

        case bsoncxx::type::k_double:
            *pT = element.get_double();
            break;

        default:
            rv = false;
        }
    }

    return rv;
}

template<>
bool nosql::element_as(const bsoncxx::document::element& element,
                       Conversion conversion,
                       string* pT)
{
    bool rv = (element.type() == bsoncxx::type::k_utf8);

    if (rv)
    {
        string_view sv = element.get_utf8();

        *pT = string(sv.data(), sv.length());
    }

    return rv;
}

template<>
bsoncxx::document::view nosql::element_as<bsoncxx::document::view>(const string& command,
                                                                   const char* zKey,
                                                                   const bsoncxx::document::element& element,
                                                                   int error_code,
                                                                   Conversion conversion)
{
    if (conversion == Conversion::STRICT && element.type() != bsoncxx::type::k_document)
    {
        ostringstream ss;
        ss << "BSON field '" << command << "." << zKey << "' is the wrong type '"
           << bsoncxx::to_string(element.type()) << "', expected type 'object'";

        throw SoftError(ss.str(), error_code);
    }

    bsoncxx::document::view doc;

    switch (element.type())
    {
    case bsoncxx::type::k_document:
        doc = element.get_document();
        break;

    case bsoncxx::type::k_null:
        break;

    default:
        {
            ostringstream ss;
            ss << "BSON field '" << command << "." << zKey << "' is the wrong type '"
               << bsoncxx::to_string(element.type()) << "', expected type 'object' or 'null'";

            throw SoftError(ss.str(), error_code);
        }
    }

    return doc;
}

template<>
bsoncxx::array::view nosql::element_as<bsoncxx::array::view>(const string& command,
                                                             const char* zKey,
                                                             const bsoncxx::document::element& element,
                                                             int error_code,
                                                             Conversion)
{
    if (element.type() != bsoncxx::type::k_array)
    {
        ostringstream ss;
        ss << "BSON field '" << command << "." << zKey << "' is the wrong type '"
           << bsoncxx::to_string(element.type()) << "', expected type 'array'";

        throw SoftError(ss.str(), error_code);
    }

    return element.get_array();
}

template<>
string nosql::element_as<string>(const string& command,
                                 const char* zKey,
                                 const bsoncxx::document::element& element,
                                 int error_code,
                                 Conversion)
{
    if (element.type() != bsoncxx::type::k_utf8)
    {
        ostringstream ss;
        ss << "BSON field '" << command << "." << zKey << "' is the wrong type '"
           << bsoncxx::to_string(element.type()) << "', expected type 'string'";

        throw SoftError(ss.str(), error_code);
    }

    const auto& utf8 = element.get_utf8();
    return string(utf8.value.data(), utf8.value.size());
}

template<>
nosql::string_view nosql::element_as<nosql::string_view>(const string& command,
                                                         const char* zKey,
                                                         const bsoncxx::document::element& element,
                                                         int error_code,
                                                         Conversion)
{
    if (element.type() != bsoncxx::type::k_utf8)
    {
        ostringstream ss;
        ss << "BSON field '" << command << "." << zKey << "' is the wrong type '"
           << bsoncxx::to_string(element.type()) << "', expected type 'string'";

        throw SoftError(ss.str(), error_code);
    }

    return element.get_utf8();
}

template<>
int64_t nosql::element_as<int64_t>(const string& command,
                                   const char* zKey,
                                   const bsoncxx::document::element& element,
                                   int error_code,
                                   Conversion conversion)
{
    int64_t rv;

    if (conversion == Conversion::STRICT && element.type() != bsoncxx::type::k_int64)
    {
        ostringstream ss;
        ss << "BSON field '" << command << "." << zKey << "' is the wrong type '"
           << bsoncxx::to_string(element.type()) << "', expected type 'int64'";

        throw SoftError(ss.str(), error_code);
    }

    switch (element.type())
    {
    case bsoncxx::type::k_int32:
        rv = element.get_int32();
        break;

    case bsoncxx::type::k_int64:
        rv = element.get_int64();
        break;

    case bsoncxx::type::k_double:
        rv = element.get_double();
        break;

    default:
        {
            ostringstream ss;
            ss << "BSON field '" << command << "." << zKey << "' is the wrong type '"
               << bsoncxx::to_string(element.type()) << "', expected a number";

            throw SoftError(ss.str(), error_code);
        }
    }

    return rv;
}

template<>
int32_t nosql::element_as<int32_t>(const string& command,
                                   const char* zKey,
                                   const bsoncxx::document::element& element,
                                   int error_code,
                                   Conversion conversion)
{
    int32_t rv;

    if (conversion == Conversion::STRICT && element.type() != bsoncxx::type::k_int32)
    {
        ostringstream ss;
        ss << "BSON field '" << command << "." << zKey << "' is the wrong type '"
           << bsoncxx::to_string(element.type()) << "', expected type 'int32'";

        throw SoftError(ss.str(), error_code);
    }

    switch (element.type())
    {
    case bsoncxx::type::k_int32:
        rv = element.get_int32();
        break;

    case bsoncxx::type::k_int64:
        rv = element.get_int64();
        break;

    case bsoncxx::type::k_double:
        rv = element.get_double();
        break;

    default:
        {
            ostringstream ss;
            ss << "BSON field '" << command << "." << zKey << "' is the wrong type '"
               << bsoncxx::to_string(element.type()) << "', expected a number";

            throw SoftError(ss.str(), error_code);
        }
    }

    return rv;
}

template<>
bool nosql::element_as<bool>(const string& command,
                             const char* zKey,
                             const bsoncxx::document::element& element,
                             int error_code,
                             Conversion conversion)
{
    bool rv = true;

    if (conversion == Conversion::STRICT && element.type() != bsoncxx::type::k_bool)
    {
        ostringstream ss;
        ss << "BSON field '" << command << "." << zKey << "' is the wrong type '"
           << bsoncxx::to_string(element.type()) << "', expected type 'bool'";

        throw SoftError(ss.str(), error_code);
    }

    switch (element.type())
    {
    case bsoncxx::type::k_bool:
        rv = element.get_bool();
        break;

    case bsoncxx::type::k_int32:
        rv = element.get_int32() != 0;
        break;

    case bsoncxx::type::k_int64:
        rv = element.get_int64() != 0;
        break;

    case bsoncxx::type::k_double:
        rv = element.get_double() != 0;
        break;

    case bsoncxx::type::k_null:
        rv = false;
        break;

    default:
        rv = true;
    }

    return rv;
}

template<>
bsoncxx::types::b_binary
nosql::element_as<bsoncxx::types::b_binary>(const std::string& command,
                                            const char* zKey,
                                            const bsoncxx::document::element& element,
                                            int error_code,
                                            Conversion)
{
    if (element.type() != bsoncxx::type::k_binary)
    {
        ostringstream ss;
        ss << "BSON field '" << command << "." << zKey << "' is the wrong type '"
           << bsoncxx::to_string(element.type()) << "', expected type '"
           << bsoncxx::to_string(bsoncxx::type::k_binary) << "'";

        throw SoftError(ss.str(), error_code);
    }

    return element.get_binary();
}
