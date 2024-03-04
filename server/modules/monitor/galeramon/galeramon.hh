/*
 * Copyright (c) 2018 MariaDB Corporation Ab
 * Copyright (c) 2023 MariaDB plc, Finnish Branch
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2028-02-27
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */
#pragma once

/**
 * @file galeramon.hh - The Galera cluster monitor
 */

#include <maxscale/ccdefs.hh>
#include <unordered_map>
#include <maxscale/monitor.hh>

/**
 *  Galera status variables
 */
struct GaleraNode
{
    int         joined;         /**< Node is in sync with the cluster */
    int         local_index;    /**< Node index */
    int         local_state;    /**< Node state */
    int         cluster_size;   /**< The cluster size*/
    std::string cluster_uuid;   /**< Cluster UUID */
    std::string gtid_binlog_pos;
    std::string gtid_current_pos;
    std::string comment;
    bool        read_only = false;
    int         master_id;
    int         server_id;
};

typedef std::unordered_map<mxs::MonitorServer*, GaleraNode> NodeMap;

class GaleraMonitor : public maxscale::MonitorWorkerSimple
{
public:
    GaleraMonitor(const GaleraMonitor&) = delete;
    GaleraMonitor& operator=(const GaleraMonitor&) = delete;

    ~GaleraMonitor();
    static GaleraMonitor* create(const std::string& name, const std::string& module);
    json_t*               diagnostics() const override;
    json_t*               diagnostics(mxs::MonitorServer* server) const override;
    std::string           annotate_state_change(mxs::MonitorServer* server) override;

    mxs::config::Configuration& configuration() override final;

protected:
    bool has_sufficient_permissions() override;
    void update_server_status(mxs::MonitorServer* monitored_server) override;
    void pre_tick() override;
    void post_tick() override;
    bool can_be_disabled(const mxs::MonitorServer& server, DisableType type,
                         std::string* errmsg_out) const override;

    class Config : public mxs::config::Configuration
    {
    public:
        Config(const std::string& name, GaleraMonitor* monitor);

        bool post_configure(const std::map<std::string, mxs::ConfigParameters>& nested_params) override final;

        bool disable_master_failback;       /**< Monitor flag for Galera Cluster Master failback */
        bool available_when_donor;          /**< Monitor flag for Galera Cluster Donor availability */
        bool disable_master_role_setting;   /**< Monitor flag to disable setting master role */
        bool root_node_as_master;           /**< Use node with wsrep_local_index=0 as master */
        bool use_priority;                  /**< Use server priorities */
        bool set_donor_nodes;               /**< Set the wrep_sst_donor variable */

    private:
        GaleraMonitor* m_monitor;
    };

private:

    Config      m_config;
    std::string m_cluster_uuid;         /**< The Cluster UUID */
    bool        m_log_no_members;       /**< Should we log if no member are found. */
    NodeMap     m_info;                 /**< Contains Galera Cluster variables of all nodes */
    NodeMap     m_prev_info;            /**< Contains the info from the previous tick */
    int         m_cluster_size;         /**< How many nodes in the cluster */

    // Prevents concurrent use that might occur during the diagnostics_json call
    mutable std::mutex m_lock;

    GaleraMonitor(const std::string& name, const std::string& module);

    mxs::MonitorServer* get_candidate_master();
    void                set_galera_cluster();
    void                update_sst_donor_nodes(int is_cluster);
    void                calculate_cluster();

    bool        post_configure();
    friend bool Config::post_configure(const std::map<std::string, mxs::ConfigParameters>& nested_params);
};
