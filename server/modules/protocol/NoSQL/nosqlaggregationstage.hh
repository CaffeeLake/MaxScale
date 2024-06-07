/*
 * Copyright (c) 2024 MariaDB plc
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2028-05-14
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */
#pragma once

#include "nosqlprotocol.hh"
#include <map>
#include <vector>
#include <bsoncxx/document/view.hpp>
#include <maxbase/json.hh>
#include "nosqlaggregationoperator.hh"

namespace nosql
{

namespace aggregation
{

/**
 * Stage
 */
class Stage
{
public:
    using OperatorCreator = std::function<std::unique_ptr<Operator>(bsoncxx::types::value)>;
    using Operators = std::map<std::string_view, OperatorCreator, std::less<>>;

    Stage(const Stage&) = delete;
    Stage& operator=(const Stage&) = delete;

    virtual const char* name() const = 0;

    enum class Kind
    {
        PURE, // A pure pipeline stage and can be anywhere.
        SQL,  // Must be able to provide the SQL and must thus be the first.
        DUAL  // Can be part of the pipeline or can modify the SQL.
    };

    Kind kind() const
    {
        return m_kind;
    }

    Stage* previous() const
    {
        return m_pPrevious;
    }

    virtual std::string trailing_sql() const;

    virtual ~Stage();

    static std::unique_ptr<Stage> get(bsoncxx::document::element element,
                                      std::string_view database,
                                      std::string_view table,
                                      Stage* pPrevious);

    /**
     * Perform the stage on the provided documents.
     *
     * @param in  An array of documents.
     *
     * @return An array of documents.
     */
    virtual std::vector<bsoncxx::document::value> process(std::vector<bsoncxx::document::value>& in) = 0;

    /**
     * Postprocess a resultset. This will only be called the stages that produces
     * the SQL that then produced the resultset.
     *
     * @param mariadb_response
     *
     * @return An array of corresponding documents.
     */
    virtual std::vector<bsoncxx::document::value> post_process(GWBUF&& mariadb_response);

protected:
    Stage(Stage* pPrevious, Kind kind = Kind::PURE)
        : m_pPrevious(pPrevious)
        , m_kind(kind)
    {
    }

private:
    Stage* const m_pPrevious;
    const Kind   m_kind;
};

template<class Derived>
class ConcreteStage : public Stage
{
public:
    using Stage::Stage;

    const char* name() const override
    {
        return Derived::NAME;
    }

    static std::unique_ptr<Stage> create(bsoncxx::document::element element,
                                         std::string_view database,
                                         std::string_view table,
                                         Stage* pPrevious)
    {
        mxb_assert(element.key() == Derived::NAME);
        return std::make_unique<Derived>(element, database, table, pPrevious);
    }
};

/**
 * AddFields
 */
class AddFields : public ConcreteStage<AddFields>
{
public:
    static constexpr const char* const NAME = "$addFields";

    AddFields(bsoncxx::document::element element,
              std::string_view database,
              std::string_view table,
              Stage* pPrevious);

    std::vector<bsoncxx::document::value> process(std::vector<bsoncxx::document::value>& in) override;

private:
    struct NamedOperator
    {
        std::string_view          name;
        std::unique_ptr<Operator> sOperator;
    };

    std::vector<NamedOperator> m_operators;
};

/**
 * CollStats
 */
class CollStats : public ConcreteStage<CollStats>
{
public:
    static constexpr const char* const NAME = "$collStats";

    CollStats(bsoncxx::document::element element,
              std::string_view database,
              std::string_view table,
              Stage* pPrevious);

    std::string trailing_sql() const override;

    std::vector<bsoncxx::document::value> process(std::vector<bsoncxx::document::value>& in) override;

    std::vector<bsoncxx::document::value> post_process(GWBUF&& mariadb_response) override;

private:
    std::string m_sql;
};

/**
 * Count
 */
class Count : public ConcreteStage<Count>
{
public:
    static constexpr const char* const NAME = "$count";

    Count(bsoncxx::document::element element,
          std::string_view database,
          std::string_view table,
          Stage* pPrevious);

    std::vector<bsoncxx::document::value> process(std::vector<bsoncxx::document::value>& in) override;

private:
    std::string_view m_field;
};

/**
 * Group
 */
class Group : public ConcreteStage<Group>
{
public:
    static constexpr const char* const NAME = "$group";

    Group(bsoncxx::document::element element,
          std::string_view database,
          std::string_view table,
          Stage* pPrevious);

    std::vector<bsoncxx::document::value> process(std::vector<bsoncxx::document::value>& in) override;

private:
    void add_operator(bsoncxx::document::element operator_def);
    void add_operator(std::string_view name, bsoncxx::document::view def);

    struct NamedOperator
    {
        std::string_view          name;
        std::unique_ptr<Operator> sOperator;
    };

    std::vector<NamedOperator> m_operators;

    static Operators           s_available_operators;
};

/**
 * Limit
 */
class Limit : public ConcreteStage<Limit>
{
public:
    static constexpr const char* const NAME = "$limit";

    Limit(bsoncxx::document::element element,
          std::string_view database,
          std::string_view table,
          Stage* pPrevious);

    std::string trailing_sql() const override;

    std::vector<bsoncxx::document::value> process(std::vector<bsoncxx::document::value>& in) override;

private:
    int64_t m_nLimit;
};

/**
 * ListSearchIndexes
 */
class ListSearchIndexes : public ConcreteStage<ListSearchIndexes>
{
public:
    static constexpr const char* const NAME = "$listSearchIndexes";

    ListSearchIndexes(bsoncxx::document::element element,
                      std::string_view database,
                      std::string_view table,
                      Stage* pPrevious);

    std::vector<bsoncxx::document::value> process(std::vector<bsoncxx::document::value>& in) override;
};

/**
 * Match
 */
class Match : public ConcreteStage<Match>
{
public:
    static constexpr const char* const NAME = "$match";

    Match(bsoncxx::document::element element,
          std::string_view database,
          std::string_view table,
          Stage* pPrevious);

    std::string trailing_sql() const override;

    std::vector<bsoncxx::document::value> process(std::vector<bsoncxx::document::value>& in) override;

    std::vector<bsoncxx::document::value> post_process(GWBUF&& mariadb_response) override;

private:
    std::string             m_database;
    std::string             m_table;
    bsoncxx::document::view m_match;
    std::string             m_sql;
};

}
}

