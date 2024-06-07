/*
 * Copyright (c) 2023 MariaDB plc
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2029-02-28
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */
import QueryConn from '@wsModels/QueryConn'
import QueryEditor from '@wsModels/QueryEditor'
import QueryTabTmp from '@wsModels/QueryTabTmp'
import Worksheet from '@wsModels/Worksheet'
import queries from '@/api/sql/queries'
import store from '@/store'
import queryConnService from '@wsServices/queryConnService'
import prefAndStorageService from '@wsServices/prefAndStorageService'
import { QUERY_MODES, QUERY_LOG_TYPES, QUERY_CANCELED } from '@/constants/workspace'
import { tryAsync, immutableUpdate } from '@/utils/helpers'
import { t as typy } from 'typy'

/**
 * @param {String} param.qualified_name - Table id (database_name.table_name).
 * @param {String} param.query_mode - a key in QUERY_MODES. Either PRVW_DATA or PRVW_DATA_DETAILS
 */
async function queryPrvw({ qualified_name, query_mode }) {
  const config = Worksheet.getters('activeRequestConfig')
  const { id, meta: { name: connection_name } = {} } = QueryConn.getters('activeQueryTabConn')
  const activeQueryTabId = QueryEditor.getters('activeQueryTabId')
  const request_sent_time = new Date().valueOf()
  let field, sql, queryName
  switch (query_mode) {
    case QUERY_MODES.PRVW_DATA:
      sql = `SELECT * FROM ${qualified_name} LIMIT 1000;`
      queryName = `Preview ${qualified_name} data`
      field = 'prvw_data'
      break
    case QUERY_MODES.PRVW_DATA_DETAILS:
      sql = `DESCRIBE ${qualified_name};`
      queryName = `View ${qualified_name} details`
      field = 'prvw_data_details'
      break
  }
  QueryTabTmp.update({
    where: activeQueryTabId,
    data(obj) {
      obj[field].request_sent_time = request_sent_time
      obj[field].total_duration = 0
      obj[field].is_loading = true
    },
  })
  const [e, res] = await tryAsync(
    queries.post({
      id,
      body: { sql, max_rows: store.state.prefAndStorage.query_row_limit },
      config,
    })
  )
  if (e)
    QueryTabTmp.update({
      where: activeQueryTabId,
      data(obj) {
        obj[field].is_loading = false
      },
    })
  else {
    const now = new Date().valueOf()
    const total_duration = ((now - request_sent_time) / 1000).toFixed(4)
    QueryTabTmp.update({
      where: activeQueryTabId,
      data(obj) {
        obj[field].data = Object.freeze(res.data.data)
        obj[field].total_duration = parseFloat(total_duration)
        obj[field].is_loading = false
      },
    })
    prefAndStorageService.pushQueryLog({
      startTime: now,
      name: queryName,
      sql,
      res,
      connection_name,
      queryType: QUERY_LOG_TYPES.ACTION_LOGS,
    })
  }
}

/**
 * @param {array} param.statements - Array of statement objects.
 * @param {string} param.sql - SQL string joined from statements.
 */
