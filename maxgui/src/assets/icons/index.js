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
import { h } from 'vue'

const req = import.meta.glob('./*.vue', { eager: true })

const svgIcons = Object.keys(req).reduce((obj, fileName) => {
  obj[
    fileName
      .split('/')
      .pop()
      .replace(/\.\w+$/, '')
  ] = req[fileName].default
  return obj
}, {})

export default {
  component: (props) => h(svgIcons[props.icon]),
}
