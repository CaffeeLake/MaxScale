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
 * MXS-1507: Transaction replay tests
 *
 * https://jira.mariadb.org/browse/MXS-1507
 */
#include <maxtest/testconnections.hh>
#include <functional>
#include <iostream>
#include <vector>

using namespace std;

const std::string BIG_VALUE(1500, 'a');

int main(int argc, char** argv)
{
    TestConnections test(argc, argv);

    auto query = [&](string q) {
            return execute_query_silent(test.maxscale->conn_rwsplit, q.c_str()) == 0;
        };

    auto ok = [&](string q) {
            test.expect(query(q),
                        "Query '%s' should work: %s",
                        q.c_str(),
                        mysql_error(test.maxscale->conn_rwsplit));
        };

    auto err = [&](string q) {
            test.expect(!query(q), "Query should not work: %s", q.c_str());
        };

    auto check = [&](string q, string res) {
            Row row = get_row(test.maxscale->conn_rwsplit, q.c_str());
            test.expect(!row.empty() && row[0] == res,
                        "Query '%s' should return 1: %s (%s)",
                        q.c_str(),
                        row.empty() ? "<empty>" : row[0].c_str(),
                        mysql_error(test.maxscale->conn_rwsplit));
        };

    struct TrxTest
    {
        string                    description;
        vector<function<void ()>> pre;
        vector<function<void ()>> post;
        vector<function<void ()>> check;
    };

    std::vector<TrxTest> tests
    ({
        {
            "Basic transaction",
            {
                bind(ok, "BEGIN"),
                bind(ok, "SELECT 1"),
            },
            {
                bind(ok, "SELECT 2"),
                bind(ok, "COMMIT"),
            },
            {
            }
        },
        {
            "Large result",
            {
                bind(ok, "BEGIN"),
                bind(ok, "SELECT REPEAT('a', 100000)"),
            },
            {
                bind(ok, "SELECT REPEAT('a', 100000)"),
                bind(ok, "COMMIT"),
            },
            {
            }
        },
        {
            "Transaction with a write",
            {
                bind(ok, "BEGIN"),
                bind(ok, "INSERT INTO test.t1 VALUES (1)"),
            },
            {
                bind(ok, "INSERT INTO test.t1 VALUES (2)"),
                bind(ok, "COMMIT"),
            },
            {
                bind(check, "SELECT COUNT(*) FROM test.t1 WHERE id IN (1, 2)", "2"),
            }
        },
        {
            "Read-only transaction",
            {
                bind(ok, "START TRANSACTION READ ONLY"),
                bind(ok, "SELECT 1"),
            },
            {
                bind(ok, "SELECT 2"),
                bind(ok, "COMMIT"),
            },
            {
            }
        },
        {
            "Trx started, no queries",
            {
                bind(ok, "BEGIN"),
            },
            {
                bind(ok, "SELECT 1"),
                bind(ok, "COMMIT"),
            },
            {
            }
        },
        {
            "Trx waiting on commit",
            {
                bind(ok, "BEGIN"),
                bind(ok, "SELECT 1"),
            },
            {
                bind(ok, "COMMIT"),
            },
            {
            }
        },
        {
            "Trx with NOW()",
            {
                bind(ok, "BEGIN"),
                bind(ok, "SELECT NOW(), SLEEP(1)"),
            },
            {
                bind(err, "SELECT 1"),
            },
            {
            }
        },
        {
            "Commit trx with NOW()",
            {
                bind(ok, "BEGIN"),
                bind(ok, "SELECT NOW(), SLEEP(1)"),
            },
            {
                bind(err, "COMMIT"),
            },
            {
            }
        },
        {
            "NOW() used after replay",
            {
                bind(ok, "BEGIN"),
                bind(ok, "SELECT 1"),
            },
            {
                bind(ok, "SELECT NOW()"),
                bind(ok, "COMMIT"),
            },
            {
            }
        },
        {
            "Exceed transaction length limit",
            {
                bind(ok, "BEGIN"),
                bind(ok,
                     "SELECT '" + BIG_VALUE + "'"),
            },
            {
                bind(err, "SELECT 7"),
                bind(err, "COMMIT"),
            },
            {
            }
        },
        {
            "Normal trx after hitting limit",
            {
                bind(ok, "BEGIN"),
                bind(ok,
                     "SELECT '" + BIG_VALUE + "'"),
            },
            {
                bind(err, "SELECT 8"),
                bind(err, "COMMIT"),
            },
            {
                bind(ok, "BEGIN"),
                bind(ok, "SELECT 1"),
                bind(ok, "SELECT 2"),
                bind(ok, "COMMIT"),
            }
        },
        {
            "Session command inside transaction",
            {
                bind(ok, "BEGIN"),
                bind(ok, "SET @a = 1"),
            },
            {
                bind(check, "SELECT @a", "1"),
                bind(ok, "COMMIT"),
            },
            {
            }
        },
        {
            "Empty transaction",
            {
                bind(ok, "BEGIN"),
            },
            {
                bind(ok, "COMMIT"),
            },
            {
            }
        }
    });

    // Create a table for testing
    test.maxscale->connect_rwsplit();
    test.try_query(test.maxscale->conn_rwsplit, "CREATE OR REPLACE TABLE test.t1(id INT)");
    test.maxscale->disconnect();

    int i = 1;

    for (auto& a : tests)
    {
        test.reset_timeout();
        test.tprintf("%d: %s", i++, a.description.c_str());

        test.maxscale->connect_rwsplit();
        for (auto& f : a.pre)
        {
            f();
        }

        // Block and unblock the master
        test.repl->block_node(0);
        test.maxscale->wait_for_monitor(2);
        test.repl->unblock_node(0);
        test.maxscale->wait_for_monitor(2);

        for (auto& f : a.post)
        {
            f();
        }
        test.maxscale->disconnect();

        test.repl->connect();
        test.repl->sync_slaves();
        test.repl->disconnect();

        test.maxscale->connect_rwsplit();
        for (auto& f : a.check)
        {
            f();
        }
        test.maxscale->disconnect();

        // Clear the table at the end of the test
        test.maxscale->connect_rwsplit();
        test.try_query(test.maxscale->conn_rwsplit, "TRUNCATE TABLE test.t1");
        test.maxscale->disconnect();
    }

    test.maxscale->connect_rwsplit();
    test.try_query(test.maxscale->conn_rwsplit, "DROP TABLE test.t1");
    test.maxscale->disconnect();

    return test.global_result;
}
