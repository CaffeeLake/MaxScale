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
import { PERSISTENT_ORM_ENTITY_MAP } from '@/constants/workspace'

export default class InsightViewer extends Extender {
  static entity = PERSISTENT_ORM_ENTITY_MAP.INSIGHT_VIEWERS

  /**
   * @returns {Object} - return fields that are not key, relational fields
   */
  static getNonKeyFields() {
    return {
      active_spec: this.string(''),
      active_node: this.attr(null),
    }
  }

  static fields() {
    return {
      id: this.attr(null),
      ...this.getNonKeyFields(),
    }
  }
}
