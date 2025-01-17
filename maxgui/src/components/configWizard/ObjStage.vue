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
  objType: { type: String, required: true },
  stageDataMap: { type: Object, required: true },
  getModules: { type: Function, required: true },
})
const emit = defineEmits(['on-obj-created', 'next'])
const { SERVICES, SERVERS, MONITORS, LISTENERS, FILTERS } = MXS_OBJ_TYPE_MAP

const store = useStore()
const {
  lodash: { merge },
  scrollToFirstErrMsgInput,
} = useHelpers()
const typy = useTypy()
const { t } = useI18n()

const { createObj } = useMxsObjActions(props.objType)

const search_keyword = computed(() => store.state.search_keyword)
const modules = computed(() => props.getModules(props.objType))

const existingIds = computed(() =>
  Object.keys(props.stageDataMap).flatMap((type) => getAllObjsByType(type).map((obj) => obj.id))
)
const allServers = computed(() => getAllObjsByType(SERVERS))
const allFilters = computed(() => getAllObjsByType(FILTERS))
const allServices = computed(() => getAllObjsByType(SERVICES))
const newObjs = computed(() => getNewObjsByType(props.objType))
const isNextDisabled = computed(() => (props.objType === FILTERS ? false : !newObjs.value.length))
const moduleParamsProps = computed(() => ({
  modules: modules.value,
  search: search_keyword.value,
  validate: validateForm,
}))

const formValidity = ref(null)
const objId = ref('')
const formRef = ref(null)
const formCtrRef = ref(null)

/**
 * @param {string} param.type - stage type
 * @param {string} param.field - field name. i.e. newObjMap or existingObjMap
 * @returns {object} either newObjMap or existingObjMap
 */
function getObjMap({ type, field }) {
  return typy(props.stageDataMap[type], field).safeObjectOrEmpty
}

/**
 * @param {string} type object type
 * @returns {array} existing objects and recently added objects
 */
function getAllObjsByType(type) {
  return Object.values(
    merge({}, getObjMap({ type, field: 'existingObjMap' }), getObjMap({ type, field: 'newObjMap' }))
  )
}

/**
 * @param {string} type object type
 * @returns {array} recently added objects
 */
function getNewObjsByType(type) {
  return Object.values(getObjMap({ type, field: 'newObjMap' })).map(({ id, type }) => ({
    id,
    type,
  }))
}

async function validateForm() {
  await typy(formCtrRef.value, `validate`).safeFunction()
}

function emptyObjId() {
  objId.value = ''
  nextTick(() => typy(formCtrRef.value, `resetValidation`).safeFunction())
}

async function create() {
  await createObj({
    id: objId.value,
    successCb: () => {
      emit('on-obj-created', { id: objId.value, type: props.objType })
      emptyObjId()
    },
    ...formRef.value.getValues(),
  })
}

async function handleCreate() {
  await validateForm()
  if (formValidity.value === false) scrollToFirstErrMsgInput()
  else await create()
}
</script>

<template>
  <VForm ref="formCtrRef" v-model="formValidity" class="fill-height" validate-on="lazy input">
    <WizardStageCtr
      className="pa-0"
      headerClassName="pl-9 pt-1"
      bodyClassName="pl-9 fill-height"
      footerClassName="pl-9 pt-4"
    >
      <template #header>
        <div class="form--header d-flex flex-column flex-grow-1">
          <ObjIdInput v-model="objId" :type="objType" :allObjIds="existingIds" />
          <VDivider class="my-3" />
        </div>
      </template>
      <template #body>
        <ServerForm
          v-if="objType === SERVERS"
          ref="formRef"
          :withRelationship="false"
          :moduleParamsProps="moduleParamsProps"
        />
        <MonitorForm
          v-else-if="objType === MONITORS"
          ref="formRef"
          :allServers="allServers"
          :defaultItems="getNewObjsByType(SERVERS)"
          :moduleParamsProps="moduleParamsProps"
        />
        <FilterForm
          v-else-if="objType === FILTERS"
          ref="formRef"
          :moduleParamsProps="moduleParamsProps"
        />
        <ServiceForm
          v-else-if="objType === SERVICES"
          ref="formRef"
          :allFilters="allFilters"
          :defRoutingTargetItems="getNewObjsByType(SERVERS)"
          :defFilterItem="getNewObjsByType(FILTERS)"
          :moduleParamsProps="moduleParamsProps"
        />
        <ListenerForm
          v-else-if="objType === LISTENERS"
          ref="formRef"
          :allServices="allServices"
          :defaultItems="$typy(getNewObjsByType(SERVICES), '[0]').safeObject"
          :moduleParamsProps="moduleParamsProps"
        />
      </template>
      <template #footer>
        <VBtn
          class="mt-auto mr-2 font-weight-medium px-7 text-capitalize"
          color="primary"
          rounded
          :variant="isNextDisabled ? 'flat' : 'outlined'"
          :disabled="formValidity === false"
          @click="handleCreate"
        >
          {{ t('createObj') }}
        </VBtn>
        <VBtn
          v-if="objType !== LISTENERS"
          class="mt-auto font-weight-medium px-7 text-capitalize"
          color="primary"
          rounded
          :variant="isNextDisabled ? 'outlined' : 'flat'"
          :disabled="isNextDisabled"
          @click="emit('next')"
        >
          {{ t('next') }}
        </VBtn>
        <VBtn
          v-else-if="!isNextDisabled"
          class="mt-auto font-weight-medium px-7 text-capitalize"
          color="primary"
          rounded
          variant="flat"
          to="/visualization/configuration"
        >
          {{ t('visualizeConfig') }}
        </VBtn>
      </template>
    </WizardStageCtr>
  </VForm>
</template>
