/*
 * Copyright (c) 2023 MariaDB plc
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
import ax from 'axios'
import { handleNullStatusCode, defErrStatusHandler } from '@/utils/axios/handlers'
import queryHttp from '@/utils/axios/workspace'
import router from '@/router'
import store from '@/store'
import { t as typy } from 'typy'

let controller = new AbortController()
const abortRequests = () => {
  controller.abort()
  // refresh the controller
  controller = new AbortController()
}

function getBaseHttp() {
  return ax.create({
    baseURL: import.meta.env.PROD ? '/' : '/api',
    headers: {
      'X-Requested-With': 'XMLHttpRequest',
      'Content-Type': 'application/json',
      'Cache-Control': 'no-cache',
    },
  })
}

// axios instance for `/auth` endpoint
const authHttp = getBaseHttp()
authHttp.interceptors.request.use(
  (config) => ({ ...config }),
  (error) => Promise.reject(error)
)

// axios instance for all endpoints except `/sql`
const http = getBaseHttp()

http.interceptors.request.use(
  (config) => ({ ...config, signal: controller.signal }),
  (error) => Promise.reject(error)
)
http.interceptors.response.use(
  (response) => {
    store.commit('mxsApp/SET_IS_SESSION_ALIVE', true)
    return response
  },
  async (error) => {
    const { response: { status = null } = {}, config } = error || {}
    switch (status) {
      case 401:
        abortRequests()
        store.commit('mxsApp/SET_IS_SESSION_ALIVE', false)
        break
      case 404:
        if (!typy(config, 'metadata.ignore404').safeBoolean) await router.push('/404')
        break
      case null:
        handleNullStatusCode({ store, error })
        break
      default:
        defErrStatusHandler({ store, error })
    }
    return Promise.reject(error)
  }
)

export { authHttp, http, queryHttp, abortRequests, getBaseHttp }
