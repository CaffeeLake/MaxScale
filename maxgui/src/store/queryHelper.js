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
import {
  tryAsync,
  quotingIdentifier as quoting,
  lodash,
  getCurrentTimeStamp,
} from '@/utils/helpers'
import { t as typy } from 'typy'
import TableParser from '@/utils/TableParser'
import { SNACKBAR_TYPE_MAP } from '@/constants'
import {
  NODE_TYPE_MAP,
  NODE_GROUP_TYPE_MAP,
  UNSUPPORTED_TBL_CREATION_ENGINES,
  QUERY_LOG_TYPE_MAP,
} from '@/constants/workspace'
import QueryConn from '@wsModels/QueryConn'
import Worksheet from '@wsModels/Worksheet'
import prefAndStorageService from '@wsServices/prefAndStorageService'
import queries from '@/api/sql/queries'
import schemaNodeHelper from '@/utils/schemaNodeHelper'
import erdHelper from '@/utils/erdHelper'
import { globalI18n as i18n } from '@/plugins/i18n'
import store from '@/store'

/**
 * @public
 * @param {String} param.connId - SQL connection ID
 * @param {Object} param.nodeGroup - A node group. (NODE_GROUP_TYPE_MAP)
 * @param {Object} [param.nodeAttrs] - node attributes
 * @param {Object} param.config - axios config
 * @returns {Promise<Array>} nodes
 */
export async function getChildNodes({ connId, nodeGroup, nodeAttrs, config }) {
  const sql = schemaNodeHelper.genNodeGroupSQL({
    type: nodeGroup.type,
    schemaName: schemaNodeHelper.getSchemaName(nodeGroup),
    tblName: schemaNodeHelper.getTblName(nodeGroup),
    nodeAttrs,
  })
  const [e, res] = await tryAsync(queries.post({ id: connId, body: { sql }, config }))
  if (e) return []
  else {
    return schemaNodeHelper.genNodes({
      queryResult: typy(res, 'data.data.attributes.results[0]').safeObject,
      nodeGroup,
      nodeAttrs,
    })
  }
}

function stringifyQueryResErr(result) {
  return Object.keys(result)
    .map((key) => `${key}: ${result[key]}`)
    .join('\n')
}

/**
 * Execute one or more statements.
 * If any of the statements fail, it is logged as a failed action.
 * @param {object} param
 * @param {string} param.connId - connection id
 * @param {string} param.sql - sql to be executed
 * @param {string} param.action - action name. e.g. DROP TABLE table_name
 * @param {array<string>} [param.snackbarTypes] - message types to shown in in the SnackbarMsg component
 * @returns {Promise<[error: object|null, data: object|null]>}
 */
export async function exeSql({ connId, sql, action, snackbarTypes = [SNACKBAR_TYPE_MAP.SUCCESS] }) {
  const { meta: { name: connection_name } = {} } = QueryConn.find(connId)
  const config = Worksheet.getters('activeRequestConfig')
  const start_time = getCurrentTimeStamp()

  const [e, res] = await tryAsync(queries.post({ id: connId, body: { sql }, config }))
  // return early, api error is already handled in axios instance.
  if (e) return [e, null]

  const data = typy(res, 'data.data.attributes').safeObject
  const resErr = typy(data, 'results').safeArray.find((res) => typy(res, 'errno').isDefined)

  prefAndStorageService.pushQueryLog({
    startTime: start_time,
    name: resErr
      ? i18n.t('errors.failedToExeAction', { action })
      : i18n.t('success.exeAction', { action }),
    sql,
    res,
    connection_name,
    queryType: QUERY_LOG_TYPE_MAP.ACTION_LOGS,
  })

  if (snackbarTypes.length) {
    const type = resErr ? SNACKBAR_TYPE_MAP.ERROR : SNACKBAR_TYPE_MAP.SUCCESS
    if (snackbarTypes.includes(type))
      store.commit('mxsApp/SET_SNACK_BAR_MESSAGE', {
        text: [i18n.t(resErr ? 'errors.failedToExeAction' : 'success.exeAction', { action })],
        type,
      })
  }

  if (resErr) return [resErr, data]
  return [null, data]
}

/**
 * @param {string} param.connId - id of connection
 * @param {string} param.type - NODE_TYPE_MAP
 * @param {object[]} param.qualifiedNames - e.g. ['`test`.`t1`']
 * @param {object} param.config - axios config
 * @returns {Promise<array>}
 */
export async function queryDDL({ connId, type, qualifiedNames, config }) {
  const [e, res] = await tryAsync(
    queries.post({
      id: connId,
      body: {
        sql: qualifiedNames.map((item) => `SHOW CREATE ${type} ${item};`).join('\n'),
      },
      config,
    })
  )
  const results = typy(res, 'data.data.attributes.results').safeArray
  const errors = results.reduce((errors, result) => {
    if (result.errno) errors.push({ detail: stringifyQueryResErr(result) })
    return errors
  }, [])
  if (errors.length) return [{ response: { data: { errors } } }, []]
  return [e, typy(res, 'data.data.attributes.results').safeArray]
}

