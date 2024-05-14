/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 * Copyright (c) 2023 MariaDB plc, Finnish Branch
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2024-06-03
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

/**
 * MXS-1111: Dbfwfilter COM_PING test
 *
 * Check that COM_PING is allowed with `action=allow`
 */

#include <maxtest/testconnections.hh>

#include <fstream>

auto rules =
    R"(
rule dont_mess_with_system_tables match regex 'mysql.*' on_queries drop|alter|create|use|load
users %@% match any rules dont_mess_with_system_tables
)";

int main(int argc, char** argv)
{
    std::ofstream file("rules.txt");
    file << rules;
    file.close();

    TestConnections::skip_maxscale_start(true);
    TestConnections test(argc, argv);
    test.maxscale->copy_fw_rules("rules.txt", ".");
    test.maxscale->start();

    auto conn = test.maxscale->rwsplit();
    test.expect(conn.connect(), "Connect failed: %s", conn.error());
    test.expect(conn.query("SELECT 1; SELECT 2; SELECT 3;"), "Query failed: %s", conn.error());
    test.expect(!conn.query("DROP DATABASE mysql"), "DROP succeeded");

    return test.global_result;
}
