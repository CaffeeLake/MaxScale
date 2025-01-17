<script setup>
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
import { MRDB_MON, MXS_OBJ_TYPE_MAP } from '@/constants'
import RepTooltip from '@/components/dashboard/RepTooltip.vue'
import AnchorLink from '@/components/dashboard/AnchorLink.vue'

const store = useStore()
const { t } = useI18n()
const typy = useTypy()
const {
  lodash: { groupBy, cloneDeep },
} = useHelpers()

const commonPaddingProps = { class: 'pa-0 pl-6' }
const smallPaddingProps = { class: 'pa-0 pl-4' }
const HEADERS = [
  {
    title: `Monitor`,
    value: 'monitorId',
    cellProps: smallPaddingProps,
    headerProps: smallPaddingProps,
    autoTruncate: true,
  },
  {
    title: 'State',
    value: 'monitorState',
    cellProps: { class: 'pa-0 border-right--table-border' },
    headerProps: smallPaddingProps,
    customRender: {
      renderer: 'StatusIcon',
      objType: MXS_OBJ_TYPE_MAP.MONITORS,
      props: smallPaddingProps,
    },
  },
  {
    title: 'Servers',
    value: 'id',
    cellProps: { class: 'pa-0' },
    headerProps: commonPaddingProps,
    autoTruncate: true,
  },
  {
    title: 'Address',
    value: 'serverAddress',
    cellProps: { ...commonPaddingProps, style: { maxWidth: '160px' } },
    headerProps: commonPaddingProps,
    autoTruncate: true,
  },
  {
    title: 'Connections',
    value: 'serverConnections',
    cellProps: commonPaddingProps,
    headerProps: commonPaddingProps,
    autoTruncate: true,
  },
  {
    title: 'Operations',
    value: 'serverActiveOperations',
    cellProps: { class: 'pa-0' },
    headerProps: commonPaddingProps,
  },
  {
    title: 'State',
    value: 'serverState',
    cellProps: { class: 'pa-0' },
    headerProps: commonPaddingProps,
  },
  {
    title: 'GTID',
    value: 'gtid',
    cellProps: commonPaddingProps,
    headerProps: commonPaddingProps,
  },
  {
    title: 'Services',
    value: 'serviceIds',
    autoTruncate: true,
    headerProps: smallPaddingProps,
    cellProps: { class: 'pa-0' },
    customRender: {
      renderer: 'RelationshipItems',
      objType: MXS_OBJ_TYPE_MAP.SERVICES,
      props: { class: 'px-4' },
    },
  },
]

const rowspanCols = ['monitorId', 'monitorState']

const highlightGroupedIds = ref([])
const rowspanColId = ref('')
const { sortBy, toggleSortBy, compareFn } = useSortBy({ key: 'monitorId', isDesc: false })

const search_keyword = computed(() => store.state.search_keyword)
const allServers = computed(() => store.state.servers.all_objs)
const monitorsMap = computed(() => store.getters['monitors/monitorsMap'])
const totalMonitors = computed(() => store.getters['monitors/total'])
const allMonitorIds = computed(() => Object.keys(monitorsMap.value))
const totalMap = computed(() => ({
  monitorId: totalMonitors.value,
  id: allServers.value.length,
  serviceIds: totalServices.value,
}))

