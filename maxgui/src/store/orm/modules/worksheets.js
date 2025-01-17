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
import Worksheet from '@wsModels/Worksheet'
import worksheetService from '@wsServices/worksheetService'

export default {
  namespaced: true,
  state: { active_wke_id: null }, // Persistence state
  getters: {
    activeId: (state) => state.active_wke_id,
    activeRecord: (state) => Worksheet.find(state.active_wke_id) || {},
    activeRequestConfig: (state) => worksheetService.findRequestConfig(state.active_wke_id),
  },
}
