/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 * Copyright (c) 2023 MariaDB plc, Finnish Branch
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2028-01-30
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

/**
 * Firewall filter multiple matching users
 *
 * Test it multiple matching user rows are handled in OR fashion.
 */


#include <iostream>
#include <unistd.h>
#include <maxtest/testconnections.hh>

int main(int argc, char** argv)
{
    TestConnections::skip_maxscale_start(true);
    char rules_dir[4096];

    TestConnections test(argc, argv);

    test.tprintf("Creating rules\n");
    test.maxscale->stop();

    sprintf(rules_dir, "%s/fw/", mxt::SOURCE_DIR);
    test.maxscale->copy_fw_rules("rules_mxs1583", rules_dir);

    test.reset_timeout();
    test.maxscale->start_maxscale();

    test.reset_timeout();
    test.maxscale->connect_maxscale();

    test.try_query(test.maxscale->conn_rwsplit, "drop table if exists t");
    test.try_query(test.maxscale->conn_rwsplit, "create table t (a text, b text)");

    test.tprintf("Trying query that matches one 'user' row, expecting failure\n");
    test.reset_timeout();
    test.add_result(!execute_query(test.maxscale->conn_rwsplit, "select concat(a) from t"),
                    "Query that matches one 'user' row should fail.\n");

    test.tprintf("Trying query that matches other 'user' row, expecting failure\n");
    test.reset_timeout();
    test.add_result(!execute_query(test.maxscale->conn_rwsplit, "select concat(b) from t"),
                    "Query that matches other 'user' row should fail.\n");

    test.tprintf("Trying query that matches both 'user' rows, expecting failure\n");
    test.reset_timeout();
    test.add_result(!execute_query_silent(test.maxscale->conn_rwsplit,
                                          "select concat(a), concat(b) from t"),
                    "Query that matches both 'user' rows should fail.\n");

    test.tprintf("Trying non-matching query to blacklisted RWSplit, expecting success\n");
    test.reset_timeout();
    test.add_result(execute_query_silent(test.maxscale->conn_rwsplit, "show status"),
                    "Non-matching query to blacklist service should succeed.\n");

    test.maxscale->expect_running_status(true);
    test.maxscale->stop();
    test.maxscale->expect_running_status(false);

    return test.global_result;
}
