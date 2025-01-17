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
import EtlTask from '@wsModels/EtlTask'
import EtlTaskTmp from '@wsModels/EtlTaskTmp'
import Worksheet from '@wsModels/Worksheet'
import QueryConn from '@wsModels/QueryConn'
import queries from '@/api/sql/queries'
import etl from '@/api/sql/etl'
import store from '@/store'
import { globalI18n as i18n } from '@/plugins/i18n'
import queryConnService from '@wsServices/queryConnService'
import worksheetService from '@wsServices/worksheetService'
import { tryAsync, uuidv1, getErrorsArr, delay, lodash, getCurrentTimeStamp } from '@/utils/helpers'
import { t as typy } from 'typy'
import { stringifyErrResult } from '@/utils/queryUtils'
import schemaNodeHelper from '@/utils/schemaNodeHelper'
import { SNACKBAR_TYPE_MAP } from '@/constants'
import {
  NODE_TYPE_MAP,
  NODE_NAME_KEY_MAP,
  ETL_ACTION_MAP,
  ETL_STATUS_MAP,
  ETL_STAGE_INDEX_MAP,
  MIGR_DLG_TYPE_MAP,
  ETL_DEF_POLLING_INTERVAL,
} from '@/constants/workspace'

const col = NODE_NAME_KEY_MAP[NODE_TYPE_MAP.SCHEMA]
const schemaSql = `SELECT ${col} FROM information_schema.SCHEMATA ORDER BY ${col}`
const { INITIALIZING, COMPLETE, RUNNING, ERROR, CANCELED } = ETL_STATUS_MAP
const { CANCEL, DELETE, DISCONNECT, MIGR_OTHER_OBJS, VIEW } = ETL_ACTION_MAP

function find(id) {
  return EtlTask.find(id) || {}
}

function findTmpRecord(id) {
  return EtlTaskTmp.find(id) || {}
}

function isTaskCancelled(id) {
  return find(id).status === CANCELED
}

function findPersistedRes(id) {
  return find(id).res || {}
}

function findEtlRes(id) {
  return findTmpRecord(id).etl_res
}

function findSrcSchemaTree(id) {
  return findTmpRecord(id).src_schema_tree
}

function findCreateMode(id) {
  return findTmpRecord(id).create_mode
}

function findMigrationObjs(id) {
  return findTmpRecord(id).migration_objs
}

function findResTables(id) {
  const { tables = [] } = findEtlRes(id) || findPersistedRes(id)
  return tables
}

function findResStage(id) {
  const { stage = '' } = findEtlRes(id) || findPersistedRes(id)
  return stage
}

/**
 * If a record is deleted, then the corresponding records in the child
 * tables will be automatically deleted
 * @param {string|function} payload - either an ETL task id or a callback function that return Boolean (filter)
 */
async function cascadeDelete(payload) {
  const entityIds = EtlTask.filterEntity(EtlTask, payload).map((entity) => entity.id)
  for (const id of entityIds) {
    await queryConnService.disconnectEtlConns(id)
    EtlTask.delete(id) // delete itself
    // delete record in its the relational tables
    EtlTaskTmp.delete(id)
  }
}

function view(task) {
  const wkeId = worksheetService.findEtlTaskWkeId(task.id) || Worksheet.getters('activeId')
  Worksheet.update({ where: wkeId, data: { etl_task_id: task.id, name: task.name } })
  Worksheet.commit((state) => (state.active_wke_id = wkeId))
}

/**
 * Create an EtlTask and its mandatory relational entities
 * @param {String} name - etl task name
 */
function create(name) {
  const id = uuidv1()
  EtlTask.insert({ data: { id, name, created: Date.now() } })
  EtlTaskTmp.insert({ data: { id } })
  view(find(id))
}

async function cancel(id) {
  const config = Worksheet.getters('activeRequestConfig')
  const { id: srcConnId } = queryConnService.findEtlSrcConn(id)
  if (srcConnId) {
    const [e] = await tryAsync(queries.cancel({ id: srcConnId, config }))
    let etlStatus = CANCELED
    if (e) {
      etlStatus = ERROR
      store.commit('mxsApp/SET_SNACK_BAR_MESSAGE', {
        text: [i18n.t('error.etlCanceledFailed')],
        type: SNACKBAR_TYPE_MAP.ERROR,
      })
    }
    EtlTask.update({ where: id, data: { status: etlStatus } })
  }
}

function pushLog({ id, log }) {
  EtlTask.update({
    where: id,
    data(obj) {
      obj.logs[`${obj.active_stage_index}`].push(log)
    },
  })
}

async function fetchSrcSchemas() {
  const taskId = EtlTask.getters('activeRecord').id
  const config = Worksheet.getters('activeRequestConfig')
  if (!typy(findSrcSchemaTree(taskId)).safeArray.length) {
    pushLog({
      id: taskId,
      log: { timestamp: getCurrentTimeStamp(), name: i18n.t('info.retrievingSchemaObj') },
    })
    const [e, res] = await tryAsync(
      queries.post({
        id: QueryConn.getters('activeEtlSrcConn').id,
        body: { sql: schemaSql },
        config,
      })
    )
    let logName = ''
    if (e) logName = i18n.t('errors.retrieveSchemaObj')
    else {
      const result = typy(res, 'data.data.attributes.results[0]').safeObject
      if (typy(result, 'errno').isDefined) {
        logName = i18n.t('errors.retrieveSchemaObj')
        logName += `\n${stringifyErrResult(result)}`
      } else {
        EtlTaskTmp.update({
          where: taskId,
          data: {
            src_schema_tree: schemaNodeHelper.genNodes({
              queryResult: result,
              nodeAttrs: { isEmptyChildren: true },
            }),
          },
        })
        logName = i18n.t('success.retrieved')
      }
    }
    pushLog({ id: taskId, log: { timestamp: getCurrentTimeStamp(), name: logName } })
  }
}

