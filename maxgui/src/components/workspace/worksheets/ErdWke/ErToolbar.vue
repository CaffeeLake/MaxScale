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
import ConnectionBtn from '@wsComps/ConnectionBtn.vue'
import { LINK_SHAPES } from '@/components/svgGraph/shapeConfig'
import { CONN_TYPE_MAP, OS_CMD, KEYBOARD_SHORTCUT_MAP } from '@/constants/workspace'
import { WS_KEY } from '@/constants/injectionKeys'
import workspace from '@/composables/workspace'

const props = defineProps({
  graphConfig: {
    type: Object,
    required: true,
    validator: (cnf) =>
      typeof cnf.linkShape === 'object' &&
      typeof cnf.linkShape.type === 'string' &&
      typeof cnf.link === 'object' &&
      typeof cnf.link.isAttrToAttr === 'boolean' &&
      typeof cnf.link.isHighlightAll === 'boolean',
  },
  height: { type: Number, required: true },
  zoom: { type: Number, required: true },
  isFitIntoView: { type: Boolean, required: true },
  exportOptions: {
    type: Array,
    required: true,
    validator: (arr) =>
      arr.every(
        (item) =>
          typeof item === 'object' &&
          typeof item.title === 'string' &&
          typeof item.action === 'function'
      ),
  },
  conn: { type: Object, required: true },
  nodesHistory: { type: Array, required: true },
  activeHistoryIdx: { type: Number, required: true },
})
const emit = defineEmits([
  'set-zoom', // { v:number, isFitIntoView: boolean }
  'on-undo',
  'on-redo',
  'on-apply-script',
  'change-graph-config-attr-value', // { path: string, value: any}. path. e.g. 'link.isAttrToAttr'
  'click-auto-arrange',
  'on-create-table',
])

const { CTRL_SHIFT_ENTER, META_SHIFT_ENTER, CTRL_Z, META_Z, CTRL_SHIFT_Z, META_SHIFT_Z } =
  KEYBOARD_SHORTCUT_MAP

const ALL_LINK_SHAPES = Object.values(LINK_SHAPES)
const BTN_HEIGHT = 28
const store = useStore()
workspace.useShortKeyListener({ handler: shortKeyHandler, injectKeys: [WS_KEY] })

const zoomRatio = computed({
  get: () => props.zoom,
  set: (v) => emit('set-zoom', { v }),
})
const hasConnId = computed(() => Boolean(props.conn.id))
const isUndoDisabled = computed(() => props.activeHistoryIdx === 0)
const isRedoDisabled = computed(() => props.activeHistoryIdx === props.nodesHistory.length - 1)

function genErd() {
  store.commit('workspace/SET_GEN_ERD_DLG', {
    is_opened: true,
    preselected_schemas: [],
    connection: props.conn,
    gen_in_new_ws: false,
  })
}

function shortKeyHandler(key) {
  switch (key) {
    case CTRL_Z:
    case META_Z:
      if (!isUndoDisabled.value) emit('on-undo')
      break
    case CTRL_SHIFT_Z:
    case META_SHIFT_Z:
      if (!isRedoDisabled.value) emit('on-redo')
      break
    case CTRL_SHIFT_ENTER:
    case META_SHIFT_ENTER:
      emit('on-apply-script')
      break
  }
}

function openCnnDlg() {
  store.commit('workspace/SET_CONN_DLG', { is_opened: true, type: CONN_TYPE_MAP.ERD })
}
</script>

