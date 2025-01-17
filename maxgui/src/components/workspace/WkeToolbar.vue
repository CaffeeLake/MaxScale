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
import PrefDlg from '@wsComps/PrefDlg.vue'
import worksheetService from '@wsServices/worksheetService'

const emit = defineEmits(['get-total-btn-width'])

const store = useStore()

const toolbarLeftRef = ref(null)
const toolbarRightRef = ref(null)
const isPrefDlgOpened = ref(false)
const leftBtnsWidth = ref(0)
const rightBtnsWidth = ref(0)

const is_fullscreen = computed(() => store.state.prefAndStorage.is_fullscreen)
const totalWidth = computed(() => rightBtnsWidth.value + leftBtnsWidth.value.width)

watch(totalWidth, (v) => emit('get-total-btn-width', v), { immediate: true })

onMounted(() => {
  nextTick(() => {
    leftBtnsWidth.value = toolbarLeftRef.value.clientWidth
    rightBtnsWidth.value = toolbarRightRef.value.clientWidth
  })
})

function add() {
  worksheetService.insertBlank()
}
function toggleFullscreen() {
  store.commit('prefAndStorage/SET_IS_FULLSCREEN', !is_fullscreen.value)
}
</script>

<template>
  <div class="wke-toolbar d-flex justify-space-between border-bottom--table-border px-2">
    <div ref="toolbarLeftRef" class="d-flex align-center fill-height">
      <VBtn class="float-left add-wke-btn" icon variant="text" density="compact" @click="add">
        <VIcon size="18" color="blue-azure" icon="$mdiPlus" />
      </VBtn>
    </div>
    <div ref="toolbarRightRef" class="d-flex align-center fill-height">
      <TooltipBtn
        data-test="query-setting-btn"
        icon
        variant="text"
        density="compact"
        color="primary"
        @click="isPrefDlgOpened = !isPrefDlgOpened"
      >
        <template #btn-content>
          <VIcon size="16" icon="mxs:settings" />
          <PrefDlg v-model="isPrefDlgOpened" />
        </template>
        {{ $t('pref') }}
      </TooltipBtn>
      <TooltipBtn
        data-test="min-max-btn"
        icon
        variant="text"
        color="primary"
        density="compact"
        @click="toggleFullscreen"
      >
        <template #btn-content>
          <VIcon size="22" :icon="`$mdiFullscreen${is_fullscreen ? 'Exit' : ''}`" />
        </template>
        {{ is_fullscreen ? $t('minimize') : $t('maximize') }}
      </TooltipBtn>
    </div>
  </div>
</template>
