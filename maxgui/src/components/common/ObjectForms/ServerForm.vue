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

const props = defineProps({
  allServices: { type: Array, default: () => [] },
  allMonitors: { type: Array, default: () => [] },
  defaultItems: { type: [Array, Object], default: () => [] },
  withRelationship: { type: Boolean, default: true },
  moduleParamsProps: { type: Object, required: true },
})

const typy = useTypy()

const defMonitor = ref([])
const defaultServiceItems = ref([])
const selectedMonitor = ref([])
const selectedServices = ref([])

const changedParams = ref({})

const servicesList = computed(() => props.allServices.map(({ id, type }) => ({ id, type })))
const monitorsList = computed(() => props.allMonitors.map(({ id, type }) => ({ id, type })))
const hasDefMonitor = computed(
  () => typy(props.defaultItems, 'type').safeString === MXS_OBJ_TYPE_MAP.MONITORS
)

watch(
  () => props.defaultItems,
  () => {
    if (props.withRelationship)
      if (hasDefMonitor.value) defMonitor.value = props.defaultItems
      else defaultServiceItems.value = props.defaultItems
  }
)

function getValues() {
  const values = { attributes: { parameters: changedParams.value } }
  if (props.withRelationship)
    values.relationships = {
      monitors: { data: selectedMonitor.value },
      services: { data: selectedServices.value },
    }
  return values
}
defineExpose({
  getValues,
  changedParams,
  selectedMonitor,
  selectedServices,
  defaultServiceItems,
  servicesList,
  monitorsList,
})
</script>

<template>
  <div class="mb-2">
    <ModuleParameters
      :defModuleId="MXS_OBJ_TYPE_MAP.SERVERS"
      hideModuleOpts
      :mxsObjType="MXS_OBJ_TYPE_MAP.SERVERS"
      @get-changed-params="changedParams = $event"
      v-bind="moduleParamsProps"
    />
    <ResourceRelationships
      v-if="withRelationship"
      type="services"
      :items="servicesList"
      multiple
      :initialValue="defaultServiceItems"
      @get-values="selectedServices = $event"
    />
    <!-- A server can be only monitored with a monitor, so multiple props is set to false-->
    <ResourceRelationships
      v-if="withRelationship"
      type="monitors"
      :items="monitorsList"
      clearable
      :initialValue="defMonitor"
      @get-values="selectedMonitor = $event"
    />
  </div>
</template>