const data = computed(() => {
  const rows = []
  if (allServers.value.length) {
    const activeMonitorIds = [] // ids of monitors that are monitoring servers
    allServers.value.forEach((server) => {
      const {
        id,
        attributes: {
          state: serverState,
          parameters: { address, port, socket },
          statistics: { connections: serverConnections, active_operations: serverActiveOperations },
          gtid_current_pos: gtid,
        },
        relationships: {
          services: { data: servicesData = [] } = {},
          monitors: { data: monitorsData = [] } = {},
        },
      } = server

      const serviceIds = servicesData.length
        ? servicesData.map((item) => item.id)
        : t('noEntity', [MXS_OBJ_TYPE_MAP.SERVICES])

      const row = {
        id,
        serverAddress: socket ? socket : `${address}:${port}`,
        serverConnections,
        serverActiveOperations,
        serverState,
        serviceIds,
        gtid: String(gtid),
      }

      if (!typy(monitorsMap.value).isEmptyObject && monitorsData.length) {
        // The monitorsData is always an array with one element -> get monitor at index 0
        const {
          id: monitorId = null,
          attributes: {
            state: monitorState,
            module: monitorModule,
            monitor_diagnostics: { master: masterName, server_info = [] } = {},
          } = {},
        } = monitorsMap.value[monitorsData[0].id] || {}

        if (monitorId) {
          activeMonitorIds.push(monitorId)
          row.monitorId = monitorId
          row.monitorState = monitorState
          if (monitorModule === MRDB_MON) {
            if (masterName === row.id) {
              row.isMaster = true
              row.serverInfo = server_info.filter((server) => server.name !== masterName)
            } else {
              row.isSlave = true
              row.serverInfo = server_info.filter((server) => server.name === row.id)
            }
          }
        }
      } else {
        row.monitorId = t('not', { action: 'monitored' })
        row.monitorState = ''
      }
      rows.push(row)
    })

    // push monitors that don't monitor any servers to rows
    allMonitorIds.value.forEach((id) => {
      if (!activeMonitorIds.includes(id))
        rows.push({
          id: '',
          serverAddress: '',
          serverConnections: '',
          serverActiveOperations: '',
          serverState: '',
          serviceIds: '',
          gtid: '',
          monitorId: id,
          monitorState: typy(monitorsMap.value[id], 'attributes.state').safeString,
        })
    })
  }
  if (sortBy.value.key) rows.sort(compareFn)
  return rows
})

const dataGrouped = computed(() => groupBy(data.value, 'monitorId'))

const items = computed(() => setCellAttrs(dataGrouped.value))

const totalServices = useCountUniqueValues({ data: items, field: 'serviceIds' })

/**
 * This function groups all items have same monitorId then assign
 * `hidden` and `rowspan` attributes
 * @param {object} dataGrouped
 * @return {array}
 */
function setCellAttrs(dataGrouped) {
  return Object.keys(dataGrouped).reduce((result, monitorId) => {
    const group = dataGrouped[`${monitorId}`]
    group.forEach((item, i) => {
      result.push(cloneDeep({ ...group[i], hidden: i !== 0, rowspan: group.length }))
    })
    return result
  }, [])
}

function isCooperative(id) {
  return typy(monitorsMap.value[id], 'attributes.monitor_diagnostics.primary').safeBoolean
}

function setRowspanBg({ e, item, header }) {
  if (e.type !== 'mouseenter') {
    highlightGroupedIds.value = []
    rowspanColId.value = ''
  } else {
    if (rowspanCols.includes(header.value))
      highlightGroupedIds.value = dataGrouped.value[item.monitorId].map((row) => row.id)
    else {
      const rowspanRowId = typy(dataGrouped.value[item.monitorId], '[0].id').safeString
      if (rowspanRowId) {
        highlightGroupedIds.value = []
        rowspanColId.value = rowspanRowId
      }
    }
  }
}

function getCellBgColor({ id, header }) {
  if (rowspanColId.value)
    return isRowspanCol(header) && id === rowspanColId.value ? '#F2FCFF' : 'unset'
  else return highlightGroupedIds.value.includes(id) ? '#F2FCFF' : 'unset'
}

function isRowspanCol(header) {
  return rowspanCols.includes(header.value)
}

function getActiveOperationsTooltipData(item) {
  return {
    collection: {
      'Active Operations': item.serverActiveOperations,
      connections: item.serverConnections,
    },
    location: 'right',
  }
}
</script>