function parseTables({ res, targets }) {
  return res.reduce((result, item, i) => {
    if (typy(item, 'data').safeArray.length) {
      const ddl = typy(item, 'data[0][1]').safeString
      const schema = targets[i].schema
      const tableParser = new TableParser()
      result.push(tableParser.parse({ ddl, schema, autoGenId: true }))
    }
    return result
  }, [])
}

/**
 * @public
 * @param {string} param.connId - id of connection
 * @param {object[]} param.targets - target tables to be queried and parsed. e.g. [ {schema: 'test', tbl: 't1'} ]
 * @param {object} param.config - axios config
 * @param {object} param.charsetCollationMap
 * @returns {Promise<array>} parsed data
 */
export async function queryAndParseTblDDL({ connId, targets, config, charsetCollationMap }) {
  const [e, res] = await queryDDL({
    connId,
    type: NODE_TYPE_MAP.TBL,
    qualifiedNames: targets.map((t) => `${quoting(t.schema)}.${quoting(t.tbl)}`),
    config,
  })
  if (e) return [e, []]
  else {
    const tables = parseTables({ res, targets })
    return [
      e,
      tables.map((parsedTable) =>
        erdHelper.genTblStructureData({
          parsedTable,
          lookupTables: tables,
          charsetCollationMap,
        })
      ),
    ]
  }
}

/**
 * @public
 * @param {string} param.connId - id of connection
 * @param {object} param.config - axios config
 * @param {string} param.schemaName
 * @returns {Promise<array>}
 */
export async function querySchemaIdentifiers({ connId, config, schemaName }) {
  let results = []
  const nodeGroupTypes = Object.values(NODE_GROUP_TYPE_MAP)
  const sql = nodeGroupTypes
    .map((type) =>
      schemaNodeHelper.genNodeGroupSQL({
        type,
        schemaName,
        tblName: '',
        nodeAttrs: { onlyIdentifierWithParents: true },
      })
    )
    .join('\n')
  const [e, res] = await tryAsync(queries.post({ id: connId, body: { sql }, config }))
  if (!e) results = typy(res, 'data.data.attributes.results').safeArray
  return results
}

/**
 * @public
 * @param {string} param.connId - id of connection
 * @param {object} param.config - axios config
 * @returns {Promise<array>}
 */
export async function queryEngines({ connId, config }) {
  const [e, res] = await tryAsync(
    queries.post({
      id: connId,
      body: { sql: 'SELECT engine FROM information_schema.ENGINES' },
      config,
    })
  )
  let engines = []
  if (!e)
    engines = lodash.xorWith(
      typy(res, 'data.data.attributes.results[0].data').safeArray.flat(),
      UNSUPPORTED_TBL_CREATION_ENGINES
    )
  return engines
}

/**
 * @public
 * @param {string} param.connId - id of connection
 * @param {object} param.config - axios config
 * @returns {Promise<array>}
 */
export async function queryCharsetCollationMap({ connId, config }) {
  const [e, res] = await tryAsync(
    queries.post({
      id: connId,
      body: {
        sql: 'SELECT character_set_name, collation_name, is_default FROM information_schema.collations',
      },
      config,
    })
  )
  const map = {}
  if (!e)
    typy(res, 'data.data.attributes.results[0].data').safeArray.forEach((row) => {
      const charset = row[0]
      const collation = row[1]
      const isDefCollation = row[2] === 'Yes'
      const charsetObj = map[`${charset}`] || { collations: [] }
      if (isDefCollation) charsetObj.defCollation = collation
      charsetObj.collations.push(collation)
      map[charset] = charsetObj
    })
  return map
}

/**
 * @public
 * @param {string} param.connId - id of connection
 * @param {object} param.config - axios config
 * @returns {Promise<array>}
 */
export async function queryDefDbCharsetMap({ connId, config }) {
  const [e, res] = await tryAsync(
    queries.post({
      id: connId,
      body: {
        sql: 'SELECT schema_name, default_character_set_name FROM information_schema.schemata',
      },
      config,
    })
  )
  const map = {}
  if (!e)
    typy(res, 'data.data.attributes.results[0].data').safeArray.forEach((row) => {
      const schema_name = row[0]
      const default_character_set_name = row[1]
      map[schema_name] = default_character_set_name
    })
  return map
}

/**
 * @param {string} param.connId - id of connection
 * @param {object} param.config - axios config
 * @param {string} param.schemaName
 * @returns {Promise<array>} table identifier names
 */
export async function queryTblIdentifiers({ connId, config, schemaName }) {
  const sql = schemaNodeHelper.genNodeGroupSQL({
    type: NODE_GROUP_TYPE_MAP.TBL_G,
    schemaName,
    nodeAttrs: { onlyIdentifier: true },
  })
  const [e, res] = await tryAsync(queries.post({ id: connId, body: { sql }, config }))
  if (e) return []
  return typy(res, 'data.data.attributes.results[0].data').safeArray.flat()
}
