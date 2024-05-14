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
 * Dbfwfilter prepared statement test
 *
 * Checks that both text protocol and binary protocol prepared statements are
 * properly handled.
 */

#include <maxtest/testconnections.hh>

const char* rules = "rule test1 deny columns c on_queries select\n"
                    "users %@% match any rules test1\n";

int main(int argc, char** argv)
{
    FILE* file = fopen("rules.txt", "w");
    fwrite(rules, 1, strlen(rules), file);
    fclose(file);

    TestConnections::skip_maxscale_start(true);
    TestConnections test(argc, argv);

    test.maxscale->copy_fw_rules("rules.txt", ".");

    test.add_result(test.maxscale->restart_maxscale(), "Restarting MaxScale failed");

    test.maxscale->connect_maxscale();
    execute_query_silent(test.maxscale->conn_rwsplit, "DROP TABLE test.t1");

    test.try_query(test.maxscale->conn_rwsplit, "CREATE TABLE test.t1(a INT, b INT, c INT)");
    test.try_query(test.maxscale->conn_rwsplit, "INSERT INTO test.t1 VALUES (1, 1, 1)");

    test.add_result(execute_query(test.maxscale->conn_rwsplit,
                                  "PREPARE my_ps FROM 'SELECT a, b FROM test.t1'"),
                    "Text protocol preparation should succeed");
    test.add_result(execute_query(test.maxscale->conn_rwsplit, "EXECUTE my_ps"),
                    "Text protocol execution should succeed");

    test.add_result(execute_query(test.maxscale->conn_rwsplit,
                                  "PREPARE my_ps2 FROM 'SELECT c FROM test.t1'") == 0,
                    "Text protocol preparation should fail");
    test.add_result(execute_query(test.maxscale->conn_rwsplit, "EXECUTE my_ps2") == 0,
                    "Text protocol execution should fail");

    MYSQL_STMT* stmt = mysql_stmt_init(test.maxscale->conn_rwsplit);
    const char* query = "SELECT a, b FROM test.t1";

    test.add_result(mysql_stmt_prepare(stmt, query, strlen(query)),
                    "Binary protocol preparation should succeed");
    test.add_result(mysql_stmt_execute(stmt), "Binary protocol execution should succeed");
    mysql_stmt_close(stmt);

    stmt = mysql_stmt_init(test.maxscale->conn_rwsplit);
    query = "SELECT c FROM test.t1";

    test.add_result(!mysql_stmt_prepare(stmt, query, strlen(query)),
                    "Binary protocol preparation should fail");
    mysql_stmt_close(stmt);

    test.repl->connect();
    test.try_query(test.repl->nodes[0], "DROP TABLE test.t1");

    return test.global_result;
}
