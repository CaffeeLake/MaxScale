/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 * Copyright (c) 2023 MariaDB plc, Finnish Branch
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2025-09-12
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

/**
 * MXS-1731: Empty version_string is not detected
 *
 * https://jira.mariadb.org/browse/MXS-1731
 */

#include <maxtest/testconnections.hh>
#include <fstream>
#include <iostream>

using std::cout;
using std::endl;

int main(int argc, char** argv)
{
    TestConnections test(argc, argv);
    const char* filename = "/tmp/RW-Split-Router.cnf";

    {
        std::ofstream cnf(filename);
        cnf << "[RW-Split-Router]" << endl
            << "type=service" << endl
            << "router=readwritesplit" << endl
            << "user=maxskysql" << endl
            << "password=skysql" << endl
            << "servers=server1" << endl
            << "version_string=" << endl;
    }

    test.maxscale->copy_to_node(filename, filename);
    test.maxscale->ssh_node_f(true,
                              "mkdir -p /var/lib/maxscale/maxscale.cnf.d/;"
                              "chown maxscale:maxscale /var/lib/maxscale/maxscale.cnf.d/;"
                              "cp %s /var/lib/maxscale/maxscale.cnf.d/RW-Split-Router.cnf",
                              filename);
    test.maxscale->ssh_node_f(true,
                              "chmod a+r /var/lib/maxscale/maxscale.cnf.d/RW-Split-Router.cnf");

    test.expect(test.maxscale->restart() != 0,
                "MaxScale should fail to start if a parameter has an empty value");
    test.maxscale->ssh_node_f(true, "rm /var/lib/maxscale/maxscale.cnf.d/RW-Split-Router.cnf");


    test.maxscale->restart();
    test.check_maxctrl("alter service RW-Split-Router enable_root_user true");
    test.check_maxctrl("alter service RW-Split-Router enable_root_user false");

    test.maxscale->restart();
    test.check_maxscale_alive();

    int rc = test.maxscale->ssh_node_f(true,
                                       "grep 'version_string' /var/lib/maxscale/maxscale.cnf.d/RW-Split-Router.cnf");
    test.expect(rc != 0, "Generated configuration should not have version_string defined.");

    return test.global_result;
}
