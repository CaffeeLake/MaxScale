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
import Extender from '@/store/orm/Extender'
import { PERSISTENT_ORM_ENTITY_MAP, TABLE_STRUCTURE_SPEC_MAP } from '@/constants/workspace'

export default class TblEditor extends Extender {
  static entity = PERSISTENT_ORM_ENTITY_MAP.TBL_EDITORS

  /**
   * @returns {Object} - return fields that are not key, relational fields
   */
  static getNonKeyFields() {
    return {
      active_spec: this.string(TABLE_STRUCTURE_SPEC_MAP.COLUMNS),
      data: this.attr({}),
      active_node: this.attr(null),
      is_fetching: this.boolean(true),
    }
  }

  static fields() {
    return {
      id: this.attr(null),
      ...this.getNonKeyFields(),
    }
  }
}
