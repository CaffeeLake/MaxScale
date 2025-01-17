/*
 * Copyright (c) 2024 MariaDB plc
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

#include "nosqlaggregationstage.hh"
#include <map>
#include <random>
#include <sstream>
#include <maxscale/config.hh>
#include "../../filter/masking/mysql.hh"
#include "nosqlcommon.hh"
#include "nosqlnobson.hh"

using namespace std;

namespace nosql
{

namespace aggregation
{

namespace
{

using StageCreator = unique_ptr<Stage>(*)(bsoncxx::document::element, Stage*);
using Stages = map<string_view, StageCreator, less<>>;

#define NOSQL_STAGE(O) { O::NAME, O::create }

Stages stages =
{
    NOSQL_STAGE(AddFields),
    NOSQL_STAGE(CollStats),
    NOSQL_STAGE(Count),
    NOSQL_STAGE(Group),
    NOSQL_STAGE(Limit),
    NOSQL_STAGE(ListSearchIndexes),
    NOSQL_STAGE(Match),
    NOSQL_STAGE(Project),
    NOSQL_STAGE(Sample),
    NOSQL_STAGE(Skip),
    NOSQL_STAGE(Sort),
    NOSQL_STAGE(Unset),
    NOSQL_STAGE(Unwind),
};

}

/**
 * Stage
 */

void Stage::Query::reset(std::string_view database, std::string_view table)
{
    mxb_assert(is_malleable());

    m_database = database;
    m_table = table;

    reset();
}

void Stage::Query::reset()
{
    mxb_assert(is_malleable());

    m_is_modified = false;
    m_column = "doc";
    m_from.clear();
    m_where.clear();
    m_order_by.clear();
    m_limit = MAX_LIMIT;
}

string Stage::Query::sql() const
{
    stringstream ss;
    ss << "SELECT " << column() << " AS doc FROM " << from();
    auto w = where();

    if (!w.empty())
    {
        ss << " WHERE " << w;
    }

    auto o = order_by();

    if (!o.empty())
    {
        ss << " ORDER BY " << o;
    }

    auto l = limit();

    if (l != MAX_LIMIT)
    {
        ss << " LIMIT " << l;
    }

    auto s = skip();

    if (s != 0)
    {
        if (l == MAX_LIMIT)
        {
            ss << " LIMIT " << MAX_LIMIT;
        }

        ss << " OFFSET " << s;
    }

    return ss.str();
}

Stage::~Stage()
{
}

//static
unique_ptr<Stage> Stage::get(bsoncxx::document::element element, Stage* pPrevious)
{
    auto it = stages.find(element.key());

    if (it == stages.end())
    {
        stringstream ss;
        ss << "Unrecognized pipeline stage name: '" << element.key() << "'";

        throw SoftError(ss.str(), error::LOCATION40324);
    }

    return it->second(element, pPrevious);
}

//static
std::vector<bsoncxx::document::value> Stage::process_resultset(GWBUF&& mariadb_response)
{
    uint8_t* pBuffer = mariadb_response.data();

    ComResponse response(pBuffer);

    if (response.type() == ComResponse::ERR_PACKET)
    {
        throw MariaDBError(ComERR(response));
    }

    ComQueryResponse cqr(&pBuffer);
    auto nFields = cqr.nFields();
    mxb_assert(nFields == 1);

    vector<enum_field_types> types;

    for (size_t i = 0; i < nFields; ++i)
    {
        ComQueryResponse::ColumnDef column_def(&pBuffer);

        types.push_back(column_def.type());
    }

    ComResponse eof(&pBuffer);
    mxb_assert(eof.type() == ComResponse::EOF_PACKET);

    vector<bsoncxx::document::value> docs;

    while (ComResponse(pBuffer).type() != ComResponse::EOF_PACKET)
    {
        CQRTextResultsetRow row(&pBuffer, types); // Advances pBuffer
        auto it = row.begin();

        bsoncxx::document::value doc = bsoncxx::from_json((*it++).as_string().to_string());
        docs.emplace_back(std::move(doc));
    }

    return docs;
}


/**
 * AddFields
 */
AddFields::AddFields(bsoncxx::document::element element, Stage* pPrevious)
    : PipelineStage(pPrevious)
{
    if (element.type() != bsoncxx::type::k_document)
    {
        stringstream ss;
        ss << "$addFields specification stage must be an object, got "
           << bsoncxx::to_string(element.type());

        throw SoftError(ss.str(), error::LOCATION40272);
    }

    bsoncxx::document::view add_field = element.get_document();

    for (auto it = add_field.begin(); it != add_field.end(); ++it)
    {
        auto def = *it;

        m_operators.emplace_back(NamedOperator { def.key(), Operator::create(def.get_value()) });
    }
}

