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

#include "nosqlfieldpath.hh"

using namespace std;

namespace nosql
{

FieldPath::FieldPath()
    : m_pParent(nullptr)
{
}

FieldPath::FieldPath(string_view path, Mode mode)
    : m_pParent(nullptr)
{
    reset(path, mode);
}

FieldPath::FieldPath(string_view path, FieldPath* pParent)
    : m_pParent(pParent)
{
    construct(path);
}

void FieldPath::reset(string_view path, Mode mode)
{
    if (mode == Mode::WITH_DOLLAR)
    {
        mxb_assert(!path.empty());
        mxb_assert(path.front() == '$');
        path = path.substr(1);
    }

    mxb_assert(!path.empty());
    mxb_assert(path.front() != '$');

    construct(path);
}

void FieldPath::construct(string_view path)
{
    auto pos = path.find('.');

    m_head = path.substr(0, pos);

    if (pos != path.npos)
    {
        m_sTail.reset(new FieldPath(path.substr(pos + 1), this));
    }
}

bsoncxx::document::element FieldPath::get(const bsoncxx::document::view& doc) const
{
    bsoncxx::document::element rv;

    auto element = doc[m_head];

    if (m_sTail)
    {
        if (element && element.type() == bsoncxx::type::k_document)
        {
            rv = m_sTail->get(element.get_document());
        }
    }
    else
    {
        rv = element;
    }

    return rv;
}

}
