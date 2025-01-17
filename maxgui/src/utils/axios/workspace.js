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
import { t as typy } from 'typy'
import { getErrorsArr, getConnId } from '@/utils/helpers'
import { SNACKBAR_TYPE_MAP } from '@/constants'
import { MARIADB_NET_ERRNO, ODBC_NET_ERR_SQLSTATE } from '@/constants/workspace'
import { handleNullStatusCode, defErrStatusHandler } from '@/utils/axios/handlers'
import QueryConn from '@wsModels/QueryConn'
import store from '@/store'
import queryConnService from '@wsServices/queryConnService'
/**
 *
 * @param {Boolean} param.value - is connection busy
 * @param {String} param.sql_conn_id - the connection id that the request is sent
 */
function updateConnBusyStatus({ value, sql_conn_id }) {
  QueryConn.update({
    where: sql_conn_id,
    data: { is_busy: value },
  })
}
/**
 * This function helps to check if there is a lost connection error and store the error
 * to lost_cnn_err.
 * @param {Object} param.res - response of every request from queryHttp axios instance
 * @param {String} param.sql_conn_id - the connection id that the request is sent
 */
function analyzeRes({ res, sql_conn_id }) {
  const result = typy(res, 'data.data.attributes.results[0]').safeObject
  const errno = typy(result, 'errno').safeNumber
  const sqlState = typy(result, 'sqlstate').safeString
  if (MARIADB_NET_ERRNO.includes(errno) || ODBC_NET_ERR_SQLSTATE.includes(sqlState)) {
    QueryConn.update({
      where: sql_conn_id,
      data: { lost_cnn_err: result },
    })
  }
}
function isValidatingRequest(url) {
  return url === '/sql'
}
function handleDefErr({ status, error }) {
  if (status === 401) store.commit('mxsApp/SET_IS_SESSION_ALIVE', false)
  else defErrStatusHandler({ store, error })
}
async function handleConnErr({ status, method, error }) {
  await queryConnService.validateConns()
  let msg = ''
  switch (status) {
    case 404:
      if (method !== 'delete') msg = 'Connection expired, please reconnect.'
      break
    case 503:
      msg = 'Connection busy, please wait.'
      break
  }
  store.commit('mxsApp/SET_SNACK_BAR_MESSAGE', {
    text: [...getErrorsArr(error), msg],
    type: SNACKBAR_TYPE_MAP.ERROR,
  })
}
/**
 * axios instance for workspace endpoint.
 * Use this for sql connection endpoint so that the value for
 * is_conn_busy_map can be set accurately.
 */
const queryHttp = ax.create({
  baseURL: import.meta.env.PROD ? '/' : '/api',
  headers: {
    'X-Requested-With': 'XMLHttpRequest',
    'Content-Type': 'application/json',
    'Cache-Control': 'no-cache',
  },
})
queryHttp.interceptors.request.use(
  (config) => {
    updateConnBusyStatus({ value: true, sql_conn_id: getConnId(config.url) })
    return config
  },
  (error) => Promise.reject(error)
)
queryHttp.interceptors.response.use(
  (response) => {
    updateConnBusyStatus({
      value: false,
      sql_conn_id: getConnId(response.config.url),
    })
    analyzeRes({ res: response, sql_conn_id: getConnId(response.config.url) })
    store.commit('mxsApp/SET_IS_SESSION_ALIVE', true)
    return response
  },
  async (error) => {
    updateConnBusyStatus({
      value: false,
      sql_conn_id: getConnId(error.config.url),
    })
    const { response: { status = null, config: { url = '', method } = {} } = {} } = error || {}
    if (status === null) handleNullStatusCode({ store, error })
    else if (isValidatingRequest(url)) handleDefErr({ status, error })
    else if (status === 404 || status === 503) await handleConnErr({ status, method, store, error })
    else handleDefErr({ status, error })
    updateConnBusyStatus({ value: false, sql_conn_id: getConnId(url) })
    return Promise.reject(error)
  }
)

export default queryHttp