std::vector<bsoncxx::document::value> AddFields::process(std::vector<bsoncxx::document::value>& in)
{
    vector<bsoncxx::document::value> out;

    for (const bsoncxx::document::value& in_doc : in)
    {
        DocumentBuilder out_doc;

        for (auto element : in_doc)
        {
            out_doc.append(kvp(element.key(), element.get_value()));
        }

        for (const NamedOperator& nop : m_operators)
        {
            nop.sOperator->append(out_doc, nop.name, in_doc);
        }

        out.emplace_back(out_doc.extract());
    }

    return out;
}

/**
 * CollStats
 */
CollStats::CollStats(bsoncxx::document::element element, Stage* pPrevious)
    : SQLStage(pPrevious)
{
    if (pPrevious)
    {
        throw SoftError("$collStats is only valid as the first stage in a pipeline",
                        error::LOCATION40602);
    }

    if (element.type() != bsoncxx::type::k_document)
    {
        stringstream ss;
        ss << "$collStats must take a nested object but found a "
           << bsoncxx::to_string(element.type());

        throw SoftError(ss.str(), error::LOCATION5447000);
    }

    bsoncxx::document::view coll_stats = element.get_document();

    bsoncxx::document::view latency_stats;
    if (nosql::optional("$collStats", coll_stats, "latencyStats", &latency_stats))
    {
        m_include |= Include::LATENCY_STATS;
    }

    bsoncxx::document::view storage_stats;
    if (nosql::optional("$collStats", coll_stats, "storageStats", &storage_stats))
    {
        m_include |= Include::STORAGE_STATS;
    }

    bsoncxx::document::view count;
    if (nosql::optional("$collStats", coll_stats, "count", &count))
    {
        m_include |= Include::COUNT;
    }
}

bool CollStats::update(Query& query) const
{
    mxb_assert(query.is_malleable());

    auto now = std::chrono::system_clock::now();
    bsoncxx::types::b_date iso_date(now);

    const auto& config = mxs::Config::get();

    stringstream column;
    column <<
        "JSON_OBJECT("
        "'ns', '" << query.database() << "." << query.table() << "', "
        "'host', '" << config.nodename << ":17017" << "', " // TODO: Make port available.
        "'localTime', JSON_OBJECT('$date', " << iso_date.to_int64() << ")";

    if (m_include & Include::LATENCY_STATS)
    {
        column <<
            ", 'latencyStats', "
            "JSON_OBJECT("
            "'reads', JSON_OBJECT('latency', 0, 'ops', 0, 'queryableEncryptionLatencyMicros', 0), "
            "'writes', JSON_OBJECT('latency', 0, 'ops', 0, 'queryableEncryptionLatencyMicros', 0), "
            "'commands', JSON_OBJECT('latency', 0, 'ops', 0, 'queryableEncryptionLatencyMicros', 0), "
            "'transactions', JSON_OBJECT('latency', 0, 'ops', 0, 'queryableEncryptionLatencyMicros', 0)"
            ")";
    }

    if (m_include & Include::STORAGE_STATS)
    {
        column <<
            ", 'storageStats', "
            "JSON_OBJECT("
            "'size', data_length + index_length, "
            "'count', table_rows, "
            "'avgObjSize', avg_row_length, "
            "'numOrphanDocs', 0, "
            "'storageSize', data_length + index_length, "
            "'totalIndexSize', index_length, "
            "'freeStorageSize', 0, "
            "'nindexes', 1, "
            "'capped', false"
            ")";
    }

    if (m_include & Include::COUNT)
    {
        column <<
            ", 'count', table_rows";
    }

    column << ")";

    stringstream where;
    where <<
        "information_schema.tables.table_schema = '" << query.database() << "' "
        "AND information_schema.tables.table_name = '" << query.table() << "'";

    query.set_column(column.str());
    query.set_from("information_schema.tables");
    query.set_where(where.str());

    query.freeze();

    return true;
}

/**
 * Count
 */
Count::Count(bsoncxx::document::element element, Stage* pPrevious)
    : DualStage(pPrevious)
{
    if (element.type() == bsoncxx::type::k_string)
    {
        m_field = element.get_string();
    }

    if (m_field.empty())
    {
        throw SoftError("the count field must be a non-empty string", error::LOCATION40156);
    }

    if (m_field.find('.') != string::npos)
    {
        throw SoftError("the count field cannot contain '.'", error::LOCATION40160);
    }
}

bool Count::update(Query& query) const
{
    mxb_assert(is_sql() && query.is_malleable());

    bool rv = false;

    if (query.column() == "doc")
    {
        stringstream ss;
        ss << "JSON_OBJECT('" << m_field << "', COUNT(*))";

        query.set_column(ss.str());
        rv = true;
    }

    return rv;
}

