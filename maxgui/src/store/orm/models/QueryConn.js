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
import { uuidv1 } from '@/utils/helpers'

export default class QueryConn extends Extender {
  static entity = PERSISTENT_ORM_ENTITY_MAP.QUERY_CONNS

  /**
   * @returns {Object} - return fields that are not key, relational fields
   */
  static getNonKeyFields() {
    return {
      active_db: this.string(''),
      attributes: this.attr({}),
      binding_type: this.string(''),
      /**
       * @property {string} name - connection name
       */
      meta: this.attr({}),
      clone_of_conn_id: this.attr(null).nullable(),
      is_busy: this.boolean(false),
      lost_cnn_err: this.attr({}),
    }
  }

  static fields() {
    return {
      id: this.uid(() => uuidv1()),
      ...this.getNonKeyFields(),
      //FK
      erd_task_id: this.attr(null).nullable(),
      etl_task_id: this.attr(null).nullable(),
      query_tab_id: this.attr(null).nullable(),
      query_editor_id: this.attr(null).nullable(),
    }
  }
}
