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

#include "nosqlmatch.hh"
#include <map>
#include <sstream>
#include "nosqlbase.hh"
#include "nosqlcommon.hh"
#include "nosqlfieldpath.hh"
#include "nosqlnobson.hh"

using namespace std;

namespace nosql
{

namespace condition
{

namespace
{

#define NOSQL_CONDITION(C) { C::NAME, C::create }

map<string, Match::Condition::Creator, less<>> top_level_conditions =
{
    NOSQL_CONDITION(AlwaysFalse),
    NOSQL_CONDITION(AlwaysTrue),
    NOSQL_CONDITION(And),
    NOSQL_CONDITION(Or),
    NOSQL_CONDITION(Nor)
};

map<string, Match::Condition::Creator, less<>> match_conditions =
{
    NOSQL_CONDITION(AlwaysFalse),
    NOSQL_CONDITION(AlwaysTrue),
    NOSQL_CONDITION(And),
    NOSQL_CONDITION(Or),
    NOSQL_CONDITION(Nor)
};

}
}

class FieldCondition : public Match::Condition
{
public:
    FieldCondition(string_view field_path, const BsonView& view)
        : m_field_path(field_path, FieldPath::Mode::WITHOUT_DOLLAR)
        , m_view(view)
    {
    }

    string generate_sql() const override
    {
        string condition;

        const string& head = m_field_path.head();
        const auto* pTail = m_field_path.tail();
        auto type = m_view.type();

        if (head == "_id" && pTail == nullptr && type != bsoncxx::type::k_document)
        {
            condition = "( id = '";

            bool is_utf8 = (type == bsoncxx::type::k_utf8);

            if (is_utf8)
            {
                condition += "\"";
            }

            auto id = nosql::element_to_string(m_view);

            condition += id;

            if (is_utf8)
            {
                condition += "\"";
            }

            condition += "'";

            if (is_utf8 && id.length() == 24 && nosql::is_hex(id))
            {
                // This sure looks like an ObjectId. And this is the way it will appear
                // if a search is made using a DBPointer. So we'll cover that case as well.

                condition += " OR id = '{\"$oid\":\"" + id + "\"}'";
            }

            condition += ")";
        }
        else
        {
            Path path(m_field_path.path(), m_view);

            condition = path.get_comparison_condition();
        }

        return condition;
    }

    bool matches(bsoncxx::document::view doc) override
    {
        mxb_assert(!true);
        return false;
    }

private:
    FieldPath m_field_path;
    BsonView  m_view;
};

}

namespace
{

using namespace nosql;

void require_1(const Match::Condition::BsonView& view, const char* zCondition)
{
    int32_t number = -1;

    switch (view.type())
    {
    case bsoncxx::type::k_int32:
        number = view.get_int32();
        break;

    case bsoncxx::type::k_int64:
        number = view.get_int64();
        break;

    case bsoncxx::type::k_double:
        {
            number = view.get_double();

            if (number != view.get_double())
            {
                number = -1;
            }
        }
        break;

    case bsoncxx::type::k_decimal128:
        {
            bsoncxx::decimal128 d128 = view.get_decimal128().value;

            if (d128 == bsoncxx::decimal128("1"))
            {
                number = 1;
            }
        }
        break;

    default:
        {
            stringstream serr;
            serr << "Expected a number in: " << zCondition << ": " << nobson::to_bson_expression(view);

            throw SoftError(serr.str(), error::FAILED_TO_PARSE);
        }
    }

    if (number != 1)
    {
        stringstream serr;
        serr << zCondition << " must be an integer value of 1";

        throw SoftError(serr.str(), error::FAILED_TO_PARSE);
    }
}

}