std::vector<bsoncxx::document::value> Count::process(std::vector<bsoncxx::document::value>& in)
{
    vector<bsoncxx::document::value> rv;

    int32_t nCount = in.size();

    DocumentBuilder doc;
    doc.append(kvp(m_field, nCount));

    rv.emplace_back(doc.extract());

    return rv;
}

/**
 * Group
 */
Group::Operators Group::s_available_operators =
{
    { "$addToSet",               static_cast<OperatorCreator>(nullptr) },
    { accumulation::Avg::NAME,   accumulation::Avg::create },
    { accumulation::First::NAME, accumulation::First::create },
    { accumulation::Last::NAME,  accumulation::Last::create },
    { accumulation::Max::NAME,   accumulation::Max::create },
    { "$mergeObjects",           static_cast<OperatorCreator>(nullptr) },
    { accumulation::Min::NAME,   accumulation::Min::create },
    { accumulation::Push::NAME,  accumulation::Push::create },
    { "$stdDevPop",              static_cast<OperatorCreator>(nullptr) },
    { accumulation::Sum::NAME,   accumulation::Sum::create },
};

Group::Group(bsoncxx::document::element element, Stage* pPrevious)
    : PipelineStage(pPrevious)
{
    if (element.type() != bsoncxx::type::k_document)
    {
        throw SoftError("a group's fields must be specified in an object", error::LOCATION15947);
    }

    m_group = element.get_document();

    auto id = m_group["_id"];

    if (!id)
    {
        throw SoftError("a group specification must include an _id", error::LOCATION15955);
    }

    m_sId = Operator::create(id.get_value());

    // Create one set of operators immediately, so that the whole process will be terminated
    // with an exception in case there is some problem.
    m_operators = create_operators();
}

namespace
{

struct BsonValueLess
{
    bool operator()(const bsoncxx::types::bson_value::value& lhs,
                    const bsoncxx::types::bson_value::value& rhs) const
    {
        return lhs.view() < rhs.view();
    }
};

}

vector<bsoncxx::document::value> Group::process(vector<bsoncxx::document::value>& docs)
{
    struct OneGroup
    {
        OneGroup(vector<NamedOperator>&& ops)
            : operators(move(ops))
        {
        }

        std::vector<bsoncxx::document::value> docs;
        std::vector<NamedOperator>            operators;
    };

    std::map<bsoncxx::types::bson_value::value, OneGroup, BsonValueLess> groups;

    // First sort documents per id.
    for (bsoncxx::document::value& doc : docs)
    {
        auto id = m_sId->process(doc);

        std::vector<bsoncxx::document::value>* pDocs = nullptr;

        if (groups.empty())
        {
            // Just use the one we created in the constructor.
            groups.emplace(make_pair(std::move(id), OneGroup (std::move(m_operators))));
            pDocs = &groups.begin()->second.docs;
        }
        else
        {
            auto it = groups.find(id);

            if (it == groups.end())
            {
                it = groups.emplace(make_pair(move(id), OneGroup (create_operators()))).first;
            }

            pDocs = &it->second.docs;
        }

        pDocs->emplace_back(move(doc));
    }

    // Then process each group separately.
    for (auto& kv : groups)
    {
        auto& group = kv.second;

        int32_t n = group.docs.size();
        int32_t i = 0;
        for (auto& doc : group.docs)
        {
            for (const NamedOperator& nop : group.operators)
            {
                nop.sOperator->accumulate(doc, i, n);
            }
            ++i;
        }
    }

    // Finally collect the results.
    vector<bsoncxx::document::value> rv;

    for (auto& kv : groups)
    {
        DocumentBuilder doc;

        auto& id = kv.first;
        auto& group = kv.second;

        doc.append(kvp("_id", id));

        for (const NamedOperator& nop : group.operators)
        {
            bsoncxx::types::value value = nop.sOperator->finish();

            doc.append(kvp(nop.name, value));
        }

        rv.emplace_back(doc.extract());
    }

    return rv;
}

vector<Group::NamedOperator> Group::create_operators()
{
    vector<NamedOperator> rv;

    for (auto operator_def : m_group)
    {
        if (operator_def.key() == "_id")
        {
            continue;
        }

        string_view name = operator_def.key();

        if (operator_def.type() != bsoncxx::type::k_document)
        {
            stringstream ss;
            ss << "The field '" << name << "' must be an accumulator object";

            SoftError(ss.str(), error::LOCATION40234);
        }

        rv.emplace_back(create_operator(name, operator_def.get_document()));
    }

    return rv;
}

Group::NamedOperator Group::create_operator(std::string_view name, bsoncxx::document::view def)
{
    auto it = def.begin();

    if (it == def.end())
    {
        stringstream ss;
        ss << "The field '" << name << "' must specify one accumulator";

        throw SoftError(ss.str(), error::LOCATION40238);
    }

    auto element = *it;

    auto jt = s_available_operators.find(element.key());

    if (jt == s_available_operators.end())
    {
        stringstream ss;
        ss << "Unknown group operator '" << element.key() << "'";

        throw SoftError(ss.str(), error::LOCATION15952);
    }

    OperatorCreator create = jt->second;

    if (!create)
    {
        Operator::unsupported(element.key());
    }

    auto sOperator = create(element.get_value());

    return NamedOperator { name, std::move(sOperator) };
}

