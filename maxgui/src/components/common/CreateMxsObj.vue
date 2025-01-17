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
  defFormType: { type: String, default: '' },
  defRelationshipObj: { type: Object, default: () => {} },
})

const store = useStore()
const typy = useTypy()

const dialogRef = ref(null)
const formRef = ref(null)
const isDlgOpened = ref(false)
const selectedObjType = ref('')
const objId = ref('')
const defRelationshipItems = ref([])
const serviceDefRoutingTargetItems = ref([])
const serviceDefFilterItems = ref([])

const { createObj } = useMxsObjActions(selectedObjType)
const fetchObjects = useFetchObjects()

const { items: allObjIds, fetch: fetchAllObjIds } = useFetchAllObjIds()
const { map: allModuleMap, fetch: fetchAllModules, getModules } = useFetchAllModules()

const form_type = computed(() => store.state.form_type)
const allFilters = computed(() => store.state.filters.all_objs)
const allMonitors = computed(() => store.state.monitors.all_objs)
const allServers = computed(() => store.state.servers.all_objs)
const allServices = computed(() => store.state.services.all_objs)
const isAdmin = computed(() => store.getters['users/isAdmin'])

const defRelationshipObjType = computed(() => typy(props.defRelationshipObj, 'type').safeString)
const moduleParamsProps = computed(() => {
  return {
    modules: getModules(selectedObjType.value),
    validate: typy(dialogRef.value, 'validateForm').safeFunction,
  }
})

watch(form_type, async (v) => {
  // trigger open dialog since form_type is used to open dialog without clicking button in this component
  if (v) await onCreate()
})
watch(isDlgOpened, async (v) => {
  if (v) handleSetFormType()
  else if (form_type.value) store.commit('SET_FORM_TYPE', null) // clear form_type
})
watch(selectedObjType, async (v) => {
  await onChangeObjType(v)
})
watch(objId, async (v) => {
  // add hyphens when ever input have whitespace
  objId.value = v ? v.split(' ').join('-') : v
})

async function onCreate() {
  // fetch data before open dlg
  if (typy(allModuleMap.value).isEmptyObject) await fetchAllModules()
  await fetchAllObjIds()
  isDlgOpened.value = true
}

/**
 *  global form_type state has higher priority. It is
 *  used to trigger opening form dialog without
 *  clicking the button in this component
 */
function handleSetFormType() {
  if (form_type.value) selectedObjType.value = form_type.value
  else if (props.defFormType) selectedObjType.value = props.defFormType
  else selectedObjType.value = MXS_OBJ_TYPE_MAP.SERVICES
}

/**
 * If current page is a detail page and have relationship object,
 * set default relationship items
 */
async function onChangeObjType(val) {
  const { SERVICES, SERVERS, MONITORS, LISTENERS, FILTERS } = MXS_OBJ_TYPE_MAP
  switch (val) {
    case SERVICES:
      if (defRelationshipObjType.value === SERVERS)
        serviceDefRoutingTargetItems.value = [props.defRelationshipObj]
      else if (defRelationshipObjType.value === FILTERS)
        serviceDefFilterItems.value = [props.defRelationshipObj]
      break
    case SERVERS:
      if (defRelationshipObjType.value === SERVICES)
        defRelationshipItems.value = [props.defRelationshipObj]
      else if (defRelationshipObjType.value === MONITORS)
        defRelationshipItems.value = props.defRelationshipObj
      break
    case MONITORS:
      if (defRelationshipObjType.value === SERVERS)
        defRelationshipItems.value = [props.defRelationshipObj]
      break
    case LISTENERS: {
      if (defRelationshipObjType.value === SERVICES)
        defRelationshipItems.value = props.defRelationshipObj
      break
    }
  }
}

async function onSave() {
  await createObj({
    id: objId.value,
    successCb: async () => {
      await fetchObjects(selectedObjType.value)
      reloadHandler()
    },
    ...formRef.value.getValues(),
  })
}

function reloadHandler() {
  if (defRelationshipItems.value) store.commit('SET_SHOULD_REFRESH_RESOURCE', true)
}
</script>

<template>
  <div v-if="isAdmin">
    <VBtn
      :width="160"
      color="primary"
      rounded
      variant="outlined"
      class="text-capitalize px-8 font-weight-medium"
      data-test="cancel-btn"
      @click="onCreate"
    >
      + {{ $t('createNew') }}
    </VBtn>
    <BaseDlg
      ref="dialogRef"
      v-model="isDlgOpened"
      :onSave="onSave"
      :title="`${$t('createANew')}...`"
      hasFormDivider
      minBodyWidth="635px"
    >
      <template #body>
        <ObjTypeSelect
          v-model="selectedObjType"
          :items="Object.values(MXS_OBJ_TYPE_MAP)"
          class="mt-4"
          @update:modelValue="onChangeObjType"
        />
      </template>
      <template v-if="selectedObjType" #form-body>
        <ObjIdInput v-model="objId" :type="selectedObjType" :allObjIds="allObjIds" class="mb-3" />
        <ServiceForm
          v-if="selectedObjType === MXS_OBJ_TYPE_MAP.SERVICES"
          ref="formRef"
          :allFilters="allFilters"
          :defRoutingTargetItems="serviceDefRoutingTargetItems"
          :defFilterItem="serviceDefFilterItems"
          :moduleParamsProps="moduleParamsProps"
        />
        <MonitorForm
          v-else-if="selectedObjType === MXS_OBJ_TYPE_MAP.MONITORS"
          ref="formRef"
          :allServers="allServers"
          :defaultItems="defRelationshipItems"
          :moduleParamsProps="moduleParamsProps"
        />
        <FilterForm
          v-else-if="selectedObjType === MXS_OBJ_TYPE_MAP.FILTERS"
          ref="formRef"
          :moduleParamsProps="moduleParamsProps"
        />
        <ListenerForm
          v-else-if="selectedObjType === MXS_OBJ_TYPE_MAP.LISTENERS"
          ref="formRef"
          :allServices="allServices"
          :defaultItems="defRelationshipItems"
          :moduleParamsProps="moduleParamsProps"
        />
        <ServerForm
          v-else-if="selectedObjType === MXS_OBJ_TYPE_MAP.SERVERS"
          ref="formRef"
          :allServices="allServices"
          :allMonitors="allMonitors"
          :defaultItems="defRelationshipItems"
          :moduleParamsProps="moduleParamsProps"
          class="mt-4"
        />
      </template>
    </BaseDlg>
  </div>
</template>
