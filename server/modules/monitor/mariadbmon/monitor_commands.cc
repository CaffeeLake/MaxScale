/*
 * Copyright (c) 2022 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2026-04-08
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

#include "mariadbmon.hh"
#include "maxbase/format.hh"
#include <maxbase/http.hh>
#include <maxscale/modulecmd.hh>

using maxscale::Monitor;
using maxscale::MonitorServer;
using std::string;

namespace
{
// Execution mode for commands.
enum class ExecMode
{
    SYNC,   /**< Function waits for completion or error */
    ASYNC   /**< Function only schedules the operation and will not wait */
};

const char err_passive_mode[] = "%s requested but not performed, as MaxScale is in passive mode.";
const char failover_cmd[] = "failover";
const char switchover_cmd[] = "switchover";
const char rejoin_cmd[] = "rejoin";
const char reset_repl_cmd[] = "reset-replication";
const char release_locks_cmd[] = "release-locks";
const char cs_add_node_cmd[] = "cs-add-node";
const char cs_remove_node_cmd[] = "cs-remove-node";

bool manual_switchover(ExecMode mode, const MODULECMD_ARG* args, json_t** error_out);
bool manual_failover(ExecMode mode, const MODULECMD_ARG* args, json_t** output);
bool manual_rejoin(ExecMode mode, const MODULECMD_ARG* args, json_t** output);
bool manual_reset_replication(ExecMode mode, const MODULECMD_ARG* args, json_t** output);
bool release_locks(ExecMode mode, const MODULECMD_ARG* args, json_t** output);

std::tuple<MariaDBMonitor*, string, string> read_args(const MODULECMD_ARG& args);
std::tuple<bool, std::chrono::seconds>      get_timeout(const string& timeout_str, json_t** output);

/**
 * Command handlers. These are called by the rest-api.
 */

// switchover
bool handle_manual_switchover(const MODULECMD_ARG* args, json_t** error_out)
{
    return manual_switchover(ExecMode::SYNC, args, error_out);
}

// async-switchover
bool handle_async_switchover(const MODULECMD_ARG* args, json_t** error_out)
{
    return manual_switchover(ExecMode::ASYNC, args, error_out);
}

// failover
bool handle_manual_failover(const MODULECMD_ARG* args, json_t** error_out)
{
    return manual_failover(ExecMode::SYNC, args, error_out);
}

// async-failover
bool handle_async_failover(const MODULECMD_ARG* args, json_t** error_out)
{
    return manual_failover(ExecMode::ASYNC, args, error_out);
}

// rejoin
bool handle_manual_rejoin(const MODULECMD_ARG* args, json_t** error_out)
{
    return manual_rejoin(ExecMode::SYNC, args, error_out);
}

// async-rejoin
bool handle_async_rejoin(const MODULECMD_ARG* args, json_t** error_out)
{
    return manual_rejoin(ExecMode::ASYNC, args, error_out);
}

// reset-replication
bool handle_manual_reset_replication(const MODULECMD_ARG* args, json_t** error_out)
{
    return manual_reset_replication(ExecMode::SYNC, args, error_out);
}

// async-reset-replication
bool handle_async_reset_replication(const MODULECMD_ARG* args, json_t** error_out)
{
    return manual_reset_replication(ExecMode::ASYNC, args, error_out);
}

// release-locks
bool handle_manual_release_locks(const MODULECMD_ARG* args, json_t** output)
{
    return release_locks(ExecMode::SYNC, args, output);
}

// async-release-locks
bool handle_async_release_locks(const MODULECMD_ARG* args, json_t** output)
{
    return release_locks(ExecMode::ASYNC, args, output);
}

bool handle_fetch_cmd_result(const MODULECMD_ARG* args, json_t** output)
{
    mxb_assert(args->argc == 1);
    mxb_assert(MODULECMD_GET_TYPE(&args->argv[0].type) == MODULECMD_ARG_MONITOR);
    Monitor* mon = args->argv[0].value.monitor;
    auto mariamon = static_cast<MariaDBMonitor*>(mon);
    mariamon->fetch_cmd_result(output);
    return true;    // result fetch always works, even if there is nothing to return
}

bool handle_async_cs_add_node(const MODULECMD_ARG* args, json_t** output)
{
    bool rval = false;
    auto [mon, host, timeout_str] = read_args(*args);
    auto [to_ok, timeout] = get_timeout(timeout_str, output);

    if (to_ok)
    {
        rval = mon->schedule_cs_add_node(host, timeout, output);
    }
    return rval;
}

