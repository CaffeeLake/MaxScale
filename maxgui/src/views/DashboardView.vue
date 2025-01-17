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

import ViewHeader from '@/components/dashboard/ViewHeader.vue'
import DashboardGraphs from '@/components/dashboard/DashboardGraphs.vue'
import ServersTbl from '@/components/dashboard/ServersTbl.vue'
import ServicesTbl from '@/components/dashboard/ServicesTbl.vue'
import ListenersTbl from '@/components/dashboard/ListenersTbl.vue'
import FiltersTbl from '@/components/dashboard/FiltersTbl.vue'
import SessionsTbl from '@/components/dashboard/SessionsTbl.vue'
import maxscaleService from '@/services/maxscaleService'
import sessionsService from '@/services/sessionsService'
import { MXS_OBJ_TYPE_MAP } from '@/constants'

const store = useStore()
const typy = useTypy()
const fetchObjects = useFetchObjects()

const activeTab = ref(null)
const graphsRef = ref(null)

const TABS = [
  MXS_OBJ_TYPE_MAP.SERVERS,
  'sessions',
  MXS_OBJ_TYPE_MAP.SERVICES,
  MXS_OBJ_TYPE_MAP.LISTENERS,
  MXS_OBJ_TYPE_MAP.FILTERS,
]

const tabActions = TABS.map((name) => () => {
  if (name === 'sessions') return sessionsService.fetchSessions()
  return fetchObjects(name)
})
const pageTitle = computed(() => `MariaDB MaxScale ${store.state.maxscale.maxscale_version}`)
const countMap = computed(() => {
  return {
    filters: store.getters['filters/total'],
    listeners: store.getters['listeners/total'],
    servers: store.getters['servers/total'],
    services: store.getters['services/total'],
    sessions: store.getters['sessions/total'],
  }
})

onBeforeMount(async () => {
  await maxscaleService.fetchOverviewInfo()
  await fetchAll()
  // Init graph datasets
  await typy(graphsRef.value, 'initDatasets').safeFunction()
})

async function fetchAll() {
  await Promise.all([
    maxscaleService.fetchThreadStats(),
    fetchObjects(MXS_OBJ_TYPE_MAP.MONITORS),
    ...tabActions.map((action) => action()),
    maxscaleService.fetchConfigSync(),
  ])
}

async function onCountDone() {
  const timestamp = Date.now()
  await Promise.all([fetchAll(), typy(graphsRef.value, 'updateChart').safeFunction(timestamp)])
}

function loadTabComponent(name) {
  switch (name) {
    case MXS_OBJ_TYPE_MAP.SERVERS:
      return ServersTbl
    case MXS_OBJ_TYPE_MAP.SERVICES:
      return ServicesTbl
    case MXS_OBJ_TYPE_MAP.LISTENERS:
      return ListenersTbl
    case MXS_OBJ_TYPE_MAP.FILTERS:
      return FiltersTbl
    case 'sessions':
      return SessionsTbl
    default:
      return 'div'
  }
}
</script>
<template>
  <ViewWrapper :title="pageTitle">
    <VSheet>
      <ViewHeader :onCountDone="onCountDone" />
      <DashboardGraphs ref="graphsRef" />
      <VTabs v-model="activeTab">
        <VTab
          v-for="name in TABS"
          :key="name"
          :to="`/dashboard/${name}`"
          :value="name"
          class="text-primary"
        >
          {{ $t(name === 'sessions' ? 'currentSessions' : name, 2) }}
          <span class="grayed-out-info"> ({{ countMap[name] }}) </span>
        </VTab>
      </VTabs>
      <VWindow v-model="activeTab">
        <VWindowItem v-for="name in TABS" :key="name" :value="name" class="pt-2">
          <component v-if="name === activeTab" :is="loadTabComponent(activeTab)" />
        </VWindowItem>
      </VWindow>
    </VSheet>
  </ViewWrapper>
</template>
