/*
 * Copyright (c) 2018 MariaDB Corporation Ab
 * Copyright (c) 2023 MariaDB plc, Finnish Branch
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
#pragma once

#include <maxscale/ccdefs.hh>
#include <memory>
#include <maxscale/filter.hh>
#include "module.hh"

namespace maxscale
{

/**
 * An instance of FilterModule represents a filter module.
 */
class FilterModule : public SpecificModule<FilterModule, FILTER_API>
{
    FilterModule(const FilterModule&);
    FilterModule& operator=(const FilterModule&);

public:
    static const char*           zName; /*< The name describing the module type. */
    static const mxs::ModuleType type {mxs::ModuleType::FILTER};

    class Session;
    class Instance
    {
        Instance(const Instance&);
        Instance& operator=(const Instance&);
    public:

        /**
         * Create a new filter session.
         *
         * @param pSession  The session to which the filter session belongs.
         *
         * @return A new filter session or NULL if the creation failed.
         */
        std::unique_ptr<Session> newSession(MXS_SESSION* pSession, SERVICE* pService,
                                            mxs::Routable* pDown, mxs::Routable* pUp);

    private:
        friend class FilterModule;

        Instance(FilterModule* pModule, std::unique_ptr<mxs::Filter> sInstance);

    private:
        friend class Session;

        bool routeQuery(mxs::Routable* pFilter_session, GWBUF&& statement)
        {
            return m_module.routeQuery(pFilter_session, std::move(statement));
        }

        bool clientReply(mxs::Routable* pFilter_session, GWBUF&& statement, const mxs::Reply& reply)
        {
            return m_module.clientReply(pFilter_session, std::move(statement), reply);
        }

    private:
        FilterModule&                m_module;
        std::unique_ptr<mxs::Filter> m_sInstance;
    };

    class Session
    {
        Session(const Session&);
        Session& operator=(const Session&);

    public:
        ~Session();

        /**
         * The following member functions correspond to the MaxScale filter API.
         */
        bool routeQuery(GWBUF&& statement)
        {
            return m_instance.routeQuery(m_sFilter_session.get(), std::move(statement));
        }

        bool clientReply(GWBUF&& buffer, const mxs::Reply& reply)
        {
            return m_instance.clientReply(m_sFilter_session.get(), std::move(buffer), reply);
        }

        mxs::Routable* routable()
        {
            return m_sFilter_session.get();
        }

    private:
        friend class Instance;

        Session(Instance* pInstance, std::shared_ptr<mxs::Routable> sFilter_session);

    private:
        Instance&                      m_instance;
        std::shared_ptr<mxs::Routable> m_sFilter_session;
    };

    /**
     * Create a new instance.
     *
     * @param zName        The name of the instance (config file section name),
     * @param pzOptions    Optional options.
     * @param pParameters  Configuration parameters.
     *
     * @return A new instance or NULL if creation failed.
     */
    std::unique_ptr<Instance> createInstance(const char* zName, mxs::ConfigParameters* pParameters);

private:
    friend class Instance;

    std::shared_ptr<mxs::FilterSession> newSession(mxs::Filter* pInstance,
                                                   MXS_SESSION* pSession,
                                                   SERVICE* pService)
    {
        return pInstance->newSession(pSession, pService);
    }

    bool routeQuery(mxs::Routable* pFilter_session, GWBUF&& statement)
    {
        return pFilter_session->routeQuery(std::move(statement));
    }

    bool clientReply(mxs::Routable* pFilter_session,
                     GWBUF&& statement,
                     const mxs::Reply& reply)
    {
        mxs::ReplyRoute route;
        return pFilter_session->clientReply(std::move(statement), route, reply);
    }

private:
    // Not accepted by CentOS6: friend Base;
    friend class SpecificModule<FilterModule, FILTER_API>;

    FilterModule(const MXS_MODULE* pModule)
        : SpecificModule<FilterModule, FILTER_API>(pModule)
    {
    }
};
}