bool handle_async_cs_remove_node(const MODULECMD_ARG* args, json_t** output)
{
    bool rval = false;
    auto [mon, host, timeout_str] = read_args(*args);
    auto [to_ok, timeout] = get_timeout(timeout_str, output);

    if (to_ok)
    {
        rval = mon->schedule_cs_remove_node(host, timeout, output);
    }
    return rval;
}

/**
 * Run manual switchover.
 *
 * @param mode    Execution mode
 * @param args    The provided arguments
 * @param output  Pointer where to place output object
 *
 * @return True, if the command was executed/scheduled, false otherwise.
 */
bool manual_switchover(ExecMode mode, const MODULECMD_ARG* args, json_t** error_out)
{
    mxb_assert((args->argc >= 1) && (args->argc <= 3));
    mxb_assert(MODULECMD_GET_TYPE(&args->argv[0].type) == MODULECMD_ARG_MONITOR);
    mxb_assert((args->argc < 2) || (MODULECMD_GET_TYPE(&args->argv[1].type) == MODULECMD_ARG_SERVER));
    mxb_assert((args->argc < 3) || (MODULECMD_GET_TYPE(&args->argv[2].type) == MODULECMD_ARG_SERVER));

    bool rval = false;
    if (mxs::Config::get().passive.get())
    {
        PRINT_MXS_JSON_ERROR(error_out, err_passive_mode, "Switchover");
    }
    else
    {
        Monitor* mon = args->argv[0].value.monitor;
        auto handle = static_cast<MariaDBMonitor*>(mon);
        SERVER* promotion_server = (args->argc >= 2) ? args->argv[1].value.server : nullptr;
        SERVER* demotion_server = (args->argc == 3) ? args->argv[2].value.server : nullptr;

        switch (mode)
        {
        case ExecMode::SYNC:
            rval = handle->run_manual_switchover(promotion_server, demotion_server, error_out);
            break;

        case ExecMode::ASYNC:
            rval = handle->schedule_async_switchover(promotion_server, demotion_server, error_out);
            break;
        }
    }
    return rval;
}

/**
 * Run manual failover.
 *
 * @paran mode Execution mode
 * @param args Arguments given by user
 * @param output Json error output
 * @return True on success
 */
bool manual_failover(ExecMode mode, const MODULECMD_ARG* args, json_t** output)
{
    mxb_assert(args->argc == 1);
    mxb_assert(MODULECMD_GET_TYPE(&args->argv[0].type) == MODULECMD_ARG_MONITOR);
    bool rv = false;

    if (mxs::Config::get().passive.get())
    {
        PRINT_MXS_JSON_ERROR(output, err_passive_mode, "Failover");
    }
    else
    {
        Monitor* mon = args->argv[0].value.monitor;
        auto handle = static_cast<MariaDBMonitor*>(mon);

        switch (mode)
        {
        case ExecMode::SYNC:
            rv = handle->run_manual_failover(output);
            break;

        case ExecMode::ASYNC:
            rv = handle->schedule_async_failover(output);
            break;
        }
    }
    return rv;
}

/**
 * Run manual rejoin.
 *
 * @paran mode Execution mode
 * @param args Arguments given by user
 * @param output Json error output
 * @return True on success
 */
bool manual_rejoin(ExecMode mode, const MODULECMD_ARG* args, json_t** output)
{
    mxb_assert(args->argc == 2);
    mxb_assert(MODULECMD_GET_TYPE(&args->argv[0].type) == MODULECMD_ARG_MONITOR);
    mxb_assert(MODULECMD_GET_TYPE(&args->argv[1].type) == MODULECMD_ARG_SERVER);

    bool rv = false;
    if (mxs::Config::get().passive.get())
    {
        PRINT_MXS_JSON_ERROR(output, "Rejoin requested but not performed, as MaxScale is in passive mode.");
    }
    else
    {
        Monitor* mon = args->argv[0].value.monitor;
        SERVER* server = args->argv[1].value.server;
        auto handle = static_cast<MariaDBMonitor*>(mon);

        switch (mode)
        {
        case ExecMode::SYNC:
            rv = handle->run_manual_rejoin(server, output);
            break;

        case ExecMode::ASYNC:
            rv = handle->schedule_async_rejoin(server, output);
            break;
        }
    }
    return rv;
}

