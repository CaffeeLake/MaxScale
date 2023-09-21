/*
 * Copyright (c) 2018 MariaDB Corporation Ab
 * Copyright (c) 2023 MariaDB plc, Finnish Branch
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2027-09-19
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

#include "xpandnode.hh"
#include "xpand.hh"

xpand::Result XpandNode::ping_or_connect(const char* zName,
                                         const mxs::MonitorServer::ConnectionSettings& settings,
                                         xpand::Softfailed softfailed)
{
    mxb_assert(m_pServer);
    auto rv = xpand::ping_or_connect_to_hub(zName, settings, softfailed, *m_pServer, &m_pCon);

    if (rv == xpand::Result::ERROR)
    {
        mysql_close(m_pCon);
        m_pCon = nullptr;
    }

    return rv;
}