/**
 * @param {String} param.type - ETL_ACTION_MAP
 * @param {Object} param.task - etl task
 */
async function actionHandler({ type, task }) {
  switch (type) {
    case CANCEL:
      await cancel(task.id)
      break
    case DELETE:
      store.commit('workspace/SET_MIGR_DLG', {
        etl_task_id: task.id,
        type: MIGR_DLG_TYPE_MAP.DELETE,
        is_opened: true,
      })
      break
    case DISCONNECT:
      await queryConnService.disconnectEtlConns(task.id)
      break
    case MIGR_OTHER_OBJS:
      await fetchSrcSchemas()
      EtlTask.update({ where: task.id, data: { active_stage_index: ETL_STAGE_INDEX_MAP.SRC_OBJ } })
      break
    case VIEW:
      view(task)
      break
  }
}

/**
 * @param {String} param.id - etl task id
 * @param {Array} param.tables - tables for preparing etl or start etl
 */
async function handleEtlCall({ id, tables }) {
  const config = worksheetService.findEtlTaskRequestConfig(id)

  const srcConn = queryConnService.findEtlSrcConn(id)
  const destConn = queryConnService.findEtlDestConn(id)
  if (srcConn.id && destConn.id) {
    const task = find(id) || {}

    let logName, apiAction, status
    const timestamp = getCurrentTimeStamp()

    const body = { target: destConn.id, type: task.meta.src_type, tables }

    if (task.is_prepare_etl) {
      logName = i18n.t('info.preparingMigrationScript')
      apiAction = etl.prepare
      status = RUNNING
      body.create_mode = findCreateMode(id)
    } else {
      logName = i18n.t('info.startingMigration')
      apiAction = etl.start
      status = RUNNING
    }
    if (body.type === 'generic') body.catalog = typy(srcConn, 'active_db').safeString

    EtlTask.update({
      where: id,
      data(obj) {
        obj.status = status
        delete obj.meta.async_query_id
      },
    })
    const [e, res] = await tryAsync(apiAction({ id: srcConn.id, body, config }))
    if (e) {
      status = ERROR
      logName = `${i18n.t('errors.failedToPrepareMigrationScript')} ${getErrorsArr(e).join('. ')}`
    }
    const queryId = typy(res, 'data.data.id').safeString
    EtlTask.update({
      where: id,
      data(obj) {
        obj.status = status
        if (!e) obj.meta.async_query_id = queryId // Persist query id
      },
    })
    pushLog({ id, log: { timestamp, name: logName } })
  } else
    store.commit('mxsApp/SET_SNACK_BAR_MESSAGE', {
      text: ['Connection expired, please reconnect.'],
      type: SNACKBAR_TYPE_MAP.ERROR,
    })
}

/**
 * @param {String} id - etl task id
 */
async function getEtlCallRes(id) {
  const task = EtlTask.find(id)
  const config = worksheetService.findEtlTaskRequestConfig(id)
  const queryId = typy(task, 'meta.async_query_id').safeString
  const srcConn = queryConnService.findEtlSrcConn(id)

  const ignoreKeys = ['create', 'insert', 'select']

  let etlStatus, migrationRes

  const [e, res] = await tryAsync(queries.getAsyncRes({ id: srcConn.id, queryId, config }))
  if (!e) {
    const results = typy(res, 'data.data.attributes.results').safeObject
    let logMsg
    if (res.status === 202) {
      EtlTaskTmp.update({ where: id, data: { etl_res: results } })
      const { etl_polling_interval } = store.state.workspace
      const newInterval = etl_polling_interval * 2
      store.commit('workspace/SET_ETL_POLLING_INTERVAL', newInterval <= 4000 ? newInterval : 5000)
      await delay(etl_polling_interval).then(async () => await getEtlCallRes(id))
    } else if (res.status === 201) {
      const timestamp = getCurrentTimeStamp()
      const ok = typy(results, 'ok').safeBoolean

      if (task.is_prepare_etl) {
        logMsg = i18n.t(ok ? 'success.prepared' : 'errors.failedToPrepareMigrationScript')
        etlStatus = ok ? INITIALIZING : ERROR
      } else {
        logMsg = i18n.t(ok ? 'success.migration' : 'errors.migration')
        etlStatus = ok ? COMPLETE : ERROR
        if (isTaskCancelled(id)) {
          logMsg = i18n.t('warnings.migrationCanceled')
          etlStatus = CANCELED
        }
        migrationRes = {
          ...results,
          tables: results.tables.map((obj) =>
            lodash.pickBy(obj, (v, key) => !ignoreKeys.includes(key))
          ),
        }
      }

      const error = typy(results, 'error').safeString
      if (error) logMsg += ` \n${error}`

      pushLog({ id, log: { timestamp, name: logMsg } })

      EtlTaskTmp.update({ where: id, data: { etl_res: results } })
      store.commit('workspace/SET_ETL_POLLING_INTERVAL', ETL_DEF_POLLING_INTERVAL)
    }
  }
  EtlTask.update({
    where: id,
    data(obj) {
      if (etlStatus) obj.status = etlStatus
      if (migrationRes) obj.res = migrationRes
    },
  })
}

export default {
  find,
  findEtlRes,
  findSrcSchemaTree,
  findCreateMode,
  findMigrationObjs,
  findResTables,
  findResStage,
  cascadeDelete,
  view,
  create,
  pushLog,
  fetchSrcSchemas,
  actionHandler,
  handleEtlCall,
  getEtlCallRes,
}