/**
 * Limit
 */
Limit::Limit(bsoncxx::document::element element, Stage* pPrevious)
    : DualStage(pPrevious)
{
    if (!nobson::get_number(element, &m_nLimit))
    {
        stringstream ss;
        ss << "invalid argument to $limit stage: Expected a number in: $limit: "
           << nobson::to_bson_expression(element.get_value());

        throw SoftError(ss.str(), error::LOCATION2107201);
    }

    if (m_nLimit < 0)
    {
        stringstream ss;
        ss << "invalid argument to $limit stage: Expected a non-negative number in: $limit: " << m_nLimit;

        throw SoftError(ss.str(), error::LOCATION5107201);
    }
    else if (m_nLimit == 0)
    {
        throw SoftError("the limit must be positive", error::LOCATION15958);
    }
}

bool Limit::update(Query& query) const
{
    bool rv = false;
    mxb_assert(is_sql() && query.is_malleable());

    if (query.order_by().empty())
    {
        auto limit = query.limit();

        if (m_nLimit < limit)
        {
            query.set_limit(m_nLimit);
        }

        rv = true;
    }

    return rv;
}

std::vector<bsoncxx::document::value> Limit::process(std::vector<bsoncxx::document::value>& in)
{
    if (in.size() > (size_t)m_nLimit)
    {
        in.erase(in.begin() + m_nLimit, in.end());
    }

    return std::move(in);
}

/**
 * ListSearchIndexes
 */
ListSearchIndexes::ListSearchIndexes(bsoncxx::document::element element, Stage* pPrevious)
    : UnsupportedStage("listSearchIndexes stage is only allowed on MongoDB Atlas", error::LOCATION6047401)
{
}

/**
 * Match
 */
Match::Match(bsoncxx::document::element element, Stage* pPrevious)
    : DualStage(pPrevious)
    , m_match(get_document(element))
{
}

bool Match::update(Query& query) const
{
    mxb_assert(is_sql() && query.is_malleable());

    string where = query.where();

    if (!where.empty())
    {
        where += " AND ";
    }

    where += m_match.sql();

    query.set_where(where);

    return true;
}

std::vector<bsoncxx::document::value> Match::process(std::vector<bsoncxx::document::value>& in)
{
    vector<bsoncxx::document::value> out;

    for (bsoncxx::document::value& doc : in)
    {
        if (m_match.matches(doc))
        {
            out.emplace_back(std::move(doc));
        }
    }

    return out;
}

//static
bsoncxx::document::view Match::get_document(const bsoncxx::document::element& element)
{
    if (element.type() != bsoncxx::type::k_document)
    {
        throw SoftError("the match filter must be an expression in a object", error::LOCATION15959);
    }

    return element.get_document();
}

/**
 * Project
 */
Project::Project(bsoncxx::document::element element, Stage* pPrevious)
    : DualStage(pPrevious)
{
    if (element.type() != bsoncxx::type::k_document)
    {
        throw SoftError("$project specification must be an object", error::LOCATION15969);
    }

    construct(element.get_document());
}

Project::Project(const bsoncxx::document::view& project, Stage* pPrevious)
    : DualStage(pPrevious)
{
    construct(project);
}

Project::Project(Extractions&& extractions)
    : DualStage(nullptr)
    , m_extractions(std::move(extractions))
{
}

void Project::construct(const bsoncxx::document::view& project)
{
    if (project.empty())
    {
        throw SoftError("projection specification must have "
                        "at least one field", error::LOCATION51272);
    }

    m_extractions = Extractions::from_projection(project);
}

bool Project::update(Query& query) const
{
    mxb_assert(is_sql() && query.is_malleable());
    mxb_assert(!m_extractions.empty());

    auto p = m_extractions.generate_column(query.column());

    query.set_column(p.first);

    bool rv = (p.second == Extractions::Projection::COMPLETE);

    if (!rv)
    {
        query.freeze();
    }

    return rv;
}

vector<bsoncxx::document::value> Project::process(vector<bsoncxx::document::value>& in)
{
    vector<bsoncxx::document::value> out;

    if (m_extractions.is_including())
    {
        out = include(in);
    }
    else
    {
        mxb_assert(m_extractions.is_excluding());

        out = exclude(in);
    }

    return out;
}

