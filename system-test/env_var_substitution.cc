/*
 * Copyright (c) 2024 MariaDB plc, Finnish Branch
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

#include <maxtest/testconnections.hh>

using std::string;

// This is the "upper limit" for the size of the static page that's served from admin.cc. The test will fail
// if the GUI at some point will generate a page that's shorter than this.
static constexpr size_t STATIC_PAGE_SIZE = 1500;

namespace
{
void test_main(TestConnections& test)
{
    auto& mxs = *test.maxscale;

    // Start MaxScale in a shell in a separate thread.
    std::thread maxscale_thread;

    auto run_maxscale = [&](bool secure_gui) {
        // Give environment variables in the command. Disable ASAN leak detection as it fails due to internal
        // error, causing MaxScale to return an error value.
        auto res = mxs.vm_node().run_cmd_output_sudof(
            "monitor_servers=server1,server2 monitor_user=maxskysql monitor_password=skysql secure_gui=%s "
            "ASAN_OPTIONS=detect_leaks=0 "
            "maxscale -d --user=maxscale --piddir=/tmp", secure_gui ? "true" : "false");
        if (res.rc == 0)
        {
            test.tprintf("MaxScale process exited with code 0.");
        }
        else
        {
            test.add_failure("MaxScale exited with error %i. Output: %s", res.rc, res.output.c_str());
        }
    };

    auto start_maxscale = [&](bool secure_gui) {
        test.tprintf("Starting MaxScale.");
        maxscale_thread = std::thread(run_maxscale, secure_gui);
        sleep(1);
        mxs.expect_running_status(true);
    };

    auto stop_maxscale = [&]() {
        test.tprintf("Shutting down MaxScale with kill.");
        mxs.vm_node().run_cmd_output_sudof("kill $(pidof maxscale)");
        sleep(1);
        mxs.expect_running_status(false);
        maxscale_thread.join();
    };

    start_maxscale(true);

    auto servers = mxs.get_servers();
    mxs.check_print_servers_status({mxt::ServerInfo::master_st, mxt::ServerInfo::slave_st,
                                    mxt::ServerInfo::DOWN, mxt::ServerInfo::DOWN});

    if (test.ok())
    {
        test.tprintf("Environment variable substitution works.");

        test.tprintf("Testing admin_secure_gui=true, fetching GUI should give a message.");

        const string curl_fetch_gui = "curl --silent -u admin:mariadb http://localhost:8989";
        const string insecure_gui = "The MaxScale GUI requires HTTPS to work, "
                                    "please enable it by configuring";

        auto res = mxs.vm_node().run_cmd_output_sudo(curl_fetch_gui);
        if (res.rc == 0)
        {
            test.expect(res.output.find(insecure_gui) != string::npos, "Did not find the expected message.");
            test.expect(res.output.size() < STATIC_PAGE_SIZE,
                        "Output is longer (%lu) than expected (%lu): %s",
                        res.output.size(), STATIC_PAGE_SIZE, res.output.c_str());
            if (test.ok())
            {
                test.tprintf("Received message explaining GUI is insecure.");
            }
        }
        else
        {
            test.add_failure("curl failed. Error %i, %s", res.rc, res.output.c_str());
        }

        if (test.ok())
        {
            stop_maxscale();

            test.tprintf("Testing admin_secure_gui=false, fetching GUI should work.");
            start_maxscale(false);

            res = mxs.vm_node().run_cmd_output_sudo(curl_fetch_gui);
            if (res.rc == 0)
            {
                test.expect(res.output.find(insecure_gui, 0) == string::npos,
                            "Found message when expecting GUI.");
                test.expect(res.output.size() > STATIC_PAGE_SIZE,
                            "Output is shorter (%lu) than expected (%lu): %s",
                            res.output.size(), STATIC_PAGE_SIZE, res.output.c_str());
                if (test.ok())
                {
                    test.tprintf("Received the GUI page.");
                }
            }
            else
            {
                test.add_failure("curl failed. Error %i, %s", res.rc, res.output.c_str());
            }
        }
    }

    stop_maxscale();
}
}

int main(int argc, char* argv[])
{
    TestConnections test;
    TestConnections::skip_maxscale_start(true);
    return test.run_test(argc, argv, test_main);
}
