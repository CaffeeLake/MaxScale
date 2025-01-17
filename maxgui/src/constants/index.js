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

export const PERSIST_TOKEN_OPT = 'persist=yes&max-age=604800'

export const TOOLTIP_DEBOUNCE = 300
export const LOADING_TIME = 400 // loading time animation for table

// Do not alter the order of the keys as it's used for generating steps for the Config Wizard page
export const MXS_OBJ_TYPE_MAP = Object.freeze({
  SERVERS: 'servers',
  MONITORS: 'monitors',
  FILTERS: 'filters',
  SERVICES: 'services',
  LISTENERS: 'listeners',
})

export const LOGO = `
.___  ___.      ___      ___   ___      _______.  ______      ___       __       _______
|   \\/   |     /   \\     \\  \\ /  /     /       | /      |    /   \\     |  |     |   ____|
|  \\  /  |    /  ^  \\     \\  V  /     |   (----'|  ,----'   /  ^  \\    |  |     |  |__
|  |\\/|  |   /  /_\\  \\     >   <       \\   \\    |  |       /  /_\\  \\   |  |     |   __|
|  |  |  |  /  _____  \\   /  .  \\  .----)   |   |  '----. /  _____  \\  |  '----.|  |____
|__|  |__| /__/     \\__\\ /__/ \\__\\ |_______/     \\______|/__/     \\__\\ |_______||_______|
`

export const ROUTING_TARGET_RELATIONSHIP_TYPES = Object.freeze([
  MXS_OBJ_TYPE_MAP.SERVERS,
  MXS_OBJ_TYPE_MAP.SERVICES,
  MXS_OBJ_TYPE_MAP.MONITORS,
])

// routes having children routes
export const ROUTE_GROUP_MAP = Object.freeze({
  DASHBOARD: 'dashboard',
  VISUALIZATION: 'visualization',
  CLUSTER: 'cluster',
  DETAIL: 'detail',
})

export const DEF_REFRESH_RATE_MAP = Object.freeze({
  [ROUTE_GROUP_MAP.DASHBOARD]: 10,
  [ROUTE_GROUP_MAP.VISUALIZATION]: 60,
  [ROUTE_GROUP_MAP.CLUSTER]: 60,
  [ROUTE_GROUP_MAP.DETAIL]: 10,
})

export const LOG_PRIORITIES = ['alert', 'error', 'warning', 'notice', 'info', 'debug']

export const SERVER_OP_TYPE_MAP = Object.freeze({
  MAINTAIN: 'maintain',
  CLEAR: 'clear',
  DRAIN: 'drain',
  DELETE: 'delete',
})

export const SERVICE_OP_TYPE_MAP = Object.freeze({
  STOP: 'stop',
  START: 'start',
  DESTROY: 'destroy',
})

export const MONITOR_OP_TYPE_MAP = Object.freeze({
  STOP: 'stop',
  START: 'start',
  DESTROY: 'destroy',
  SWITCHOVER: 'async-switchover',
  RESET_REP: 'async-reset-replication',
  RELEASE_LOCKS: 'async-release-locks',
  FAILOVER: 'async-failover',
  REJOIN: 'async-rejoin',
  CS_GET_STATUS: 'async-cs-get-status',
  CS_STOP_CLUSTER: 'async-cs-stop-cluster',
  CS_START_CLUSTER: 'async-cs-start-cluster',
  CS_SET_READONLY: 'async-cs-set-readonly',
  CS_SET_READWRITE: 'async-cs-set-readwrite',
  CS_ADD_NODE: 'async-cs-add-node',
  CS_REMOVE_NODE: 'async-cs-remove-node',
})

export const COMMON_OBJ_OP_TYPE_MAP = Object.freeze({ DESTROY: 'destroy' })

export const USER_ROLE_MAP = Object.freeze({ ADMIN: 'admin', BASIC: 'basic' })

export const USER_ADMIN_ACTION_MAP = Object.freeze({
  DELETE: 'delete',
  UPDATE: 'update',
  ADD: 'add',
})

export const DURATION_UNITS = Object.freeze(['ms', 's', 'm', 'h'])

export const SIZE_UNITS = Object.freeze(['Ki', 'Mi', 'Gi', 'Ti', 'k', 'M', 'G', 'T'])

export const MRDB_MON = 'mariadbmon'

export const MRDB_PROTOCOL = 'MariaDBProtocol'

const TIME_REF_POINT_KEYS = [
  'NOW',
  'START_OF_TODAY',
  'END_OF_YESTERDAY',
  'START_OF_YESTERDAY',
  'NOW_MINUS_2_DAYS',
  'NOW_MINUS_LAST_WEEK',
  'NOW_MINUS_LAST_2_WEEKS',
  'NOW_MINUS_LAST_MONTH',
]

export const TIME_REF_POINTS = Object.freeze(
  TIME_REF_POINT_KEYS.reduce((obj, key) => ({ ...obj, [key]: key }), {})
)

export const ZOOM_OPTS = [25, 50, 100, 125, 150, 200]

export const DIAGRAM_CTX_TYPE_MAP = Object.freeze({
  NODE: 'node',
  LINK: 'link',
  BOARD: 'board',
})

export const SNACKBAR_TYPE_MAP = {
  SUCCESS: 'success',
  ERROR: 'error',
  INFO: 'info',
  WARNING: 'warning',
}
