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
import SelDlg from '@/components/details/SelDlg.vue'
import RoutingTargetDlg from '@/components/service/RoutingTargetDlg.vue'
import Sortable from 'sortablejs'

const props = defineProps({
  type: { type: String, required: true },
  data: { type: Array, required: true },
  removable: { type: Boolean, default: false },
  addable: { type: Boolean, default: false },
  customAddableItems: { type: Array },
  getRelationshipData: { type: Function },
  objId: { type: String, default: '' },
})
const emit = defineEmits(['confirm-update', 'click-add-listener', 'confirm-update-relationships'])

const { t } = useI18n()
const store = useStore()
const typy = useTypy()
const loading = useLoading()
const {
  lodash: { cloneDeep, xorWith, groupBy },
} = useHelpers()

const DEFAULT_HEADERS = [
  {
    title: t(props.type, 1),
    value: 'id',
    autoTruncate: true,
    cellProps: { class: 'pa-0' },
    customRender: {
      renderer: 'AnchorLink',
      objType: props.type,
      props: { class: 'px-6' },
    },
  },
  {
    title: 'Status',
    value: 'state',
    cellProps: { align: 'center' },
    headerProps: { align: 'center' },
    customRender: { renderer: 'StatusIcon', objType: props.type },
  },
]

const ACTION_HEADER = {
  title: '',
  value: 'action',
  width: '1px',
  cellProps: { class: 'pl-0 pr-3' },
  headerProps: { class: 'pl-0 pr-3' },
}

const vSortable = {
  beforeMount: (el) => {
    const options = {
      handle: '.drag-handle',
      draggable: '.draggable-row',
      animation: 200,
      onEnd: filterDragReorder,
    }
    Sortable.create(el.getElementsByTagName('tbody')[0], options)
  },
}

const targetItems = ref([])
const addableItems = ref([])
const isConfDlgOpened = ref(false)
const isSelDlgOpened = ref(false)
const isRoutingTargetDlgOpened = ref(false)

const headers = computed(() => {
  let values = DEFAULT_HEADERS
  switch (props.type) {
    case MXS_OBJ_TYPE_MAP.FILTERS:
      values = [
        {
          title: '',
          value: 'index',
          width: '1px',
          headerProps: { class: 'px-2' },
          cellProps: {
            class: 'px-2 border-right--table-border text-grayed-out',
            style: { fontSize: '10px' },
          },
        },
        { ...DEFAULT_HEADERS[0], width: '85%' },
      ]
      break
    case 'routingTargets':
      values = [
        {
          title: 'id',
          value: 'state',
          cellProps: { class: 'pr-0' },
          headerProps: { class: 'pr-0' },
          customRender: { renderer: 'StatusIcon', objType: props.type },
        },
        {
          title: '',
          value: 'id',
          autoTruncate: true,
          cellProps: { class: 'pl-0' },
          customRender: {
            renderer: 'AnchorLink',
            objType: props.type,
            props: { class: 'pr-6' },
          },
          width: '85%',
        },
        { title: 'type', value: 'type' },
      ]
      break
  }
  if (props.removable) values.push(ACTION_HEADER)
  return values
})
const search_keyword = computed(() => store.state.search_keyword)
const isAdmin = computed(() => store.getters['users/isAdmin'])
const items = computed(() =>
  props.type === MXS_OBJ_TYPE_MAP.FILTERS
    ? cloneDeep(props.data).map((row, i) => ({ ...row, index: i }))
    : props.data
)
const addBtnText = computed(
  () =>
    `${t('addEntity', {
      entityName: t(props.type, props.type === MXS_OBJ_TYPE_MAP.LISTENERS ? 1 : 2),
    })}`
)
const isFilterType = computed(() => props.type === MXS_OBJ_TYPE_MAP.FILTERS)
const isRoutingTargetType = computed(() => props.type === 'routingTargets')

const initialRelationshipItems = computed(() => formRelationshipData(items.value))
const initialTypeGroups = computed(() => groupBy(initialRelationshipItems.value, 'type'))
const itemToBeUnlinked = computed(() => typy(targetItems, '[0]').safeObjectOrEmpty)

function onDelete(item) {
  targetItems.value = [item]
  isConfDlgOpened.value = true
}

function confirmDelete() {
  const map = initialTypeGroups.value
  const type = typy(targetItems.value, '[0].type').safeString
  emit('confirm-update', {
    type,
    data: xorWith(map[type], targetItems.value, (a, b) => a.id === b.id),
    isRoutingTargetType: isRoutingTargetType.value,
  })
}

async function getAllEntities() {
  if (props.customAddableItems) {
    await props.getRelationshipData({ type: props.type })
    addableItems.value = props.customAddableItems
  } else {
    const all = await props.getRelationshipData({ type: props.type })
    const availableEntities = xorWith(all, items.value, (a, b) => a.id === b.id)
    addableItems.value = formRelationshipData(availableEntities)
  }
}

function onClickBtn() {
  if (isRoutingTargetType.value) isRoutingTargetDlgOpened.value = true
  else if (props.type !== MXS_OBJ_TYPE_MAP.LISTENERS) isSelDlgOpened.value = true
  else emit('click-add-listener')
}