namespace
{

class IncludingBuilder
{
public:
    IncludingBuilder(const Extractions& extractions)
        : m_pExtractions(&extractions)
        , m_pParent(nullptr)
    {
        mxb_assert(extractions.is_including());

        for (const auto& extraction : extractions)
        {
            if (!extraction.is_exclude()) // There may be an exclude for "_id"
            {
                string name = extraction.name();
                auto pos = name.rfind('.');

                if (pos != name.npos)
                {
                    get_descendant(name.substr(0, pos));
                }
            }
        }
    }

    IncludingBuilder(string_view key, IncludingBuilder* pParent)
        : m_pExtractions(nullptr)
        , m_key(key)
        , m_pParent(pParent)
    {
    }

    bsoncxx::document::value build(bsoncxx::document::view doc)
    {
        mxb_assert(m_pExtractions);

        for (const auto& extraction : *m_pExtractions)
        {
            switch (extraction.action())
            {
            case Extraction::Action::EXCLUDE:
                break;

            case Extraction::Action::INCLUDE:
                {
                    auto element = get(extraction.name(), doc);

                    if (element)
                    {
                        IncludingBuilder* pBuilder = builder_for(extraction.name());
                        mxb_assert(pBuilder);

                        pBuilder->append(element.key(), element.get_value());
                    }
                }
                break;

            case Extraction::Action::REPLACE:
                {
                    auto name = extraction.name();
                    IncludingBuilder* pBuilder = builder_for(name);
                    mxb_assert(pBuilder);

                    string key;
                    auto pos = name.rfind('.');

                    if (pos == name.npos)
                    {
                        key = name;
                    }
                    else
                    {
                        key = name.substr(pos + 1);
                    }

                    pBuilder->append(key, extraction, doc);
                }
            }
        }

        return extract();
    }

private:
    bsoncxx::document::value extract()
    {
        for (auto& kv : m_children)
        {
            auto doc = kv.second->extract();

            if (!doc.empty())
            {
                m_builder.append(kvp(kv.first, doc));
            }
        }

        return m_builder.extract();
    }

    void append(std::string_view key, const Extraction& extraction, const bsoncxx::document::view& doc)
    {
        extraction.append(m_builder, key, doc);
    }

    void append(std::string_view key, bsoncxx::types::bson_value::view value)
    {
        m_builder.append(kvp(key, value));
    }

    bsoncxx::document::element get(string_view path, bsoncxx::document::view doc)
    {
        bsoncxx::document::element rv;

        auto pos = path.find('.');

        if (pos == path.npos)
        {
            rv = doc[path];
        }
        else
        {
            auto head = path.substr(0, pos);

            auto element = doc[head];

            if (element && element.type() == bsoncxx::type::k_document)
            {
                doc = element.get_document();

                rv = get(path.substr(pos + 1), element.get_document());
            }
        }

        return rv;
    }

    IncludingBuilder* builder_for(string_view path)
    {
        IncludingBuilder* pBuilder;

        auto pos = path.find('.');

        if (pos == path.npos)
        {
            pBuilder = this;
        }
        else
        {
            auto child = path.substr(0, pos);
            auto it = m_children.find(path.substr(0, pos));
            mxb_assert(it != m_children.end());

            pBuilder = it->second->builder_for(path.substr(pos + 1));
        }

        return pBuilder;
    }

    IncludingBuilder* get_descendant(string_view name)
    {
        auto pos = name.find('.');
        auto head = name.substr(0, pos);

        IncludingBuilder* pDescendant = get_child(head);

        if (pos != name.npos)
        {
            auto tail = name.substr(pos + 1);

            pDescendant = pDescendant->get_descendant(tail);
        }

        return pDescendant;
    }

    IncludingBuilder* get_child(string_view name)
    {
        mxb_assert(name.find('.') == string_view::npos);

        IncludingBuilder* pChild = nullptr;

        auto it = m_children.find(name);

        if (it != m_children.end())
        {
            pChild = it->second.get();
        }
        else
        {
            auto p = m_children.insert(make_pair(name, make_unique<IncludingBuilder>(name, this)));
            auto jt = p.first;
            pChild = jt->second.get();
        }

        return pChild;
    }

    using SIncludingBuilder = std::unique_ptr<IncludingBuilder>;
    using Children = std::map<string, SIncludingBuilder, less<>>;

    const Extractions* m_pExtractions;
    string             m_key;
    IncludingBuilder*  m_pParent;
    DocumentBuilder    m_builder;
    Children           m_children;
};

}

vector<bsoncxx::document::value> Project::include(vector<bsoncxx::document::value>& in)
{
    IncludingBuilder builder(m_extractions);

    vector<bsoncxx::document::value> out;

    for (auto& doc : in)
    {
        out.emplace_back(builder.build(doc));
    }

    return out;
}

namespace
{

class ExcludingBuilder
{
public:
    ExcludingBuilder(const Extractions& extractions)
    {
        mxb_assert(extractions.is_excluding());

        for (const auto& extraction : extractions)
        {
            m_extractions.insert(make_pair(extraction.name(), &extraction));
        }
    }

