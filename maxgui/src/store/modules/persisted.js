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
import { DEF_REFRESH_RATE_MAP } from '@/constants'
import { genSetMutations, lodash } from '@/utils/helpers'
import { t as typy } from 'typy'
import router from '@/router'

const states = () => ({
  dsh_graphs_cnf: {
    sessions: { annotations: {} },
    connections: { annotations: {} },
    load: { annotations: {} },
  },
  are_dsh_graphs_expanded: false,
})

export default {
  namespaced: true,
  // Place here any states need to be persisted without being cleared when logging out
  state: () => ({
    refresh_rate_by_route_group: lodash.cloneDeep(DEF_REFRESH_RATE_MAP),
    ...states(),
  }),
  mutations: {
    UPDATE_REFRESH_RATE_BY_ROUTE_GROUP(state, { group, payload }) {
      state.refresh_rate_by_route_group[group] = payload
    },
    ...genSetMutations(states()),
  },
  getters: {
    currRefreshRate: (state) => {
      const group = typy(router, 'currentRoute.value.meta.group').safeString
      if (group) return state.refresh_rate_by_route_group[group]
      return 10
    },
  },
}