/**
 * Run reset replication.
 *
 * @paran mode Execution mode
 * @param args Arguments given by user
 * @param output Json error output
 * @return True on success
 */
bool manual_reset_replication(ExecMode mode, const MODULECMD_ARG* args, json_t** output)
{
    mxb_assert(args->argc >= 1);
    mxb_assert(MODULECMD_GET_TYPE(&args->argv[0].type) == MODULECMD_ARG_MONITOR);
    mxb_assert(args->argc == 1 || MODULECMD_GET_TYPE(&args->argv[1].type) == MODULECMD_ARG_SERVER);

    bool rv = false;
    if (mxs::Config::get().passive.get())
    {
        PRINT_MXS_JSON_ERROR(output, "Replication reset requested but not performed, as MaxScale is in "
                                     "passive mode.");
    }
    else
    {
        Monitor* mon = args->argv[0].value.monitor;
        SERVER* server = args->argv[1].value.server;
        auto handle = static_cast<MariaDBMonitor*>(mon);

        switch (mode)
        {
        case ExecMode::SYNC:
            rv = handle->run_manual_reset_replication(server, output);
            break;

        case ExecMode::ASYNC:
            rv = handle->schedule_reset_replication(server, output);
            break;
        }
    }
    return rv;
}

/**
 * Run release locks.
 *
 * @paran mode Execution mode
 * @param args Arguments given by user
 * @param output Json error output
 * @return True on success
 */
bool release_locks(ExecMode mode, const MODULECMD_ARG* args, json_t** output)
{
    mxb_assert(args->argc == 1);
    mxb_assert(MODULECMD_GET_TYPE(&args->argv[0].type) == MODULECMD_ARG_MONITOR);

    bool rv = false;
    Monitor* mon = args->argv[0].value.monitor;
    auto mariamon = static_cast<MariaDBMonitor*>(mon);

    switch (mode)
    {
    case ExecMode::SYNC:
        rv = mariamon->run_release_locks(output);
        break;

    case ExecMode::ASYNC:
        rv = mariamon->schedule_release_locks(output);
        break;
    }
    return rv;
}

std::tuple<MariaDBMonitor*, string, string> read_args(const MODULECMD_ARG& args)
{
    mxb_assert(MODULECMD_GET_TYPE(&args.argv[0].type) == MODULECMD_ARG_MONITOR);
    mxb_assert(args.argc <= 1 || MODULECMD_GET_TYPE(&args.argv[1].type) == MODULECMD_ARG_STRING);
    mxb_assert(args.argc <= 2 || MODULECMD_GET_TYPE(&args.argv[2].type) == MODULECMD_ARG_STRING);

    MariaDBMonitor* mon = static_cast<MariaDBMonitor*>(args.argv[0].value.monitor);
    string text1 = args.argc >= 2 ? args.argv[1].value.string : "";
    string text2 = args.argc >= 3 ? args.argv[2].value.string : "";

    return {mon, text1, text2};
}

std::tuple<bool, std::chrono::seconds> get_timeout(const string& timeout_str, json_t** output)
{
    std::chrono::milliseconds duration;
    mxs::config::DurationUnit unit;
    std::chrono::seconds timeout = 0s;

    bool rv = get_suffixed_duration(timeout_str.c_str(), &duration, &unit);
    if (rv)
    {
        if (unit == mxs::config::DURATION_IN_MILLISECONDS)
        {
            MXB_WARNING("Duration specified in milliseconds, will be converted to seconds.");
        }

        timeout = std::chrono::duration_cast<std::chrono::seconds>(duration);
    }
    else
    {
        PRINT_MXS_JSON_ERROR(output,
                             "Timeout must be specified with a 's', 'm', or 'h' suffix. 'ms' is accepted "
                             "but the time will be converted to seconds.");
        rv = false;
    }

    return {rv, timeout};
}
}

