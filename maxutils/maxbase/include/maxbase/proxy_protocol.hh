/*
 * Copyright (c) 2023 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2026-11-16
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */
#pragma once

#include <maxscale/ccdefs.hh>
#include <string>

struct sockaddr_storage;

namespace maxbase
{
namespace proxy_protocol
{
struct HeaderV1Res
{
    char        header[108];
    int         len {0};
    std::string errmsg;
};
HeaderV1Res
generate_proxy_header_v1(const sockaddr_storage* client_addr, const sockaddr_storage* server_addr);
}
}