async function executeSQL({ statements, sql }) {
  const config = Worksheet.getters('activeRequestConfig')
  const { id, meta: { name: connection_name } = {} } = QueryConn.getters('activeQueryTabConn')
  const request_sent_time = new Date().valueOf()
  const activeQueryTabId = QueryEditor.getters('activeQueryTabId')
  const abortController = new AbortController()
  store.commit('queryResultsMem/UPDATE_ABORT_CONTROLLER_MAP', {
    id: activeQueryTabId,
    value: abortController,
  })
  QueryTabTmp.update({
    where: activeQueryTabId,
    data(obj) {
      obj.query_results.request_sent_time = request_sent_time
      obj.query_results.total_duration = 0
      obj.query_results.is_loading = true
    },
  })

  let [e, res] = await tryAsync(
    queries.post({
      id,
      body: { sql, max_rows: store.state.prefAndStorage.query_row_limit },
      config: { ...config, signal: abortController.signal },
    })
  )
  const now = new Date().valueOf()
  const total_duration = ((now - request_sent_time) / 1000).toFixed(4)

  if (!e && res && sql.match(/(use|drop database)\s/i)) await queryConnService.updateActiveDb()

  if (typy(QueryTabTmp.find(activeQueryTabId), 'has_kill_flag').safeBoolean) {
    // If the KILL command was sent for the query is being run, the query request is aborted
    QueryTabTmp.update({ where: activeQueryTabId, data: { has_kill_flag: false } })
    /**
     * This is done automatically in queryHttp.interceptors.response.
     * However, because the request is aborted, is_busy needs to be set manually.
     */
    QueryConn.update({ where: id, data: { is_busy: false } })
    res = { data: { data: { attributes: { results: [{ message: QUERY_CANCELED }], sql } } } }
  }
  /* TODO: The current mapping doesn't guarantee correctness since the statements may be
   * split incorrectly. Once the API supports an array of statements instead of a single SQL string,
   * this approach should work properly.
   */
  res = immutableUpdate(res, {
    data: {
      data: {
        attributes: {
          results: {
            $set: typy(res.data.data.attributes.results).safeArray.map((item, i) => ({
              ...item,
              executedStatement: statements[i],
            })),
          },
        },
      },
    },
  })
  QueryTabTmp.update({
    where: activeQueryTabId,
    data(obj) {
      obj.query_results.data = Object.freeze(res.data.data)
      obj.query_results.total_duration = parseFloat(total_duration)
      obj.query_results.is_loading = false
    },
  })

  prefAndStorageService.pushQueryLog({
    startTime: request_sent_time,
    sql,
    res,
    connection_name,
    queryType: QUERY_LOG_TYPES.USER_LOGS,
  })
  store.commit('queryResultsMem/DELETE_ABORT_CONTROLLER', activeQueryTabId)
}

/**
 * This action uses the current active QueryEditor connection to send
 * KILL QUERY thread_id
 */
async function killQuery() {
  const config = Worksheet.getters('activeRequestConfig')
  const activeQueryTabConn = QueryConn.getters('activeQueryTabConn')
  const activeQueryTabId = QueryEditor.getters('activeQueryTabId')
  const queryEditorConn = QueryConn.getters('activeQueryEditorConn')

  // abort the query first then send kill flag
  const abort_controller = store.getters['queryResultsMem/getAbortController'](activeQueryTabId)
  abort_controller.abort() // abort the running query

  QueryTabTmp.update({
    where: activeQueryTabId,
    data: { has_kill_flag: true },
  })
  const [e, res] = await tryAsync(
    queries.post({
      id: queryEditorConn.id,
      body: { sql: `KILL QUERY ${activeQueryTabConn.attributes.thread_id}` },
      config,
    })
  )
  if (!e) {
    const results = typy(res, 'data.data.attributes.results').safeArray
    if (typy(results, '[0].errno').isDefined)
      store.commit('mxsApp/SET_SNACK_BAR_MESSAGE', {
        text: [
          'Failed to stop the query',
          ...Object.keys(results[0]).map((key) => `${key}: ${results[0][key]}`),
        ],
        type: 'error',
      })
  }
}

/**
 * This action clears prvw_data and prvw_data_details to empty object.
 * Call this action when user selects option in the sidebar.
 * This ensure sub-tabs in Data Preview tab are generated with fresh data
 */
function clearDataPreview() {
  QueryTabTmp.update({
    where: QueryEditor.getters('activeQueryTabId'),
    data(obj) {
      obj.prvw_data = {}
      obj.prvw_data_details = {}
    },
  })
}

async function queryProcessList() {
  const config = Worksheet.getters('activeRequestConfig')
  const { id } = QueryConn.getters('activeQueryTabConn')
  const activeQueryTabId = QueryEditor.getters('activeQueryTabId')

  QueryTabTmp.update({
    where: activeQueryTabId,
    data(obj) {
      obj.process_list.is_loading = true
    },
  })
  const [, res] = await tryAsync(
    queries.post({
      id,
      body: {
        sql: 'SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST',
        max_rows: store.state.prefAndStorage.query_row_limit,
      },
      config,
    })
  )
  QueryTabTmp.update({
    where: activeQueryTabId,
    data(obj) {
      obj.process_list.is_loading = false
      obj.process_list.data = Object.freeze(res.data.data)
    },
  })
}

export default { queryPrvw, executeSQL, killQuery, clearDataPreview, queryProcessList }