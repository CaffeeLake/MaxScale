/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 * Copyright (c) 2023 MariaDB plc, Finnish Branch
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2027-11-30
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

/**
 * @file bug143.cpp bug143 regression case (MaxScale ignores host in user authentication)
 *
 * - create  user@'non_existing_host1', user@'%', user@'non_existing_host2' identified by different passwords.
 * - try to connect using RWSplit. First and third are expected to fail, second should succeed.
 */

#include <maxtest/testconnections.hh>

int main(int argc, char* argv[])
{
    TestConnections* Test = new TestConnections(argc, argv);

    Test->tprintf("Creating user 'user' with 3 different passwords for different hosts\n");
    Test->maxscale->connect_maxscale();
    execute_query(Test->maxscale->conn_rwsplit,
                  "CREATE USER 'user'@'non_existing_host1' IDENTIFIED BY 'pass1'");
    execute_query(Test->maxscale->conn_rwsplit, "CREATE USER 'user'@'%%' IDENTIFIED BY 'pass2'");
    execute_query(Test->maxscale->conn_rwsplit,
                  "CREATE USER 'user'@'non_existing_host2' IDENTIFIED BY 'pass3'");
    execute_query(Test->maxscale->conn_rwsplit,
                  "GRANT ALL PRIVILEGES ON *.* TO 'user'@'non_existing_host1'");
    execute_query(Test->maxscale->conn_rwsplit, "GRANT ALL PRIVILEGES ON *.* TO 'user'@'%%'");
    execute_query(Test->maxscale->conn_rwsplit,
                  "GRANT ALL PRIVILEGES ON *.* TO 'user'@'non_existing_host2'");

    Test->tprintf("Synchronizing slaves");
    Test->repl->sync_slaves();

    const char* mxs_ip = Test->maxscale->ip4();

    Test->tprintf("Trying first hostname, expecting failure");
    MYSQL* conn = open_conn(Test->maxscale->rwsplit_port, mxs_ip, "user", "pass1", Test->maxscale_ssl);
    if (mysql_errno(conn) == 0)
    {
        Test->add_result(1, "MaxScale ignores host in authentication\n");
    }
    if (conn != NULL)
    {
        mysql_close(conn);
    }

    Test->tprintf("Trying second hostname, expecting success");
    conn = open_conn(Test->maxscale->rwsplit_port, mxs_ip, "user", "pass2", Test->maxscale_ssl);
    Test->add_result(mysql_errno(conn), "MaxScale can't connect: %s\n", mysql_error(conn));
    if (conn != NULL)
    {
        mysql_close(conn);
    }

    Test->tprintf("Trying third hostname, expecting failure");
    conn = open_conn(Test->maxscale->rwsplit_port, mxs_ip, "user", "pass3", Test->maxscale_ssl);
    if (mysql_errno(conn) == 0)
    {
        Test->add_result(1, "MaxScale ignores host in authentication\n");
    }
    if (conn != NULL)
    {
        mysql_close(conn);
    }

    execute_query(Test->maxscale->conn_rwsplit, "DROP USER 'user'@'non_existing_host1'");
    execute_query(Test->maxscale->conn_rwsplit, "DROP USER 'user'@'%%'");
    execute_query(Test->maxscale->conn_rwsplit, "DROP USER 'user'@'non_existing_host2'");
    Test->maxscale->close_maxscale_connections();

    int rval = Test->global_result;
    delete Test;
    return rval;
}