<template>
  <VDataTable
    :headers="HEADERS"
    :items="items"
    :search="search_keyword"
    :itemsPerPage="-1"
    hide-default-footer
  >
    <template #headers="{ columns }">
      <tr>
        <template v-for="column in columns" :key="column.value">
          <CustomTblHeader
            :column="column"
            :sortBy="sortBy"
            :total="totalMap[column.value]"
            :showTotal="typy(totalMap[column.value]).isDefined"
            @click="toggleSortBy(column.value)"
          />
        </template>
      </tr>
    </template>
    <template #item="{ item, columns }">
      <tr class="v-data-table__tr">
        <CustomTblCol
          v-for="(h, i) in columns"
          :key="h.value"
          :value="item[h.value]"
          :name="h.value"
          :rowspan="isRowspanCol(h) ? item.rowspan : 1"
          :class="{ 'd-none': isRowspanCol(h) ? item.hidden : false }"
          :search="search_keyword"
          :autoTruncate="h.autoTruncate"
          :style="{ backgroundColor: getCellBgColor({ id: item.id, header: h }) }"
          v-bind="columns[i].cellProps"
          @mouseenter="setRowspanBg({ e: $event, item, header: h })"
          @mouseleave="setRowspanBg({ e: $event, item, header: h })"
        >
          <template #[`item.monitorId`]="{ value: monitorId, highlighter }">
            <div class="d-flex align-center justify-space-between">
              <span
                v-if="monitorId === $t('not', { action: 'monitored' })"
                v-mxs-highlighter="highlighter"
              >
                {{ monitorId }}
              </span>
              <GblTooltipActivator
                v-else
                :data="{ txt: monitorId }"
                tag="div"
                activateOnTruncation
                class="cursor--pointer"
                fillHeight
              >
                <AnchorLink
                  type="monitors"
                  :txt="`${monitorId}`"
                  :highlighter="highlighter"
                  class="font-weight-bold"
                />
              </GblTooltipActivator>
              <span v-if="isCooperative(monitorId)" class="ml-1 text-success text-caption">
                Primary
              </span>
            </div>
          </template>

          <template #[`item.id`]="{ value: id, highlighter }">
            <RepTooltip
              v-if="$typy(item.serverInfo, '[0].slave_connections').safeArray.length"
              :disabled="!(item.isSlave || item.isMaster)"
              :serverInfo="item.serverInfo"
              :isMaster="item.isMaster"
              activatorClass="pl-6"
            >
              <template #activator>
                <AnchorLink
                  class="text-truncate"
                  :type="MXS_OBJ_TYPE_MAP.SERVERS"
                  :txt="`${id}`"
                  :highlighter="highlighter"
                />
              </template>
            </RepTooltip>
            <GblTooltipActivator
              v-else
              :data="{ txt: id }"
              tag="div"
              activateOnTruncation
              class="pl-6 cursor--pointer"
              fillHeight
            >
              <AnchorLink
                :type="MXS_OBJ_TYPE_MAP.SERVERS"
                :txt="`${id}`"
                :highlighter="highlighter"
              />
            </GblTooltipActivator>
          </template>

          <template #[`item.serverActiveOperations`]="{ value }">
            <GblTooltipActivator
              v-if="item.id"
              :data="getActiveOperationsTooltipData(item)"
              fillHeight
              tag="div"
              class="pl-6 d-flex align-center cursor--pointer"
            >
              <VSheet :width="70">
                <VProgressLinear
                  :height="12"
                  :min="0"
                  :max="item.serverConnections"
                  :model-value="value"
                  :bg-opacity="0.3"
                  bg-color="grayed-out"
                  color="success"
                  rounded="sm"
                />
              </VSheet>
            </GblTooltipActivator>
          </template>

          <template #[`item.serverState`]="{ value: serverState, highlighter }">
            <RepTooltip
              v-if="serverState"
              :disabled="!(item.isSlave || item.isMaster)"
              :serverInfo="$typy(item, 'serverInfo').safeArray"
              :isMaster="item.isMaster"
              activatorClass="pl-6"
            >
              <template #activator>
                <StatusIcon
                  size="16"
                  class="mr-1 server-state-icon"
                  :type="MXS_OBJ_TYPE_MAP.SERVERS"
                  :value="serverState"
                />
                <span v-mxs-highlighter="highlighter">
                  {{ serverState }}
                </span>
              </template>
            </RepTooltip>
          </template>

          <template v-if="h.customRender" #[`item.${h.value}`]="{ value, highlighter }">
            <CustomCellRenderer
              :value="value"
              :componentName="h.customRender.renderer"
              :objType="h.customRender.objType"
              :mixTypes="typy(h.customRender, 'mixTypes').safeBoolean"
              :highlighter="highlighter"
              v-bind="h.customRender.props"
            />
          </template>
        </CustomTblCol>
      </tr>
    </template>
  </VDataTable>
</template>