void register_monitor_commands()
{
    static const char ARG_MONITOR_DESC[] = "Monitor name";
    static modulecmd_arg_type_t switchover_argv[] =
    {
        {MODULECMD_ARG_MONITOR | MODULECMD_ARG_NAME_MATCHES_DOMAIN, ARG_MONITOR_DESC           },
        {MODULECMD_ARG_SERVER | MODULECMD_ARG_OPTIONAL,             "New master (optional)"    },
        {MODULECMD_ARG_SERVER | MODULECMD_ARG_OPTIONAL,             "Current master (optional)"}
    };

    modulecmd_register_command(MXB_MODULE_NAME, switchover_cmd, MODULECMD_TYPE_ACTIVE,
                               handle_manual_switchover, MXS_ARRAY_NELEMS(switchover_argv), switchover_argv,
                               "Perform master switchover");

    modulecmd_register_command(MXB_MODULE_NAME, "async-switchover", MODULECMD_TYPE_ACTIVE,
                               handle_async_switchover, MXS_ARRAY_NELEMS(switchover_argv), switchover_argv,
                               "Schedule master switchover. Does not wait for completion");

    static modulecmd_arg_type_t failover_argv[] =
    {
        {MODULECMD_ARG_MONITOR | MODULECMD_ARG_NAME_MATCHES_DOMAIN, ARG_MONITOR_DESC},
    };

    modulecmd_register_command(MXB_MODULE_NAME, failover_cmd, MODULECMD_TYPE_ACTIVE,
                               handle_manual_failover, MXS_ARRAY_NELEMS(failover_argv), failover_argv,
                               "Perform master failover");

    modulecmd_register_command(MXB_MODULE_NAME, "async-failover", MODULECMD_TYPE_ACTIVE,
                               handle_async_failover, MXS_ARRAY_NELEMS(failover_argv), failover_argv,
                               "Schedule master failover. Does not wait for completion.");

    static modulecmd_arg_type_t rejoin_argv[] =
    {
        {MODULECMD_ARG_MONITOR | MODULECMD_ARG_NAME_MATCHES_DOMAIN, ARG_MONITOR_DESC},
        {MODULECMD_ARG_SERVER,                                      "Joining server"}
    };

    modulecmd_register_command(MXB_MODULE_NAME, rejoin_cmd, MODULECMD_TYPE_ACTIVE,
                               handle_manual_rejoin, MXS_ARRAY_NELEMS(rejoin_argv), rejoin_argv,
                               "Rejoin server to a cluster");

    modulecmd_register_command(MXB_MODULE_NAME, "async-rejoin", MODULECMD_TYPE_ACTIVE,
                               handle_async_rejoin, MXS_ARRAY_NELEMS(rejoin_argv), rejoin_argv,
                               "Rejoin server to a cluster. Does not wait for completion.");

    static modulecmd_arg_type_t reset_gtid_argv[] =
    {
        {MODULECMD_ARG_MONITOR | MODULECMD_ARG_NAME_MATCHES_DOMAIN, ARG_MONITOR_DESC          },
        {MODULECMD_ARG_SERVER | MODULECMD_ARG_OPTIONAL,             "Master server (optional)"}
    };

    modulecmd_register_command(MXB_MODULE_NAME, reset_repl_cmd, MODULECMD_TYPE_ACTIVE,
                               handle_manual_reset_replication,
                               MXS_ARRAY_NELEMS(reset_gtid_argv), reset_gtid_argv,
                               "Delete slave connections, delete binary logs and "
                               "set up replication (dangerous)");

    modulecmd_register_command(MXB_MODULE_NAME, "async-reset-replication", MODULECMD_TYPE_ACTIVE,
                               handle_async_reset_replication,
                               MXS_ARRAY_NELEMS(reset_gtid_argv), reset_gtid_argv,
                               "Delete slave connections, delete binary logs and "
                               "set up replication (dangerous). Does not wait for completion.");

    static modulecmd_arg_type_t release_locks_argv[] =
    {
        {MODULECMD_ARG_MONITOR | MODULECMD_ARG_NAME_MATCHES_DOMAIN, ARG_MONITOR_DESC},
    };

    modulecmd_register_command(MXB_MODULE_NAME, release_locks_cmd, MODULECMD_TYPE_ACTIVE,
                               handle_manual_release_locks,
                               MXS_ARRAY_NELEMS(release_locks_argv), release_locks_argv,
                               "Release any held server locks for 1 minute.");

    modulecmd_register_command(MXB_MODULE_NAME, "async-release-locks", MODULECMD_TYPE_ACTIVE,
                               handle_async_release_locks,
                               MXS_ARRAY_NELEMS(release_locks_argv), release_locks_argv,
                               "Release any held server locks for 1 minute. Does not wait for completion.");

    static modulecmd_arg_type_t fetch_cmd_result_argv[] =
    {
        {MODULECMD_ARG_MONITOR | MODULECMD_ARG_NAME_MATCHES_DOMAIN, ARG_MONITOR_DESC},
    };

    modulecmd_register_command(MXB_MODULE_NAME, "fetch-cmd-result", MODULECMD_TYPE_PASSIVE,
                               handle_fetch_cmd_result,
                               MXS_ARRAY_NELEMS(fetch_cmd_result_argv), fetch_cmd_result_argv,
                               "Fetch result of the last scheduled command.");

    const modulecmd_arg_type_t csmon_add_node_argv[] =
    {
        { MODULECMD_ARG_MONITOR | MODULECMD_ARG_NAME_MATCHES_DOMAIN, ARG_MONITOR_DESC },
        { MODULECMD_ARG_STRING, "Hostname/IP of node to add to ColumnStore cluster" },
        { MODULECMD_ARG_STRING, "Timeout." }
    };

    modulecmd_register_command(MXB_MODULE_NAME, "async-cs-add-node", MODULECMD_TYPE_ACTIVE,
                               handle_async_cs_add_node,
                               MXS_ARRAY_NELEMS(csmon_add_node_argv), csmon_add_node_argv,
                               "Add a node to a ColumnStore cluster. Does not wait for completion.");

    modulecmd_register_command(MXB_MODULE_NAME, "async-cs-remove-node", MODULECMD_TYPE_ACTIVE,
                               handle_async_cs_remove_node,
                               MXS_ARRAY_NELEMS(csmon_add_node_argv), csmon_add_node_argv,
                               "Remove a node from a ColumnStore cluster. Does not wait for completion.");
}

