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
import QueryConn from '@wsModels/QueryConn'
import QueryEditor from '@wsModels/QueryEditor'
import SchemaSidebar from '@wsModels/SchemaSidebar'
import { NODE_TYPE_MAP, NODE_NAME_KEY_MAP, SYS_SCHEMAS } from '@/constants/workspace'

export default {
  namespaced: true,
  getters: {
    schemaSql: (state, getters, rootState) => {
      const schema = NODE_NAME_KEY_MAP[NODE_TYPE_MAP.SCHEMA]
      let sql = 'SELECT * FROM information_schema.SCHEMATA'
      if (!rootState.prefAndStorage.query_show_sys_schemas_flag)
        sql += ` WHERE ${schema} NOT IN(${SYS_SCHEMAS.map((db) => `'${db}'`).join(',')})`
      sql += ` ORDER BY ${schema};`
      return sql
    },
    activeRecord: () => SchemaSidebar.find(QueryEditor.getters('activeId')) || {},
    expandedNodes: (state, getters) => getters.activeRecord.expanded_nodes || [],
    dbTreeOfConn: () => QueryEditor.getters('activeTmpRecord').db_tree_of_conn || '',
    dbTreeData: () => QueryEditor.getters('activeTmpRecord').db_tree || [],
    schemaTree: (state, getters, rootState) => {
      const tree = getters.dbTreeData
      const activeSchema = QueryConn.getters('activeSchema')
      if (rootState.prefAndStorage.identifier_auto_completion && activeSchema)
        return tree.filter((n) => n.qualified_name !== activeSchema)
      return tree
    },
  },
}