    bsoncxx::document::value build(bsoncxx::document::view doc)
    {
        return build("", doc);
    }

private:
    bsoncxx::document::value build(string_view scope, bsoncxx::document::view doc)
    {
        DocumentBuilder builder;

        for (auto element : doc)
        {
            string path(scope);

            if (!path.empty())
            {
                path += ".";
            }

            path += element.key();

            auto it = m_extractions.find(path);

            const Extraction* pExtraction = (it != m_extractions.end() ? it->second : nullptr);

            if (!pExtraction || pExtraction->is_include()) // There may be an including "_id".
            {
                if (element.type() == bsoncxx::type::k_document)
                {
                    builder.append(kvp(element.key(), build(path, element.get_document())));
                }
                else
                {
                    builder.append(kvp(element.key(), element.get_value()));
                }
            }
        }

        return builder.extract();
    }

    map<string, const Extraction*> m_extractions;
};

}

vector<bsoncxx::document::value> Project::exclude(vector<bsoncxx::document::value>& in)
{
    ExcludingBuilder builder(m_extractions);

    vector<bsoncxx::document::value> out;

    for (auto& doc : in)
    {
        out.emplace_back(builder.build(doc));
    }

    return out;
}

/**
 * Sample
 */
Sample::Sample(bsoncxx::document::element element, Stage* pPrevious)
    : DualStage(pPrevious)
{
    if (element.type() != bsoncxx::type::k_document)
    {
        throw SoftError("the $sample stage specification must be an object", error::LOCATION28745);
    }

    bsoncxx::document::view sample = element.get_document();

    auto it = sample.begin();

    if (it == sample.end())
    {
        throw SoftError("$sample stage must specify a size", error::LOCATION28749);
    }

    int64_t nSamples;
    while (it != sample.end())
    {
        bsoncxx::document::element e = *it;

        if (e.key() != "size")
        {
            stringstream ss;
            ss << "unrecognized option to $sample: " << e.key();

            throw SoftError(ss.str(), error::LOCATION28748);
        }

        if (!nobson::get_number(e, &nSamples))
        {
            throw SoftError("size argument to $sample must be a number", error::LOCATION28746);
        }

        ++it;
    }

    if (nSamples < 0)
    {
        throw SoftError("size argument to $sample must not be negative", error::LOCATION28747);
    }

    m_nSamples = nSamples;
}

bool Sample::update(Query& query) const
{
    bool rv = false;

    mxb_assert(is_sql() && query.is_malleable());

    if (query.order_by().empty() && query.limit() == Query::MAX_LIMIT)
    {
        query.set_order_by("RAND()");
        query.set_limit(m_nSamples);
        rv = true;
    }

    return rv;
}

std::vector<bsoncxx::document::value> Sample::process(std::vector<bsoncxx::document::value>& in)
{
    std::vector<bsoncxx::document::value> out;

    if (in.size() <= m_nSamples)
    {
        out = std::move(in);
    }
    else
    {
        std::default_random_engine gen;

        std::sample(in.begin(), in.end(), std::back_inserter(out), m_nSamples, gen);
    }

    return out;
}

/**
 * Skip
 */
Skip::Skip(bsoncxx::document::element element, Stage* pPrevious)
    : DualStage(pPrevious)
{
    if (!nobson::get_number(element.get_value(), &m_nSkip))
    {
        stringstream ss;
        ss << "invalid argument to $skip stage: Expected a number in: $skip: "
           << nobson::to_bson_expression(element.get_value());

        throw SoftError(ss.str(), error::LOCATION5107200);
    }

    if (m_nSkip < 0)
    {
        stringstream ss;
        ss << "invalid argument to $skip stage: Expected a non-negative number in: $skip: " << m_nSkip;

        throw SoftError(ss.str(), error::LOCATION5107200);
    }
}

bool Skip::update(Query& query) const
{
    mxb_assert(is_sql() && query.is_malleable());

    auto limit = query.limit();
    auto skip = query.skip();

    if (limit != Query::MAX_LIMIT)
    {
        if (limit > m_nSkip)
        {
            limit -= m_nSkip;
        }
        else
        {
            limit = 0;
        }

        query.set_limit(limit);
    }

    query.set_skip(skip + m_nSkip);

    return true;
}

std::vector<bsoncxx::document::value> Skip::process(std::vector<bsoncxx::document::value>& in)
{
    if (m_nSkip > (int)in.size())
    {
        in.clear();
    }
    else
    {
        auto b = in.begin();
        in.erase(b, b + m_nSkip);
    }

    return std::move(in);
}

/**
 * Sort
 */