bool MariaDBMonitor::run_manual_switchover(SERVER* new_master, SERVER* current_master, json_t** error_out)
{
    auto func = [this, new_master, current_master](){
        return manual_switchover(new_master, current_master);
    };
    return execute_manual_command(func, switchover_cmd, error_out);
}

bool MariaDBMonitor::schedule_async_switchover(SERVER* new_master, SERVER* current_master, json_t** error_out)
{
    auto func = [this, new_master, current_master](){
        return manual_switchover(new_master, current_master);
    };
    return schedule_manual_command(func, switchover_cmd, error_out);
}

bool MariaDBMonitor::run_manual_failover(json_t** error_out)
{
    auto func = [this](){
        return manual_failover();
    };
    return execute_manual_command(func, failover_cmd, error_out);
}

bool MariaDBMonitor::schedule_async_failover(json_t** error_out)
{
    auto func = [this](){
        return manual_failover();
    };
    return schedule_manual_command(func, failover_cmd, error_out);
}

bool MariaDBMonitor::run_manual_rejoin(SERVER* rejoin_server, json_t** error_out)
{
    auto func = [this, rejoin_server](){
        return manual_rejoin(rejoin_server);
    };
    return execute_manual_command(func, rejoin_cmd, error_out);
}

bool MariaDBMonitor::schedule_async_rejoin(SERVER* rejoin_server, json_t** error_out)
{
    auto func = [this, rejoin_server](){
        return manual_rejoin(rejoin_server);
    };
    return schedule_manual_command(func, rejoin_cmd, error_out);
}

bool MariaDBMonitor::run_manual_reset_replication(SERVER* master_server, json_t** error_out)
{
    auto func = [this, master_server](){
        return manual_reset_replication(master_server);
    };
    return execute_manual_command(func, reset_repl_cmd, error_out);
}

bool MariaDBMonitor::schedule_reset_replication(SERVER* master_server, json_t** error_out)
{
    auto func = [this, master_server](){
        return manual_reset_replication(master_server);
    };
    return schedule_manual_command(func, reset_repl_cmd, error_out);
}

bool MariaDBMonitor::run_release_locks(json_t** error_out)
{
    auto func = [this](){
        return manual_release_locks();
    };
    return execute_manual_command(func, release_locks_cmd, error_out);
}

bool MariaDBMonitor::schedule_release_locks(json_t** error_out)
{
    auto func = [this](){
        return manual_release_locks();
    };
    return schedule_manual_command(func, release_locks_cmd, error_out);
}