/**
 * @param {Array} arr - array of object, each object must have id and type attributes
 * @returns {Array} - returns valid relationship array data
 */
function formRelationshipData(arr) {
  return arr.map((item) => ({ id: item.id, type: item.type }))
}

function confirmAdd() {
  emit('confirm-update', {
    type: props.type,
    data: [...formRelationshipData(items.value), ...formRelationshipData(targetItems.value)],
  })
}

function filterDragReorder({ oldIndex, newIndex }) {
  if (oldIndex !== newIndex) {
    const data = cloneDeep(items.value)
    const temp = data[oldIndex]
    data[oldIndex] = data[newIndex]
    data[newIndex] = temp
    emit('confirm-update', { type: props.type, data: formRelationshipData(data) })
  }
}

function confirmEditRoutingTarget(changedMap) {
  emit('confirm-update-relationships', changedMap)
}
</script>

<template>
  <CollapsibleCtr :title="`${$t(type, 2)}`" :titleInfo="items.length">
    <template #header-right>
      <VBtn
        v-if="isAdmin && addable"
        color="primary"
        variant="text"
        size="small"
        class="text-capitalize"
        @click="onClickBtn"
      >
        <template v-if="isRoutingTargetType">
          <vIcon color="primary" size="12" icon="mxs:edit" class="mr-1" />
          {{ t('edit') }}
        </template>
        <template v-else>+ {{ addBtnText }}</template>
      </VBtn>
    </template>
    <VDataTable
      class="relationship-tbl"
      :search="search_keyword"
      :headers="headers"
      :items="items"
      :noDataText="$t('noEntity', [$t(type, 2)])"
      :loading="loading"
      hide-default-footer
      v-sortable
    >
      <template #item="{ item, columns }">
        <tr class="v-data-table__tr pos--relative" :class="{ 'draggable-row': isFilterType }">
          <CustomTblCol
            v-for="(h, i) in columns"
            :key="h.value"
            :value="item[h.value]"
            :name="h.value"
            :search="search_keyword"
            :autoTruncate="h.autoTruncate"
            :class="{
              'text-no-wrap': $typy(h.customRender, 'renderer').safeString === 'StatusIcon',
            }"
            v-bind="columns[i].cellProps"
          >
            <template v-if="h.customRender" #[`item.${h.value}`]="{ value, highlighter }">
              <CustomCellRenderer
                :value="value"
                :componentName="h.customRender.renderer"
                :objType="isRoutingTargetType ? item.type : h.customRender.objType"
                :mixTypes="false"
                :highlighter="highlighter"
                statusIconWithoutLabel
                v-bind="h.customRender.props"
              />
            </template>
            <template #[`item.action`]>
              <VIcon
                v-if="isFilterType && isAdmin"
                class="drag-handle cursor--move text-grayed-out pos--absolute"
                size="16"
                icon="$mdiDragHorizontalVariant"
              />
              <div class="del-btn" v-if="isAdmin">
                <TooltipBtn
                  density="comfortable"
                  icon
                  variant="text"
                  color="error"
                  @click="onDelete(item)"
                >
                  <template #btn-content>
                    <VIcon size="18" icon="mxs:unlink" :style="{ transition: 'none' }" />
                  </template>
                  {{ `${$t('unlink')} ${$t(isRoutingTargetType ? item.type : type, 1)}` }}
                </TooltipBtn>
              </div>
            </template>
          </CustomTblCol>
        </tr>
      </template>
    </VDataTable>
    <RoutingTargetDlg
      v-if="isRoutingTargetType"
      v-model="isRoutingTargetDlgOpened"
      :serviceId="objId"
      :initialRelationshipItems="initialRelationshipItems"
      :initialTypeGroups="initialTypeGroups"
      :data="items"
      :onSave="confirmEditRoutingTarget"
    />
    <ConfirmDlg
      v-if="removable"
      v-model="isConfDlgOpened"
      :title="`${t('unlink')} ${t(type, 1)}`"
      saveText="unlink"
      type="unlink"
      :item="itemToBeUnlinked"
      :onSave="confirmDelete"
    />
    <SelDlg
      v-if="addable"
      v-model="isSelDlgOpened"
      :title="t('addEntity', { entityName: t(type, 2) })"
      saveText="add"
      :onSave="confirmAdd"
      :type="type"
      multiple
      :items="addableItems"
      @selected-items="targetItems = $event"
      @on-open="getAllEntities"
    />
  </CollapsibleCtr>
</template>

<style lang="scss" scoped>
.relationship-tbl {
  .draggable-row:hover {
    background: transparent !important;
  }

  .sortable-chosen:hover {
    background: colors.$tr-hovered-color !important;
    .drag-handle {
      display: inline;
    }
  }
  .sortable-ghost {
    background: colors.$tr-hovered-color !important;
    opacity: 0.6;
  }
  tbody tr {
    .del-btn {
      visibility: hidden;
    }
    .drag-handle {
      top: 10px;
      left: 50%;
      transform: translate(-50%, -50%);
      visibility: hidden;
    }
  }
  tbody tr:hover {
    .del-btn,
    .drag-handle {
      visibility: visible;
    }
  }
}
</style>
