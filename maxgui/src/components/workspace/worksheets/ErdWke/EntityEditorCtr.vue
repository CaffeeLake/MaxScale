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
import ErdTaskTmp from '@wsModels/ErdTaskTmp'
import Worksheet from '@wsModels/Worksheet'
import TblStructureEditor from '@wsComps/TblStructureEditor/TblStructureEditor.vue'
import schemaInfoService from '@wsServices/schemaInfoService'

import { SNACKBAR_TYPE_MAP } from '@/constants'

const props = defineProps({
  dim: { type: Object, required: true },
  data: { type: Object, required: true },
  taskId: { type: String, required: true },
  connId: { type: String, required: true },
  tables: { type: Array, required: true },
  schemas: { type: Array, required: true },
  erdTaskTmp: { type: Object, required: true },
})
const emit = defineEmits(['change'])

const {
  lodash: { keyBy, isEqual, cloneDeep },
} = useHelpers()
const typy = useTypy()
const store = useStore()
const { t } = useI18n()

const editorRef = ref(null)
const stagingData = ref(null)

const lookupTables = computed(() => keyBy(props.tables, 'id'))
const connReqData = computed(() => ({
  id: props.connId,
  config: Worksheet.getters('activeRequestConfig'),
}))
const activeSpec = computed({
  get: () => typy(props.erdTaskTmp, 'active_spec').safeString,
  set: (v) => {
    ErdTaskTmp.update({ where: props.taskId, data: { active_spec: v } })
  },
})

let unwatch_stagingData

watch(
  () => props.data,
  (v) => {
    if (!typy(v).isEmptyObject && !isEqual(v, stagingData.value)) {
      typy(unwatch_stagingData).safeFunction()
      stagingData.value = cloneDeep(v)
      watch_stagingData()
    }
  },
  { deep: true, immediate: true }
)
watch(
  () => props.connId,
  async (v) => {
    if (v)
      //TODO: Rename connId to id
      await schemaInfoService.querySuppData({
        connId: connReqData.value.id,
        config: connReqData.value.config,
      })
  },
  { immediate: true }
)

onBeforeUnmount(() => typy(unwatch_stagingData).safeFunction())

function watch_stagingData() {
  unwatch_stagingData = watch(stagingData, (data) => emit('change', data), { deep: true })
}

async function validate() {
  if (await editorRef.value.validate()) return true
  else
    store.commit('mxsApp/SET_SNACK_BAR_MESSAGE', {
      text: [t('errors.requiredFields')],
      type: SNACKBAR_TYPE_MAP.ERROR,
    })
}

async function close() {
  if (await validate())
    ErdTaskTmp.update({
      where: props.taskId,
      data: { graph_height_pct: 100, active_entity_id: '' },
    })
}

defineExpose({ validate })
</script>

<template>
  <TblStructureEditor
    v-if="stagingData"
    ref="editorRef"
    v-model="stagingData"
    v-model:activeSpec="activeSpec"
    class="fill-height border-top--table-border er-editor-ctr"
    :dim="dim"
    :initialData="{}"
    isCreating
    :schemas="schemas"
    :lookupTables="lookupTables"
    :connReqData="connReqData"
    :showApplyBtn="false"
  >
    <template #toolbar-append>
      <VSpacer />
      <TooltipBtn
        square
        size="small"
        variant="text"
        color="error"
        data-test="close-btn"
        @click="close()"
      >
        <template #btn-content>
          <VIcon size="12" icon="mxs:close" />
        </template>
        {{ $t('close') }}
      </TooltipBtn>
    </template>
  </TblStructureEditor>
</template>
