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
import { MXS_OBJ_TYPE_MAP } from '@/constants'
import RelationshipTable from '@/components/details/RelationshipTable.vue'
import ResTimeDistHistogram from '@/components/server/ResTimeDistHistogram.vue'
import sessionsService from '@/services/sessionsService'

const props = defineProps({
  obj_data: { type: Object, required: true },
  serviceItems: { type: Array, required: true },
  handlePatchRelationship: { type: Function, required: true },
  fetchSessions: { type: Function, required: true },
})

const {
  lodash: { pickBy },
  dateFormat,
} = useHelpers()
const typy = useTypy()
const fetchObjData = useFetchObjData()

const store = useStore()
const current_sessions = computed(() => store.state.sessions.current_sessions)
const total_sessions = computed(() => store.state.sessions.total_sessions)

const sessionItems = computed(() =>
  current_sessions.value.map((session) => {
    const {
      id,
      attributes: { idle, connected, user, remote, memory, io_activity, queries },
    } = session
    return {
      id: id,
      user: `${user}@${remote}`,
      connected: dateFormat({ value: connected }),
      idle: idle,
      memory,
      io_activity,
      queries,
    }
  })
)
const serverStats = computed(() =>
  pickBy(
    typy(props.obj_data, 'attributes.statistics').safeObjectOrEmpty,
    (_, key) => key !== 'response_time_distribution'
  )
)
const resTimeDist = computed(
  () => typy(props.obj_data, 'attributes.statistics.response_time_distribution').safeObjectOrEmpty
)

async function confirmKillSession(id) {
  await sessionsService.kill({ id, callback: props.fetchSessions })
}
</script>

<template>
  <VRow>
    <VCol cols="4">
      <VRow>
        <VCol cols="12">
          <CollapsibleReadOnlyTbl :title="`${$t('statistics', 2)}`" :data="serverStats" expandAll />
        </VCol>
        <VCol cols="12">
          <RelationshipTable
            :type="MXS_OBJ_TYPE_MAP.SERVICES"
            addable
            removable
            :data="serviceItems"
            :getRelationshipData="fetchObjData"
            @confirm-update="handlePatchRelationship"
          />
        </VCol>
      </VRow>
    </VCol>
    <VCol cols="8">
      <VRow>
        <VCol cols="12">
          <ResTimeDistHistogram :resTimeDist="resTimeDist" />
        </VCol>
        <VCol cols="12">
          <CollapsibleCtr :title="`${$t('currentSessions', 2)}`" :titleInfo="sessionItems.length">
            <SessionsTable
              :items="sessionItems"
              :items-length="total_sessions"
              @confirm-kill="confirmKillSession"
              @on-update="fetchSessions"
            />
          </CollapsibleCtr>
        </VCol>
      </VRow>
    </VCol>
  </VRow>
</template>