MariaDBMonitor::ManualCommand::Result MariaDBMonitor::manual_release_locks()
{
    // Manual commands should only run in the main monitor thread.
    mxb_assert(mxb::Worker::get_current()->id() == this->id());
    mxb_assert(m_manual_cmd.exec_state == ManualCommand::ExecState::RUNNING);

    ManualCommand::Result rval;
    auto error_out = &rval.errors;

    bool success = false;
    if (server_locks_in_use())
    {
        std::atomic_int released_locks {0};
        auto release_lock_task = [&released_locks](MariaDBServer* server) {
            released_locks += server->release_all_locks();
        };
        execute_task_all_servers(release_lock_task);
        m_locks_info.have_lock_majority.store(false, std::memory_order_relaxed);

        // Set next locking attempt 1 minute to the future.
        m_locks_info.next_lock_attempt_delay = std::chrono::minutes(1);
        m_locks_info.last_locking_attempt.restart();

        int released = released_locks.load(std::memory_order_relaxed);
        const char LOCK_DELAY_MSG[] = "Will not attempt to reacquire locks for 1 minute.";
        if (released > 0)
        {
            MXB_NOTICE("Released %i lock(s). %s", released, LOCK_DELAY_MSG);
            success = true;
        }
        else
        {
            PRINT_MXS_JSON_ERROR(error_out, "Did not release any locks. %s", LOCK_DELAY_MSG);
        }
    }
    else
    {
        PRINT_MXS_JSON_ERROR(error_out, "Server locks are not in use, cannot release them.");
    }
    rval.success = success;
    return rval;
}

bool MariaDBMonitor::fetch_cmd_result(json_t** output)
{
    using ExecState = ManualCommand::ExecState;
    auto current_state = ExecState::NONE;
    string current_cmd_name;
    ManualCommand::Result cmd_result;

    std::unique_lock<std::mutex> lock(m_manual_cmd.lock);
    // Copy the manual command related fields to local variables under the lock.
    current_state = m_manual_cmd.exec_state.load(std::memory_order_acquire);
    if (current_state != ExecState::NONE)
    {
        current_cmd_name = m_manual_cmd.cmd_name;
        if (current_state == ExecState::DONE)
        {
            // Deep copy the json, as another manual command may start writing to the container
            // right after mutex is released.
            cmd_result.deep_copy_from(m_manual_cmd.cmd_result);
        }
    }
    lock.unlock();

    // The string contents here must match with GUI code.
    const char cmd_running_fmt[] = "No manual command results are available, %s is still %s.";
    switch (current_state)
    {
    case ExecState::NONE:
        // Command has not been ran.
        *output = mxs_json_error_append(*output, "No manual command results are available.");
        break;

    case ExecState::SCHEDULED:
        *output = mxs_json_error_append(*output, cmd_running_fmt, current_cmd_name.c_str(), "pending");
        break;

    case ExecState::RUNNING:
        *output = mxs_json_error_append(*output, cmd_running_fmt, current_cmd_name.c_str(), "running");
        break;

    case ExecState::DONE:
        if (cmd_result.success)
        {
            *output = json_sprintf("%s completed successfully.", current_cmd_name.c_str());
        }
        else if (cmd_result.errors)
        {
            *output = cmd_result.errors;
        }
        else
        {
            // Command failed, but printed no results.
            *output = json_sprintf("%s failed.", current_cmd_name.c_str());
        }
        break;
    }
    return true;
}

bool MariaDBMonitor::schedule_cs_add_node(const std::string& host, std::chrono::seconds timeout,
                                          json_t** error_out)
{
    auto func = [this, host, timeout]() {
        return manual_cs_add_node(host, timeout);
    };
    return schedule_manual_command(func, cs_add_node_cmd, error_out);
}

bool MariaDBMonitor::schedule_cs_remove_node(const std::string& host, std::chrono::seconds timeout,
                                             json_t** error_out)
{
    auto func = [this, host, timeout]() {
        return manual_cs_remove_node(host, timeout);
    };
    return schedule_manual_command(func, cs_remove_node_cmd, error_out);
}