<template>
  <div
    class="er-toolbar-ctr d-flex align-center pr-3 border-bottom--table-border"
    :style="{ minHeight: `${height}px`, maxHeight: `${height}px` }"
  >
    <VTooltip location="top">
      <template #activator="{ props }">
        <VSelect
          :modelValue="graphConfig.linkShape.type"
          :items="ALL_LINK_SHAPES"
          class="borderless-input"
          :max-width="64"
          density="compact"
          hide-details
          data-test="link-shape-select"
          @update:modelValue="
            emit('change-graph-config-attr-value', { path: 'linkShape.type', value: $event })
          "
          v-bind="props"
        >
          <template #selection="{ item }">
            <VIcon
              size="28"
              color="primary"
              :icon="`mxs:${$helpers.lodash.camelCase(item.raw)}Shape`"
            />
          </template>
          <template #item="{ props }">
            <VListItem v-bind="props">
              <template #title="{ title }">
                <VIcon
                  size="28"
                  color="primary"
                  :icon="`mxs:${$helpers.lodash.camelCase(title)}Shape`"
                />
              </template>
            </VListItem>
          </template>
        </VSelect>
      </template>
      {{ $t('shapeOfLinks') }}
    </VTooltip>
    <TooltipBtn
      class="ml-1"
      square
      size="small"
      :variant="graphConfig.link.isAttrToAttr ? 'flat' : 'text'"
      color="primary"
      data-test="attr-to-attr-btn"
      @click="
        emit('change-graph-config-attr-value', {
          path: 'link.isAttrToAttr',
          value: !graphConfig.link.isAttrToAttr,
        })
      "
    >
      <template #btn-content>
        <VIcon size="22" icon="$mdiKeyLink" />
      </template>
      {{ $t(graphConfig.link.isAttrToAttr ? 'disableDrawingFksToCols' : 'enableDrawingFksToCols') }}
    </TooltipBtn>
    <TooltipBtn
      square
      size="small"
      variant="text"
      color="primary"
      data-test="auto-arrange-btn"
      @click="emit('click-auto-arrange')"
    >
      <template #btn-content>
        <VIcon size="22" icon="$mdiArrangeSendToBack" />
      </template>
      {{ $t('autoArrangeErd') }}
    </TooltipBtn>
    <TooltipBtn
      class="mr-1"
      square
      size="small"
      :variant="graphConfig.link.isHighlightAll ? 'flat' : 'text'"
      color="primary"
      data-test="relationship-highlight-btn"
      @click="
        emit('change-graph-config-attr-value', {
          path: 'link.isHighlightAll',
          value: !graphConfig.link.isHighlightAll,
        })
      "
    >
      <template #btn-content>
        <VIcon
          size="22"
          :icon="
            graphConfig.link.isHighlightAll ? '$mdiLightbulbOnOutline' : ' $mdiLightbulbOutline'
          "
        />
      </template>
      {{
        $t(
          graphConfig.link.isHighlightAll
            ? 'turnOffRelationshipHighlight'
            : 'turnOnRelationshipHighlight'
        )
      }}
    </TooltipBtn>
    <ZoomController
      v-model:zoomRatio="zoomRatio"
      :isFitIntoView="isFitIntoView"
      class="borderless-input"
      :max-width="76"
      data-test="zoom-controller-btn"
      @update:isFitIntoView="emit('set-zoom', { isFitIntoView: $event })"
    />
    <VDivider class="align-self-center er-toolbar__separator mx-2" vertical />
    <TooltipBtn
      square
      size="small"
      variant="text"
      color="primary"
      :disabled="isUndoDisabled"
      data-test="undo-btn"
      @click="emit('on-undo')"
    >
      <template #btn-content>
        <VIcon size="22" color="primary" icon="$mdiUndo" />
      </template>
      {{ $t('undo') }}
      <br />
      <kbd>{{ OS_CMD }} + Z</kbd>
    </TooltipBtn>
    <TooltipBtn
      square
      size="small"
      variant="text"
      color="primary"
      :disabled="isRedoDisabled"
      data-test="redo-btn"
      @click="emit('on-redo')"
    >
      <template #btn-content>
        <VIcon size="22" color="primary" icon="$mdiRedo" />
      </template>
      {{ $t('redo') }}
      <br />
      <kbd>{{ OS_CMD }} + SHIFT + Z</kbd>
    </TooltipBtn>
    <VDivider class="align-self-center er-toolbar__separator mx-2" vertical />
    <TooltipBtn
      square
      size="small"
      variant="text"
      color="primary"
      data-test="create-tbl-btn"
      @click="emit('on-create-table')"
    >
      <template #btn-content>
        <VIcon size="22" color="primary" icon="$mdiTablePlus" />
      </template>
      {{ $t('createTable') }}
    </TooltipBtn>
    <VMenu location="bottom" content-class="with-shadow-border--none">
      <template #activator="{ props }">
        <TooltipBtn
          square
          size="small"
          variant="text"
          color="primary"
          data-test="export-btn"
          v-bind="props"
        >
          <template #btn-content>
            <VIcon size="20" icon="$mdiDownload" />
          </template>
          {{ $t('export') }}
        </TooltipBtn>
      </template>
      <VList>
        <VListItem v-for="opt in exportOptions" :key="opt.title" @click="opt.action">
          <VListItemTitle class="text-text"> {{ opt.title }} </VListItemTitle>
        </VListItem>
      </VList>
    </VMenu>
    <TooltipBtn
      square
      size="small"
      variant="text"
      color="primary"
      :disabled="!hasConnId"
      data-test="apply-btn"
      @click="emit('on-apply-script')"
    >
      <template #btn-content>
        <VIcon size="20" icon="mxs:running" />
      </template>
      {{ $t('applyScript') }}
      <br />
      <kbd>{{ OS_CMD }} + SHIFT + ENTER</kbd>
    </TooltipBtn>
    <VSpacer />
    <TooltipBtn
      square
      size="small"
      variant="text"
      :disabled="!hasConnId"
      :color="hasConnId ? 'primary' : ''"
      data-test="gen-erd-btn"
      @click="genErd()"
    >
      <template #btn-content>
        <VIcon size="20" icon="mxs:reports" />
      </template>
      {{ $t('genErd') }}
    </TooltipBtn>
    <ConnectionBtn
      class="rounded-0"
      :height="BTN_HEIGHT"
      :activeConn="conn"
      data-test="conn-btn"
      @click="openCnnDlg()"
    />
  </div>
</template>

<style lang="scss" scoped>
.er-toolbar-ctr {
  z-index: 4;
  .er-toolbar__separator {
    min-height: 28px;
    max-height: 28px;
  }
}
</style>
