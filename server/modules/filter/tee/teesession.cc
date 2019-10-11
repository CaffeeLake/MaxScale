/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

#include "teesession.hh"
#include "tee.hh"

#include <set>
#include <string>

#include <maxscale/listener.hh>
#include <maxscale/modutil.hh>

TeeSession::TeeSession(MXS_SESSION* session,
                       SERVICE* service,
                       LocalClient* client,
                       pcre2_code* match,
                       pcre2_match_data* md_match,
                       pcre2_code* exclude,
                       pcre2_match_data* md_exclude)
    : mxs::FilterSession(session, service)
    , m_client(client)
    , m_match(match)
    , m_md_match(md_match)
    , m_exclude(exclude)
    , m_md_exclude(md_exclude)
{
}

TeeSession* TeeSession::create(Tee* my_instance, MXS_SESSION* session, SERVICE* service)
{
    LocalClient* client = NULL;
    pcre2_code* match = NULL;
    pcre2_code* exclude = NULL;
    pcre2_match_data* md_match = NULL;
    pcre2_match_data* md_exclude = NULL;

    if (my_instance->is_enabled()
        && my_instance->user_matches(session_get_user(session))
        && my_instance->remote_matches(session_get_remote(session)))
    {
        match = my_instance->get_match();
        exclude = my_instance->get_exclude();

        if ((match && (md_match = pcre2_match_data_create_from_pattern(match, NULL)) == NULL)
            || (exclude && (md_exclude = pcre2_match_data_create_from_pattern(exclude, NULL)) == NULL))
        {
            MXS_OOM();
            return NULL;
        }

        client = LocalClient::create(session, my_instance->get_service());

        if (client)
        {
            client->connect();
        }
        else
        {
            const char* extra = !listener_find_by_service(my_instance->get_service()).empty() ? "" :
                ": Service has no network listeners";
            MXS_ERROR("Failed to create local client connection to '%s'%s",
                      my_instance->get_service()->name(), extra);
            return NULL;
        }
    }

    TeeSession* tee = new(std::nothrow) TeeSession(session, service, client,
                                                   match, md_match, exclude, md_exclude);

    if (!tee)
    {
        pcre2_match_data_free(md_match);
        pcre2_match_data_free(md_exclude);

        if (client)
        {
            delete client;
        }
    }

    return tee;
}

TeeSession::~TeeSession()
{
    delete m_client;
}

void TeeSession::close()
{
}

int TeeSession::routeQuery(GWBUF* queue)
{
    if (m_client && query_matches(queue))
    {
        m_client->queue_query(gwbuf_deep_clone(queue));
    }

    return mxs::FilterSession::routeQuery(queue);
}

void TeeSession::diagnostics(DCB* pDcb)
{
}

json_t* TeeSession::diagnostics_json() const
{
    return NULL;
}

bool TeeSession::query_matches(GWBUF* buffer)
{
    bool rval = true;

    if (m_match || m_exclude)
    {
        char* sql;
        int len;

        if (modutil_extract_SQL(buffer, &sql, &len))
        {
            if (m_match && pcre2_match_8(m_match,
                                         (PCRE2_SPTR)sql,
                                         len,
                                         0,
                                         0,
                                         m_md_match,
                                         NULL) < 0)
            {
                MXS_INFO("Query does not match the 'match' pattern: %.*s", len, sql);
                rval = false;
            }
            else if (m_exclude && pcre2_match_8(m_exclude,
                                                (PCRE2_SPTR)sql,
                                                len,
                                                0,
                                                0,
                                                m_md_exclude,
                                                NULL) >= 0)
            {
                MXS_INFO("Query matches the 'exclude' pattern: %.*s", len, sql);
                rval = false;
            }
        }
    }

    return rval;
}