MariaDBMonitor::CsRestResult MariaDBMonitor::check_cs_rest_result(const mxb::http::Response& resp)
{
    bool rval = false;
    string err_str;
    mxb::Json json_data;

    if (resp.is_success())
    {
        // The response body should be json text. Parse it.
        if (json_data.load_string(resp.body))
        {
            rval = true;
        }
        else
        {
            err_str = mxb::string_printf("REST-API call succeeded yet returned data was not JSON. %s",
                                         json_data.error_msg().c_str());
        }
    }
    else if (resp.is_client_error())
    {
        err_str = mxb::string_printf("HTTP client error %d: %s", resp.code, resp.body.c_str());
    }
    else if (resp.is_fatal())
    {
        err_str = mxb::string_printf("REST-API call failed. Error %d: %s. %s", resp.code,
                                     mxb::http::Response::to_string(resp.code), resp.body.c_str());
    }
    else if (resp.is_server_error())
    {
        err_str = mxb::string_printf("Server error %d: %s.", resp.code,
                                     mxb::http::Response::to_string(resp.code));
    }
    else
    {
        err_str = mxb::string_printf("Unexpected response from server. Error %d: %s.",
                                     resp.code, mxb::http::Response::to_string(resp.code));
    }

    return {rval, err_str, json_data};
}

MariaDBMonitor::ManualCommand::Result
MariaDBMonitor::manual_cs_add_node(const std::string& host, std::chrono::seconds timeout)
{
    RestDataFields input = {{"timeout", std::to_string(timeout.count())},
                            {"node", mxb::string_printf("\"%s\"", host.c_str())}};
    auto [ok, rest_error, rest_output] = run_cs_rest_cmd(HttpCmd::PUT, "node", input, timeout);

    ManualCommand::Result rval;
    if (ok)
    {
        // TODO: check output data
        rval.success = true;
    }
    else
    {
        string errmsg = mxb::string_printf("Could not add node %s to the ColumnStore cluster: %s",
                                           host.c_str(), rest_error.c_str());
        rval.errors = mxs_json_error_append(rval.errors, "%s", errmsg.c_str());
        MXB_ERROR("%s", errmsg.c_str());
    }
    return rval;
}

MariaDBMonitor::ManualCommand::Result
MariaDBMonitor::manual_cs_remove_node(const std::string& host, std::chrono::seconds timeout)
{
    RestDataFields input = {{"timeout", std::to_string(timeout.count())},
                            {"node", mxb::string_printf("\"%s\"", host.c_str())}};
    auto [ok, rest_error, rest_output] = run_cs_rest_cmd(HttpCmd::DELETE, "node", input, timeout);

    ManualCommand::Result rval;
    if (ok)
    {
        // TODO: check output data
        rval.success = true;
    }
    else
    {
        string errmsg = mxb::string_printf("Could not remove node %s from the ColumnStore cluster: %s",
                                           host.c_str(), rest_error.c_str());
        rval.errors = mxs_json_error_append(rval.errors, "%s", errmsg.c_str());
        MXB_ERROR("%s", errmsg.c_str());
    }
    return rval;
}

MariaDBMonitor::CsRestResult
MariaDBMonitor::run_cs_rest_cmd(HttpCmd httcmd, const std::string& rest_cmd, const RestDataFields& data,
                                std::chrono::seconds cs_timeout)
{
    auto& srvs = servers();
    if (srvs.empty())
    {
        return {false, "No valid server to send ColumnStore REST-API command found", mxb::Json()};
    }
    else
    {
        // Send the command to the first server. TODO: send to master instead?
        string url = mxb::string_printf("https://%s:%li%s/cluster/%s",
                                        srvs.front()->server->address(), m_settings.cs_admin_port,
                                        m_settings.cs_admin_base_path.c_str(), rest_cmd.c_str());
        string body = "{";
        string sep;
        for (const auto& elem : data)
        {
            body.append(sep).append("\"").append(elem.first).append("\": ").append(elem.second);
            sep = ", ";
        }
        body += "}";

        // Set the timeout to larger than the timeout specified to the ColumnStore daemon. The timeout surely
        // expires first in the daemon and only then in the HTTP library.
        mxb::http::Config http_config = m_http_config;
        http_config.timeout = cs_timeout + std::chrono::seconds(mxb::http::DEFAULT_TIMEOUT);

        mxb::http::Response response;

        switch (httcmd)
        {
        case HttpCmd::GET:
            response = mxb::http::get(url, http_config);
            break;

        case HttpCmd::PUT:
            response = mxb::http::put(url, body, http_config);
            break;

        case HttpCmd::DELETE:
            // TODO: add delete to mxb::http
            // response = mxb::http::delete(url, body, http_config);
            mxb_assert(!true);
            break;
        }

        return check_cs_rest_result(response);
    }
}