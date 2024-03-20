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
import { genSetMutations, lodash } from '@/utils/helpers'
import { http } from '@/utils/axios'

const states = () => ({
  all_filters: [],
  obj_data: {},
})

export default {
  namespaced: true,
  state: states(),
  mutations: genSetMutations(states()),
  actions: {
    async fetchAll({ commit }) {
      const [, res] = await this.vue.$helpers.tryAsync(http.get('/filters'))
      commit('SET_ALL_FILTERS', this.vue.$typy(res, 'data.data').safeArray)
    },
  },
  getters: {
    total: (state) => state.all_filters.length,
    map: (state) => lodash.keyBy(state.all_filters, 'id'),
  },
}
