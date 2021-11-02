/*
 * Copyright (c) 2020 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2025-10-11
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

#include "nosqlupdateoperator.hh"
#include "nosqlbase.hh"
#include "nosql.hh"

using namespace std;

namespace
{

using namespace nosql;

class UpdateOperator
{
public:
    UpdateOperator() = default;

    string convert_bit(const bsoncxx::document::element& element, const string& doc)
    {
        mxb_assert(element.key().compare("$bit") == 0);

        string rv = doc;

        auto fields = static_cast<bsoncxx::document::view>(element.get_document());

        FieldRecorder rec(this);
        for (auto field : fields)
        {
            auto type = field.type();

            if (type != bsoncxx::type::k_document)
            {
                ostringstream ss;
                ss << "The $bit modifier is not compatible with a " << bsoncxx::to_string(type)
                   << ". You must pass in an embedded document: {$bit: {field: {and/or/xor: #}}";

                throw SoftError(ss.str(), error::BAD_VALUE);
            }

            bsoncxx::document::view ops = field.get_document();

            if (ops.begin() == ops.end())
            {
                throw SoftError("You must pass in at least one bitwise operation. "
                                "The format is: {$bit: {field: {and/or/xor: #}}",
                                error::BAD_VALUE);
            }

            for (auto op : ops)
            {
                const char* zOp = nullptr;
                auto name = op.key();

                if (name.compare("and") == 0)
                {
                    zOp = " & ";
                }
                else if (name.compare("or") == 0)
                {
                    zOp = " | ";
                }
                else if (name.compare("xor") == 0)
                {
                    zOp = " ^ ";
                }
                else
                {
                    ostringstream ss;
                    ss << "The $bit modifier only supports 'and', 'or', and 'xor', not '"
                       << name << "' which is an unknown operator: " << bsoncxx::to_json(ops);

                    throw SoftError(ss.str(), error::BAD_VALUE);
                }

                int64_t bits = 0;

                type = op.type();

                switch (type)
                {
                case bsoncxx::type::k_int32:
                    bits = op.get_int32();
                    break;

                case bsoncxx::type::k_int64:
                    bits = op.get_int64();
                    break;

                default:
                    {
                        ostringstream ss;
                        ss << "The $bit modifier field must be an Integer(32/64 bit); a '"
                           << bsoncxx::to_string(type) << " is not supported here: "
                           << bsoncxx::to_json(ops);
                    }
                }

                string_view sv = field.key();
                string key = check_update_path(sv);

                ostringstream ss;

                ss << "IF(JSON_TYPE(JSON_VALUE(" << rv << ", '$." << key << "')) = 'INTEGER',"
                   << "JSON_SET(" << rv << ", '$." << key << "', "
                   << "JSON_VALUE(" << rv << ", '$." << key << "') " << zOp << bits << "), "
                   << rv << ")";

                rv = ss.str();
            }
        }

        rec.flush();

        return rv;
    }

    string convert_current_date(const bsoncxx::document::element& element, const string& doc)
    {
        mxb_assert(element.key().compare("$currentDate") == 0);

        ostringstream ss;

        ss << "JSON_SET(" << doc;

        auto fields = static_cast<bsoncxx::document::view>(element.get_document());

        FieldRecorder rec(this);
        for (auto field : fields)
        {
            ss << ", ";

            string_view sv = field.key();
            string key = check_update_path(sv);

            ss << "'$." << key << "', ";

            auto now = std::chrono::system_clock::now().time_since_epoch().count();

            auto type = field.type();
            switch (type)
            {
            case bsoncxx::type::k_bool:
                ss << "JSON_OBJECT(\"$date\", " << now << ")";
                break;

            case bsoncxx::type::k_document:
                {
                    bsoncxx::document::view spec_doc = field.get_document();
                    auto spec_value = spec_doc["$type"];

                    if (!spec_value || spec_value.type() != bsoncxx::type::k_utf8)
                    {
                        throw SoftError("The '$type' string field is required to be 'date' or "
                                        "'timestamp': {$currentDate: {field : {$type: 'date'}}}",
                                        error::BAD_VALUE);
                    }

                    string_view what = spec_value.get_utf8();

                    if (what.compare("date") == 0)
                    {
                        ss << "JSON_OBJECT(\"$date\", " << now << ")";
                    }
                    else if (what.compare("timestamp") == 0)
                    {
                        ss << "JSON_OBJECT(\"$timestamp\", JSON_OBJECT("
                           << "\"t\", " << now << ", \"i\", 0))";
                    }
                    else
                    {
                        throw SoftError("The '$type' string field is required to be 'date' or "
                                        "'timestamp': {$currentDate: {field : {$type: 'date'}}}",
                                        error::BAD_VALUE);
                    }
                }
                break;

            default:
                {
                    ostringstream ss;
                    ss << bsoncxx::to_string(type) << " is not valid type for $currentDate. "
                       << "Please use a boolean ('true') or a $type expression ({$type: 'timestamp/date'}).";

                    throw SoftError(ss.str(), error::BAD_VALUE);
                }
                break;
            }
        }

        ss << ")";

        rec.flush();

        return ss.str();
    }

    string convert_inc(const bsoncxx::document::element& element, const string& doc)
    {
        mxb_assert(element.key().compare("$inc") == 0);

        return convert_op(element, doc, "increment", " + ");
    }

    string convert_mul(const bsoncxx::document::element& element, const string& doc)
    {
        mxb_assert(element.key().compare("$mul") == 0);

        return convert_op(element, doc, "multiply", " * ");
    }

    string convert_push(const bsoncxx::document::element& element, const string& doc)
    {
        mxb_assert(element.key().compare("$push") == 0);

        string rv = doc;
        auto fields = static_cast<bsoncxx::document::view>(element.get_document());

        FieldRecorder rec(this);
        for (auto field : fields)
        {
            ostringstream ss;

            string_view sv = field.key();
            string key = check_update_path(sv);

            auto value = element_to_value(field, ValueFor::JSON_NESTED);

            ss << "IF(JSON_QUERY(" << rv << ", '$." << key << "') IS NOT NULL, "
               << "JSON_ARRAY_APPEND(" << rv << ", '$." << key << "', " << value << "), "
               << "JSON_SET(" << rv << ", '$." << key << "', JSON_ARRAY("
               << value
               << ")))";

            rv = ss.str();

            rec.push_back(sv);
        }

        rec.flush();

        return rv;
    }

    string convert_rename(const bsoncxx::document::element& element, const string& doc)
    {
        mxb_assert(element.key().compare("$rename") == 0);

        string rv = doc;

        auto fields = static_cast<bsoncxx::document::view>(element.get_document());

        FieldRecorder rec(this);
        for (auto field : fields)
        {
            auto from = field.key();

            if (field.type() != bsoncxx::type::k_utf8)
            {
                ostringstream ss;
                ss << "The 'to' field for $rename must be a string: "
                   << from << ":"
                   << element_to_string(element);

                throw SoftError(ss.str(), error::BAD_VALUE);
            }

            string_view to = field.get_utf8();

            if (from == to)
            {
                ostringstream ss;
                ss << "The source and target field for $rename must differ: " << from << ": \"" << to << "\"";

                throw SoftError(ss.str(), error::BAD_VALUE);
            }

            if (from.length() == 0 || to.length() == 0)
            {
                throw SoftError("An empty update path is not valid.", error::CONFLICTING_UPDATE_OPERATORS);
            }

            if (from.front() == '.' || from.back() == '.' || to.front() == '.' || to.back() == '.')
            {
                string_view path;

                if (from.front() == '.' || from.back() == '.')
                {
                    path = from;
                }
                else
                {
                    path = to;
                }

                ostringstream ss;
                ss << "The update path '" << path << "' contains an empty field name, which is not allowed.";

                throw SoftError(ss.str(), error::BAD_VALUE);
            }

            auto from_parts = mxb::strtok(string(from.data(), from.length()), ".");
            auto to_parts = mxb::strtok(string(to.data(), to.length()), ".");

            from_parts.resize(std::min(from_parts.size(), to_parts.size()));

            auto it = from_parts.begin();
            auto jt = to_parts.begin();

            while (*it == *jt && it != from_parts.end())
            {
                ++it;
                ++jt;
            }

            if (jt == to_parts.end())
            {
                ostringstream ss;
                ss << "The source and target field for $rename must not be on the same path: "
                   << from << ": \"" << to << "\"";

                throw SoftError(ss.str(), error::BAD_VALUE);
            }

            if (from.find('$') != string::npos)
            {
                ostringstream ss;
                ss << "The source field for $rename may not be dynamic: " << from;

                throw SoftError(ss.str(), error::BAD_VALUE);
            }

            if (to.find('$') != string::npos)
            {
                ostringstream ss;
                ss << "The destination field for $rename may not be dynamic: " << to;

                throw SoftError(ss.str(), error::BAD_VALUE);
            }

            string t = check_update_path(to);
            string f = check_update_path(from);

            ostringstream ss;

            string json_set;

            if (to_parts.size() == 1)
            {
                ostringstream ss2;
                ss2 << "JSON_SET(" << rv << ", '$." << t << "', JSON_EXTRACT(" << rv << ", '$." << f << "'))";
                json_set = ss2.str();
            }
            else
            {
                ostringstream ss2;

                // If we have something like '{$rename: {'a.b': 'a.c'}', by explicitly checking whether
                // 'a' is an object, we will end up renaming 'a.b' to 'a.c' (i.e. copy value at 'a.b' to 'a.c'
                // and then delete 'a.b') instead of changing the value of 'a' to '{ c: ... }'. The difference
                // is significant if the document at 'a' contains other fields in addition to 'b'.
                // TODO: This should actually be done for every level.
                string parent_of_t = t.substr(0, t.find_last_of('.'));

                ss2 << "IF(JSON_QUERY(" << rv << ", '$." << parent_of_t << "') IS NOT NULL, "
                    << "JSON_SET(" << rv << ", '$." << t << "', JSON_EXTRACT(" << rv << ", '$." << f << "'))"
                    << ", "
                    << "JSON_SET(" << rv << ", ";

                vector<string> parts = mxb::strtok(t, ".");

                auto it = parts.begin();
                auto end = parts.end() - 1;

                ss2 << "'$." << *it << "', JSON_OBJECT(";

                ++it;

                convert_rename(ss2, rv, f, it, end);

                ss2 << ")))";

                json_set = ss2.str();
            }

            ss << "IF(JSON_EXTRACT(" << rv << ", '$." << f << "') IS NOT NULL, "
               << "JSON_REMOVE(" << json_set << ", '$." << f << "'), "
               << rv << ")";

            rv = ss.str();

            rec.push_back(from);
            rec.push_back(to);
        }

        rec.flush();

        return rv;
    }

    string convert_set(const bsoncxx::document::element& element, const string& doc)
    {
        mxb_assert(element.key().compare("$set") == 0);

        ostringstream ss;

        ss << "JSON_SET(" << doc;

        auto fields = static_cast<bsoncxx::document::view>(element.get_document());

        FieldRecorder rec(this);
        for (auto field : fields)
        {
            ss << ", ";

            string_view sv = field.key();
            string key = check_update_path(sv);

            ss << "'$." << key << "', " << element_to_value(field, ValueFor::JSON_NESTED);

            rec.push_back(sv);
        }

        ss << ")";

        rec.flush();

        return ss.str();
    }

    string convert_unset(const bsoncxx::document::element& element, const string& doc)
    {
        mxb_assert(element.key().compare("$unset") == 0);

        // The prototype is JSON_REMOVE(doc, path[, path] ...) and if a particular
        // path is not present in the document, there should be no effect. However,
        // there is a bug https://jira.mariadb.org/browse/MDEV-22141 that causes
        // NULL to be returned if a path is not present. To work around that bug,
        // JSON_REMOVE(doc, a, b) is conceptually expressed like:
        //
        // (1) Z = IF(JSON_EXTRACT(doc, a) IS NOT NULL, JSON_REMOVE(doc, a), doc)
        // (2) IF(JSON_EXTRACT(Z, b) IS NOT NULL, JSON_REMOVE(Z, b), Z)
        //
        // and in practice (take a deep breath) so that in (2) every occurence of
        // Z is replaced with the IF-statement at (1). Note that in case there is
        // a third path, then on that iteration, "doc" in (2) will be the entire
        // expression we just got in (2). Also note that the "doc" we start with,
        // may be a JSON-function expression in itself...

        string rv = doc;

        auto fields = static_cast<bsoncxx::document::view>(element.get_document());

        FieldRecorder rec(this);
        for (auto field : fields)
        {
            string_view sv = field.key();
            string key = escape_essential_chars(string(sv.data(), sv.length()));

            ostringstream ss;

            ss << "IF(JSON_EXTRACT(" << rv << ", '$." << key << "') IS NOT NULL, "
               << "JSON_REMOVE(" << rv << ", '$." << key << "'), " << rv << ")";

            rv = ss.str();

            rec.push_back(sv);
        }

        rec.flush();

        return rv;
    }

    static string convert(const bsoncxx::document::view& update_operations);

    static vector<string> supported_operators();

    static bool is_supported(const string& name);

private:
    class FieldRecorder
    {
    public:
        FieldRecorder(UpdateOperator* pParent)
            : m_parent(*pParent)
        {
        }

        void flush()
        {
            for (const auto& field : m_fields)
            {
                m_parent.add_update_path(field);
            }

            m_fields.clear();
        }

        void push_back(const string_view& field)
        {
            m_fields.push_back(field);
        }

    private:
        UpdateOperator&     m_parent;
        vector<string_view> m_fields;
    };

    void convert_rename(ostream& out,
                        const string& rv,
                        const string& f,
                        vector<string>::iterator& it,
                        const vector<string>::iterator& end)
    {
        if (it != end)
        {
            out << "\"" << *it << "\", JSON_OBJECT(";

            ++it;

            convert_rename(out, rv, f, it, end);

            out << ")";
        }
        else
        {
            out << "\"" << *it << "\", JSON_EXTRACT(" << rv << ", '$." << f << "')";
        }
    };

    string convert_op(const bsoncxx::document::element& element,
                      const string& doc,
                      const char* zOperation,
                      const char* zOp)
    {

        ostringstream ss;

        ss << "JSON_SET(" << doc;

        auto fields = static_cast<bsoncxx::document::view>(element.get_document());

        FieldRecorder rec(this);
        for (auto field : fields)
        {
            ss << ", ";

            string_view sv = field.key();
            string key = get_key(sv);

            ss << "'$." << key << "', ";

            double d;
            if (element_as(field, Conversion::RELAXED, &d))
            {
                auto value = double_to_string(d);

                ss << "IF(JSON_EXTRACT(" << doc << ", '$." + key + "') IS NOT NULL, "
                   << "JSON_VALUE(" << doc << ", '$." + key + "')" << zOp << value << ", "
                   << value
                   << ")";
            }
            else
            {
                DocumentBuilder value;
                append(value, key, field);

                ostringstream ss;
                ss << "Cannot " << zOperation << " with non-numeric argument: "
                   << bsoncxx::to_json(value.view());

                throw SoftError(ss.str(), error::TYPE_MISMATCH);
            }

            rec.push_back(sv);
        }

        ss << ")";

        rec.flush();

        return ss.str();
    }

    string get_key(const string_view& field)
    {
        string key;

        auto i = field.find('.');

        if (i != string::npos)
        {
            auto copy = string(field.data(), field.length());
            key += copy.substr(0, i);

            ++i;
            string::size_type j;

            do
            {
                j = copy.find('.', i);

                if (is_number(&copy[i]))
                {
                    key += "[";
                    key += copy.substr(i, j - i);
                    key += "]";
                }
                else
                {
                    key += ".";
                    key += copy.substr(i, j - i);
                }

                i = j;

                if (j != string::npos)
                {
                    ++i;
                }
            }
            while (i != string::npos);

            key = escape_essential_chars(std::move(key));
        }
        else
        {
            key = escape_essential_chars(field);
        }

        return key;
    }

    void add_update_path(const string_view& field)
    {
        string f = string(field.data(), field.length());

        if (f == "_id")
        {
            throw SoftError("Performing an update on the path '_id' would modify the immutable field '_id'",
                            error::IMMUTABLE_FIELD);
        }

        m_paths.insert(f);

        auto i = f.find('.');

        if (i != string::npos)
        {
            m_paths.insert(f.substr(0, i));
        }
    }

    string check_update_path(const string_view& field)
    {
        string f = string(field.data(), field.length());

        auto it = m_paths.find(f);

        if (it == m_paths.end())
        {
            auto i = f.find('.');

            if (i != string::npos)
            {
                if (!is_number(f.substr(i + 1, f.find('.', i + 2))))
                {
                    it = m_paths.find(f.substr(0, i));
                }
            }
        }

        if (it != m_paths.end())
        {
            ostringstream ss;
            ss << "Updating the path '" << field << "' would create a conflict at '" << *it << "'";

            throw SoftError(ss.str(), error::CONFLICTING_UPDATE_OPERATORS);
        }

        return escape_essential_chars(get_key(field));
    }

    bool is_number(const char* z)
    {
        char* zEnd;
        auto l = strtol(z, &zEnd, 10);

        return ((*zEnd == 0 || *zEnd == '.') && l >= 0 && l != LONG_MAX) ? true : false;
    }

    bool is_number(const string& s)
    {
        return is_number(s.c_str());
    }

    using Converter = std::string (UpdateOperator::*)(const bsoncxx::document::element& element,
                                                      const std::string& doc);

    static unordered_map<string, Converter> s_converters;

    unordered_set<string> m_paths;
};

unordered_map<string, UpdateOperator::Converter> UpdateOperator::s_converters =
{
    { "$bit",         &UpdateOperator::convert_bit },
    { "$currentDate", &UpdateOperator::convert_current_date },
    { "$inc",         &UpdateOperator::convert_inc },
    { "$mul",         &UpdateOperator::convert_mul },
    { "$push",        &UpdateOperator::convert_push },
    { "$rename",      &UpdateOperator::convert_rename },
    { "$set",         &UpdateOperator::convert_set },
    { "$unset",       &UpdateOperator::convert_unset }
};

//static
string UpdateOperator::convert(const bsoncxx::document::view& update_operations)
{
    string rv;

    UpdateOperator update_operator;

    for (auto element : update_operations)
    {
        if (rv.empty())
        {
            rv = "doc";
        }

        auto key = element.key();
        auto it = s_converters.find(string(key.data(), key.length()));
        mxb_assert(it != s_converters.end());

        auto mem_fun = it->second;

        rv = (update_operator.*mem_fun)(element, rv);
    }

    rv += " ";

    return rv;
}

//static
vector<string> UpdateOperator::supported_operators()
{
    vector<string> operators;
    for (auto kv: s_converters)
    {
        operators.push_back(kv.first);
    }

    return operators;
}

//static
bool UpdateOperator::is_supported(const string& name)
{
    return s_converters.find(name) != s_converters.end();
}

}

namespace nosql
{

bool update_operator::is_supported(const std::string& name)
{
    return UpdateOperator::is_supported(name);
}

std::vector<std::string> update_operator::supported_operators()
{
    return UpdateOperator::supported_operators();
}

std::string update_operator::convert(const bsoncxx::document::view& update_operators)
{
    return UpdateOperator::convert(update_operators);
}

}
