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
import ErdTaskTmp from '@wsModels/ErdTaskTmp'
import EtlTaskTmp from '@wsModels/EtlTaskTmp'
import QueryEditor from '@wsModels/QueryEditor'
import QueryEditorTmp from '@wsModels/QueryEditorTmp'
import QueryTabTmp, { QUERY_RESULT_FIELDS } from '@wsModels/QueryTabTmp'
import Worksheet from '@wsModels/Worksheet'
import WorksheetTmp from '@wsModels/WorksheetTmp'
import worksheetService from '@wsServices/worksheetService'
import store from '@/store'
import { exeSql } from '@/store/queryHelper'
import { quotingIdentifier } from '@/utils/helpers'
import { t as typy } from 'typy'

/**
 * Initialize entities that will be kept only in memory for all worksheets and queryTabs
 */
function initMemEntities() {
  const worksheets = Worksheet.all()
  worksheets.forEach((w) => {
    if (!WorksheetTmp.find(w.id)) WorksheetTmp.insert({ data: { id: w.id } })
    if (w.query_editor_id) {
      const queryEditor = QueryEditor.query()
        .where('id', w.query_editor_id)
        .with('queryTabs')
        .first()
      if (!QueryEditorTmp.find(w.query_editor_id))
        QueryEditorTmp.insert({ data: { id: queryEditor.id } })
      queryEditor.queryTabs.forEach((t) => {
        if (!QueryTabTmp.find(t.id)) QueryTabTmp.insert({ data: { id: t.id } })
      })
    } else if (w.etl_task_id && !EtlTaskTmp.find(w.etl_task_id))
      EtlTaskTmp.insert({ data: { id: w.etl_task_id } })
    else if (w.erd_task_id && !ErdTaskTmp.find(w.erd_task_id))
      ErdTaskTmp.insert({ data: { id: w.erd_task_id } })
  })
}

function initEntities() {
  if (Worksheet.all().length === 0) worksheetService.insertBlank()
  else initMemEntities()
}

async function init() {
  initEntities()
  await store.dispatch('fileSysAccess/initStorage')
}

/**
 * @param {string} param.connId - connection id
 * @param {boolean} [param.isCreating] - is creating a new table
 * @param {string} [param.schema] - schema name
 * @param {string} [param.name] - table name
 * @param {string} [param.actionName] - action name
 * @param {function} param.successCb - success callback function
 */
async function exeDdlScript({
  connId,
  isCreating = false,
  schema,
  name,
  successCb,
  actionName = '',
}) {
  let action
  if (actionName) action = actionName
  else {
    const targetObj = `${quotingIdentifier(schema)}.${quotingIdentifier(name)}`
    action = `Apply changes to ${targetObj}`
    if (isCreating) action = `Create ${targetObj}`
  }
  const [error] = await exeSql({
    connId,
    sql: store.state.workspace.exec_sql_dlg.sql,
    action,
  })
  store.commit('workspace/SET_EXEC_SQL_DLG', { ...store.state.workspace.exec_sql_dlg, error })
  if (!error) await typy(successCb).safeFunction()
}

/**
 * @param {object} data - queryTabTmp object
 * @returns {boolean}
 */
function getIsLoading(queryTabTmp) {
  return QUERY_RESULT_FIELDS.some((field) => typy(queryTabTmp, `${field}.is_loading`).safeBoolean)
}

export default { init, exeDdlScript, getIsLoading }