Sort::Sort(bsoncxx::document::element element, Stage* pPrevious)
    : DualStage(pPrevious)
{
    if (element.type() != bsoncxx::type::k_document)
    {
        throw SoftError("the $sort key specification must be an object", error::LOCATION15973);
    }

    m_sort = element.get_document();

    if (m_sort.empty())
    {
        throw SoftError("$sort stage must have at least one sort key", error::LOCATION15976);
    }

    m_order_by = order_by_value_from_sort(m_sort);
}

bool Sort::update(Query& query) const
{
    bool rv = true;
    mxb_assert(is_sql() && query.is_malleable());

    auto& order_by = query.order_by();

    if (order_by.empty())
    {
        query.set_order_by(m_order_by);
    }
    else
    {
        rv = false;
    }

    return rv;
}

namespace
{

class Sorter
{
public:
    class FieldSorter
    {
    public:
        FieldSorter(string_view field, int order)
            : m_order(order)
        {
            size_t pos;

            do
            {
                pos = field.find('.');

                if (pos != field.npos)
                {
                    m_fields.emplace_back(string(field.substr(0, pos)));
                    field = field.substr(pos + 1);
                }
                else
                {
                    m_fields.emplace_back(string(field));
                }
            }
            while (pos != field.npos);
        }

        bool eq(bsoncxx::document::view lhs, bsoncxx::document::view rhs) const
        {
            bsoncxx::types::bson_value::view l = get_from(lhs);
            bsoncxx::types::bson_value::view r = get_from(rhs);

            return l == r;
        }

        bool lt(bsoncxx::document::view lhs, bsoncxx::document::view rhs) const
        {
            // TODO: Cashe the values digged out in eq().
            bsoncxx::types::bson_value::view l = get_from(lhs);
            bsoncxx::types::bson_value::view r = get_from(rhs);

            return m_order == 1 ? l < r : r < l;
        }

    private:
        bsoncxx::types::value get_from(bsoncxx::document::view doc) const
        {
            bsoncxx::document::element element;

            for (auto it = m_fields.begin(); it != m_fields.end(); ++it)
            {
                auto& field = *it;

                element = doc[field];

                if (!element || element.type() != bsoncxx::type::k_document)
                {
                    break;
                }

                doc = element.get_document();
            }

            bsoncxx::types::value rv;

            if (element)
            {
                rv = element.get_value();
            }

            return rv;
        }

        vector<string> m_fields;
        int            m_order;
    };

    Sorter(bsoncxx::document::view sort)
    {
        // 'sort' was validated in the constructor of Sort.
        for (auto element : sort)
        {
            mxb_assert(element.key().size() != 0);

            int64_t value;
            if (!nobson::get_number(element, &value))
            {
                mxb_assert(!true);
            }

            mxb_assert(value == 1 || value == -1);

            m_field_sorters.emplace_back(std::make_shared<FieldSorter>(element.key(), value));
        }
    }

    bool operator () (bsoncxx::document::view lhs, bsoncxx::document::view rhs) const
    {
        auto it = m_field_sorters.begin();
        auto end = m_field_sorters.end() - 1;
        for (; it != end; ++it)
        {
            auto& sField_sorter = *it;

            if (!sField_sorter->eq(lhs, rhs))
            {
                return sField_sorter->lt(lhs, rhs);
            }
        }

        mxb_assert(it == end);

        return (*it)->lt(lhs, rhs);
    }

private:
    using SFieldSorter = shared_ptr<FieldSorter>;

    vector<SFieldSorter> m_field_sorters;
};

}

std::vector<bsoncxx::document::value> Sort::process(std::vector<bsoncxx::document::value>& in)
{
    std::sort(in.begin(), in.end(), Sorter(m_sort));

    return std::move(in);
}

/**
 * Unset
 */
Unset::Unset(bsoncxx::document::element element, Stage* pPrevious)
    : DualStage(pPrevious)
    , m_extractions(Extractions::from_unset(element))
{
}

bool Unset::update(Query& query) const
{
    bool rv = false;

    if (query.column() == "doc")
    {
        auto p = m_extractions.generate_column();

        if (p.second == Extractions::Projection::COMPLETE)
        {
            query.set_column(p.first);
            query.freeze();
            rv = true;
        }
    }

    return rv;
}

std::vector<bsoncxx::document::value> Unset::process(std::vector<bsoncxx::document::value>& in)
{
    Project project(std::move(m_extractions));

    return project.process(in);
}

/**
 * Unwind
 */
