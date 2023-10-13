/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 * Copyright (c) 2023 MariaDB plc, Finnish Branch
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2027-10-10
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

/**
 * MXS-1506: Delayed query retry
 *
 * https://jira.mariadb.org/browse/MXS-1506
 */
#include <maxtest/testconnections.hh>
#include <functional>
#include <thread>
#include <iostream>
#include <vector>

using namespace std;

struct TestCase
{
    string            description;
    function<void ()> pre;      // Called before master goes down
    function<void ()> block;    // Executed in a separate thread before `main` is called
    function<void ()> main;     // Called after master goes down
    function<void ()> check;    // Called after `main` and `block` are completed
};

int main(int argc, char** argv)
{
    TestConnections test(argc, argv);

    auto query = [&test](string q, int t = 0) {
            sleep(t);
            return execute_query_silent(test.maxscale->conn_rwsplit, q.c_str()) == 0;
        };

    auto compare = [&test](string q, string res) {
            auto rc = execute_query_check_one(test.maxscale->conn_rwsplit, q.c_str(), res.c_str()) == 0;
            test.expect(rc, "Query '%s' did not produce result of '%s'", q.c_str(), res.c_str());
        };

    auto check = [&test, &compare](string q, string res) {
            test.repl->sync_slaves();
            test.maxscale->connect_rwsplit();
            compare(q, res);
            test.maxscale->disconnect();
        };

    auto ok = [&test, &query](string q, int t = 0) {
            test.expect(query(q, t),
                        "Query '%s' should work: %s",
                        q.c_str(),
                        mysql_error(test.maxscale->conn_rwsplit));
        };

    auto err = [&test, &query](string q, int t = 0) {
            test.expect(!query(q, t), "Query should fail: %s", q.c_str());
        };

    auto block = [&test](int pre = 0, int node = 0) {
            sleep(pre);
            test.repl->block_node(node);
            sleep(10);
            test.repl->unblock_node(node);
        };

    auto noop = []() {
        };

    vector<TestCase> tests(
    {
        {
            "Normal insert",
            noop,
            block,
            bind(ok, "INSERT INTO test.t1 VALUES (1)", 5),
            bind(check, "SELECT COUNT(*) FROM test.t1 WHERE id = 1", "1")
        },
        {
            "Insert with user variables",
            bind(ok, "SET @a = 2"),
            block,
            bind(ok, "INSERT INTO test.t1 VALUES (@a)", 5),
            bind(check, "SELECT COUNT(*) FROM test.t1 WHERE id = 2", "1")
        },
        {
            "Normal transaction",
            bind(ok, "START TRANSACTION"),
            block,
            bind(err, "INSERT INTO test.t1 VALUES (3)", 5),
            bind(check, "SELECT COUNT(*) FROM test.t1 WHERE id = 3", "0")
        },
        {
            "Read-only transaction",
            bind(ok, "START TRANSACTION READ ONLY"),
            block,
            bind(err, "INSERT INTO test.t1 VALUES (4)", 5),
            bind(check, "SELECT COUNT(*) FROM test.t1 WHERE id = 4", "0")
        },
        {
            "Insert with autocommit=0",
            bind(ok, "SET autocommit=0"),
            block,
            bind(err, "INSERT INTO test.t1 VALUES (5)", 5),
            bind(check, "SELECT COUNT(*) FROM test.t1 WHERE id = 5", "0")
        },
        {
            "Interrupted insert (should cause duplicate statement execution)",
            noop,
            bind(block, 5),
            bind(ok, "INSERT INTO test.t1 VALUES ((SELECT SLEEP(10) + 6))", 0),
            bind(check, "SELECT COUNT(*) FROM test.t1 WHERE id = 6", "2")
        },
        {
            "Interrupted insert with user variable (should cause duplicate statement execution)",
            bind(ok, "SET @b = 7"),
            bind(block, 5),
            bind(ok, "INSERT INTO test.t1 VALUES ((SELECT SLEEP(10) + @b))", 0),
            bind(check, "SELECT COUNT(*) FROM test.t1 WHERE id = 7", "2")
        },
        {
            "Interrupted insert in transaction",
            bind(ok, "START TRANSACTION"),
            bind(block, 5),
            bind(err, "INSERT INTO test.t1 VALUES ((SELECT SLEEP(10) + 8))", 0),
            bind(check, "SELECT COUNT(*) FROM test.t1 WHERE id = 8", "0")
        },
        {
            "Interrupted insert in read-only transaction",
            bind(ok, "START TRANSACTION READ ONLY"),
            bind(block, 5),
            bind(err, "INSERT INTO test.t1 VALUES ((SELECT SLEEP(10) + 9))", 0),
            bind(check, "SELECT COUNT(*) FROM test.t1 WHERE id = 9", "0")
        },
        {
            "Interrupted select",
            noop,
            bind(block, 5, 1),
            bind(compare, "SELECT SLEEP(10) + 10", "10"),
            noop
        },
        {
            "Interrupted select with user variable",
            bind(ok, "SET @c = 11"),
            bind(block, 5, 1),
            bind(compare, "SELECT SLEEP(10) + @c", "11"),
            noop
        },
        {
            "Interrupted select in transaction",
            bind(ok, "START TRANSACTION"),
            bind(block, 5, 0),
            bind(err, "SELECT SLEEP(10)"),
            noop
        },
        {
            "Interrupted select in read-only transaction",
            bind(ok, "START TRANSACTION READ ONLY"),
            bind(block, 5, 1),
            bind(err, "SELECT SLEEP(10)"),
            noop
        },
        {
            "MXS-3383: Interrupted insert after session command with slow slaves (causes duplicate insert)",
            bind(ok, "SET @b = (SELECT SLEEP(@@server_id))"),
            bind(block, 5),
            bind(ok, "INSERT INTO test.t1 VALUES ((SELECT SLEEP(10) + @b))", 0),
            bind(check, "SELECT COUNT(*) FROM test.t1", "2")
        },
    });

    cout << "Create table for testing" << endl;
    test.maxscale->connect_rwsplit();
    ok("DROP TABLE IF EXISTS test.t1");
    ok("CREATE TABLE test.t1 (id INT)");
    test.maxscale->disconnect();

    for (auto a : tests)
    {
        test.log_printf("%s", a.description.c_str());
        test.maxscale->connect_rwsplit();
        a.pre();
        thread thr(a.block);
        a.main();
        test.maxscale->disconnect();
        thr.join();
        a.check();

        // Remove any inserted values
        test.maxscale->connect_rwsplit();
        query("TRUNCATE TABLE test.t1");
        test.maxscale->disconnect();
    }

    test.maxscale->connect_rwsplit();
    query("DROP TABLE test.t1");
    test.maxscale->disconnect();

    return test.global_result;
}
