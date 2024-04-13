/*
 * Copyright (c) 2023 MariaDB plc
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2027-11-30
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */
import ErdTask from '@wsModels/ErdTask'
import QueryConn from '@wsModels/QueryConn'
import QueryEditor from '@wsModels/QueryEditor'
import Worksheet from '@wsModels/Worksheet'
import { QUERY_CONN_BINDING_TYPES } from '@/constants/workspace'

export default {
  namespaced: true,
  getters: {
    activeQueryEditorConn: () =>
      QueryConn.query().where('query_editor_id', QueryEditor.getters('activeId')).first() || {},
    activeQueryTabConn: () => {
      const activeQueryTabId = QueryEditor.getters('activeQueryTabId')
      if (!activeQueryTabId) return {}
      return QueryConn.query().where('query_tab_id', activeQueryTabId).first() || {}
    },
    activeSchema: (state, getters) => getters.activeQueryTabConn.active_db || '',
    activeEtlConns: () =>
      QueryConn.query().where('etl_task_id', Worksheet.getters('activeRecord').etl_task_id).get(),
    activeEtlSrcConn: (state, getters) =>
      getters.activeEtlConns.find((c) => c.binding_type === QUERY_CONN_BINDING_TYPES.ETL_SRC) || {},
    activeErdConn: () =>
      QueryConn.query().where('erd_task_id', ErdTask.getters('activeRecordId')).first() || {},
    queryEditorConns: () =>
      QueryConn.query().where('binding_type', QUERY_CONN_BINDING_TYPES.QUERY_EDITOR).get(),
    etlConns: () => {
      const { ETL_SRC, ETL_DEST } = QUERY_CONN_BINDING_TYPES
      return QueryConn.query()
        .where('binding_type', (v) => v === ETL_SRC || v === ETL_DEST)
        .get()
    },
    erdConns: () => {
      const { ERD } = QUERY_CONN_BINDING_TYPES
      return QueryConn.query()
        .where('binding_type', (v) => v === ERD)
        .get()
    },
    // Method-style getters (Uncached getters)
    findQueryTabConn: () => (query_tab_id) =>
      QueryConn.query().where('query_tab_id', query_tab_id).first() || {},
    findEtlConns: () => (etl_task_id) => QueryConn.query().where('etl_task_id', etl_task_id).get(),
    findEtlSrcConn: (state, getters) => (etl_task_id) =>
      getters
        .findEtlConns(etl_task_id)
        .find((c) => c.binding_type === QUERY_CONN_BINDING_TYPES.ETL_SRC) || {},
    findEtlDestConn: (state, getters) => (etl_task_id) =>
      getters
        .findEtlConns(etl_task_id)
        .find((c) => c.binding_type === QUERY_CONN_BINDING_TYPES.ETL_DEST) || {},
  },
}
