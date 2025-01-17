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
import { genSetMutations, lodash } from '@/utils/helpers'

const states = () => ({ all_objs: [], obj_data: {}, monitor_diagnostics: {} })

export default {
  namespaced: true,
  state: states(),
  mutations: genSetMutations(states()),
  getters: {
    total: (state) => state.all_objs.length,
    monitorsMap: (state) => lodash.keyBy(state.all_objs, 'id'),
  },
}