namespace nosql
{

/*
 * Match
 */
Match::Match(bsoncxx::document::view match)
    : m_conditions(create(match))
{
}

std::string Match::sql() const
{
    if (m_sql.empty())
    {
        auto it = m_conditions.begin();
        auto end = m_conditions.end();

        while (it != end)
        {
            auto& sCondition = *it;

            string sql = sCondition->generate_sql();

            if (sql.empty())
            {
                m_sql.clear();
                break;
            }
            else if (!m_sql.empty())
            {
                m_sql += " AND ";
            }

            m_sql += sql;

            ++it;
        }

        if (m_sql.empty())
        {
            m_sql = "true";
        }
    }

    return m_sql;
}

bool Match::matches(bsoncxx::document::view doc)
{
    bool rv = true;

    for (const auto& sCondition : m_conditions)
    {
        if (!sCondition->matches(doc))
        {
            rv = false;
            break;
        }
    }

    return rv;
}

//static
Match::SConditions Match::create(bsoncxx::document::view doc)
{
    SConditions conditions;

    for (const auto& element : doc)
    {
        conditions.emplace_back(Condition::create(element));
    }

    return conditions;
}

/*
 * Match::Condition
 */

//static
Match::SCondition Match::Condition::create(string_view name, const BsonView& view)
{
    SCondition sCondition;

    if (!name.empty() && name.front() == '$')
    {
        auto it = nosql::condition::top_level_conditions.find(name);

        if (it == nosql::condition::top_level_conditions.end())
        {
            ostringstream serr;
            serr << "unknown top level operator: " << name;

            throw SoftError(serr.str(), error::BAD_VALUE);
        }

        sCondition = it->second(view);
    }
    else
    {
        sCondition = std::make_unique<FieldCondition>(name, view);
    }

    return sCondition;
}

//static
Match::SCondition Match::Condition::create(bsoncxx::document::view doc)
{
    SConditions conditions;

    for (const auto& element : doc)
    {
        conditions.emplace_back(create(element));
    }

    SCondition sCondition;

    switch (conditions.size())
    {
    case 0:
        sCondition = std::make_unique<condition::AlwaysTrue>();
        break;

    case 1:
        sCondition = std::move(conditions.front());
        break;

    default:
        sCondition = std::make_unique<condition::And>(std::move(conditions));
    }

    return sCondition;
}

Match::SConditions Match::Condition::logical_condition(const BsonView& view, const char* zOp)
{
    if (view.type() != bsoncxx::type::k_array)
    {
        ostringstream serr;
        serr << zOp << " must be an array";

        throw SoftError(serr.str(), error::BAD_VALUE);
    }

    bsoncxx::array::view array = view.get_array();

    auto begin = array.begin();
    auto end = array.end();

    if (begin == end)
    {
        throw SoftError("$and/$or/$nor must be a nonempty array", error::BAD_VALUE);
    }

    SConditions conditions;

    for (auto it = begin; it != end; ++it)
    {
        auto& element = *it;

        if (element.type() != bsoncxx::type::k_document)
        {
            throw SoftError("$or/$and/$nor entries need to be full objects", error::BAD_VALUE);
        }

        conditions.emplace_back(create(element.get_document()));
    }

    return conditions;
}


namespace condition
{

/*
 * LogicalCondition
 */

/**
 * AlwaysFalse
 */
AlwaysFalse::AlwaysFalse(const BsonView& view)
{
    require_1(view, NAME);
}

string AlwaysFalse::generate_sql() const
{
    return "false";
}

bool AlwaysFalse::matches(bsoncxx::document::view doc)
{
    mxb_assert(!true);
    return false;
}

/**
 * AlwaysTrue
 */
AlwaysTrue::AlwaysTrue()
{
}

AlwaysTrue::AlwaysTrue(const BsonView& view)
{
    require_1(view, NAME);
}

string AlwaysTrue::generate_sql() const
{
    return "true";
}

bool AlwaysTrue::matches(bsoncxx::document::view doc)
{
    mxb_assert(!true);
    return false;
}

/**
 * And
 */
bool And::matches(bsoncxx::document::view doc)
{
    mxb_assert(!true);
    return false;
}

void And::add_sql(string& sql, const string& condition) const
{
    if (!sql.empty())
    {
        sql += " AND ";
    }

    sql += condition;
}

/**
 * Or
 */
bool Or::matches(bsoncxx::document::view doc)
{
    mxb_assert(!true);
    return false;
}

void Or::add_sql(string& sql, const string& condition) const
{
    if (!sql.empty())
    {
        sql += " OR ";
    }

    sql += condition;
}

/**
 * Nor
 */
bool Nor::matches(bsoncxx::document::view doc)
{
    mxb_assert(!true);
    return false;
}

void Nor::add_sql(string& sql, const string& condition) const
{
    if (!sql.empty())
    {
        sql += " AND ";
    }

    sql += "NOT " + condition;
}

}

}