Unwind::Unwind(bsoncxx::document::element element, Stage* pPrevious)
    : PipelineStage(pPrevious)
{
    string_view field_path;
    auto type = element.type();

    switch (type)
    {
    case bsoncxx::type::k_string:
        field_path = element.get_string();
        break;

    case bsoncxx::type::k_document:
        {
            bsoncxx::document::view doc = element.get_document();
            for (auto e : doc)
            {
                if (e.key() == "path")
                {
                    if (e.type() != bsoncxx::type::k_string)
                    {
                        stringstream ss;
                        ss << "expected a string as the path for the $unwind stage, got "
                           << bsoncxx::to_string(type);

                        throw SoftError(ss.str(), error::LOCATION28808);
                    }

                    field_path = e.get_string();
                }
                else if (e.key() == "preserveNullAndEmptyArrays")
                {
                    if (e.type() != bsoncxx::type::k_bool)
                    {
                        stringstream ss;
                        ss << "expected a boolean for the preserveNullAndEmptyArrays option to "
                           << "$unwind stage, got " << bsoncxx::to_string(type);

                        throw SoftError(ss.str(), error::LOCATION28811);
                    }

                    m_preserve_null_and_empty_arrays = e.get_bool();
                }
                else if (e.key() == "includeArrayIndex")
                {
                    if (e.type() != bsoncxx::type::k_string
                        || (static_cast<string_view>(e.get_string()).empty()))
                    {
                        stringstream ss;
                        ss << "expected a non-empty string for the includeArrayIndex option to "
                           << "$unwind stage, got " << bsoncxx::to_string(type);
                    }

                    m_include_array_index = e.get_string();

                    if (m_include_array_index.front() == '$')
                    {
                        stringstream ss;
                        ss << "includeArrayIndex option to $unwind stage should not be prefixed with a '$: "
                           << m_include_array_index;

                        throw SoftError(ss.str(), error::LOCATION28822);
                    }

                    if (m_include_array_index.find('.') != m_include_array_index.npos)
                    {
                        stringstream ss;
                        ss << "Currently, includeArrayIndex cannot be a path: " << m_include_array_index;

                        throw SoftError(ss.str(), error::INTERNAL_ERROR);
                    }
                }
                else
                {
                    stringstream ss;
                    ss << "unrecognized option to $unwind stage: " << e.key();

                    throw SoftError(ss.str(), error::LOCATION28811);
                }
            }
        }
        break;

    default:
        {
            stringstream ss;
            ss << "expected a string as specification for $unwind stage, got " << bsoncxx::to_string(type);

            throw SoftError(ss.str(), error::LOCATION15981);
        }
    }

    if (field_path.empty())
    {
        throw SoftError("no path specified for $unwind stage", error::LOCATION28812);
    }

    if (field_path.front() != '$')
    {
        stringstream ss;
        ss << "path option to $unwind stage should be prefixed with a '$': " << field_path;

        throw SoftError(ss.str(), error::LOCATION28818);
    }

    m_field_path.reset(field_path);
}

std::vector<bsoncxx::document::value> Unwind::process(std::vector<bsoncxx::document::value>& in)
{
    vector<bsoncxx::document::value> out;

    for (bsoncxx::document::value& doc : in)
    {
        bsoncxx::document::element element = m_field_path.get(doc);

        if (element)
        {
            auto type = element.type();
            if (type == bsoncxx::type::k_array)
            {
                bsoncxx::array::view array = element.get_array();

                int64_t index = 0;
                for (const bsoncxx::array::element& ae : array)
                {
                    out.emplace_back(build(doc, ae, bsoncxx::types::bson_value::value(index++)));
                }

                if (index == 0 && m_preserve_null_and_empty_arrays)
                {
                    out.emplace_back(build(doc,
                                           bsoncxx::array::element {},
                                           bsoncxx::types::bson_value::value(nullptr)));
                }
            }
            else if (type != bsoncxx::type::k_null || m_preserve_null_and_empty_arrays)
            {
                out.emplace_back(std::move(doc));
            }
        }
        else if (m_preserve_null_and_empty_arrays)
        {
            out.emplace_back(std::move(doc));
        }
    }

    return out;
}

bsoncxx::document::value Unwind::build(const bsoncxx::document::view& doc,
                                       const bsoncxx::array::element& ae,
                                       const bsoncxx::types::bson_value::value& index)
{
    return build(m_field_path, doc, ae, index);
}

bsoncxx::document::value Unwind::build(const FieldPath& field_path,
                                       const bsoncxx::document::view& doc,
                                       const bsoncxx::array::element& ae,
                                       const bsoncxx::types::bson_value::value& index)
{
    DocumentBuilder b;

    const FieldPath* pTail = field_path.tail();

    for (const bsoncxx::document::element& de : doc)
    {
        if (de.key() == field_path.head())
        {
            if (pTail)
            {
                mxb_assert(de.type() == bsoncxx::type::k_document);

                b.append(kvp(de.key(), build(*pTail, de.get_document(), ae, index)));
            }
            else
            {
                if (ae)
                {
                    b.append(kvp(de.key(), ae.get_value()));
                }

                if (!m_include_array_index.empty())
                {
                    b.append(kvp(m_include_array_index, index));
                }
            }
        }
        else
        {
            b.append(kvp(de.key(), de.get_value()));
        }
    }

    return b.extract();
}

}
}

