/*
 * Copyright (c) 2016 MariaDB Corporation Ab
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

#include <maxscale/ccdefs.hh>
#include "maxscale/filtermodule.hh"
#include "../../../core/internal/modules.hh"

using std::unique_ptr;

namespace maxscale
{

//
// FilterModule
//

const char* FilterModule::zName = MODULE_FILTER;

unique_ptr<FilterModule::Instance> FilterModule::createInstance(const char* zFilter_name,
                                                                mxs::ConfigParameters* pParameters)
{
    unique_ptr<Instance> sInstance;

    std::unique_ptr<mxs::Filter> sFilter = m_pApi->createInstance(zFilter_name);

    if (sFilter)
    {
        if (sFilter->getConfiguration().configure(*pParameters))
        {
            sInstance.reset(new Instance(this, std::move(sFilter)));
        }
    }

    return sInstance;
}

//
// FilterModule::Instance
//

FilterModule::Instance::Instance(FilterModule* pModule, std::unique_ptr<mxs::Filter> sInstance)
    : m_module(*pModule)
    , m_sInstance(std::move(sInstance))
{
}

unique_ptr<FilterModule::Session> FilterModule::Instance::newSession(MXS_SESSION* pSession,
                                                                     SERVICE* pService,
                                                                     mxs::Routable* down,
                                                                     mxs::Routable* up)
{
    unique_ptr<Session> sFilter_session;

    auto pFilter_session = m_module.newSession(m_sInstance.get(), pSession, pService);

    if (pFilter_session)
    {
        pFilter_session->setDownstream(down);
        pFilter_session->setUpstream(up);
        sFilter_session.reset(new Session(this, pFilter_session));
    }

    return sFilter_session;
}

//
// FilterModule::Session
//

FilterModule::Session::Session(Instance* pInstance, std::shared_ptr<mxs::Routable> sFilter_session)
    : m_instance(*pInstance)
    , m_sFilter_session(std::move(sFilter_session))
{
}

FilterModule::Session::~Session()
{
}
}
