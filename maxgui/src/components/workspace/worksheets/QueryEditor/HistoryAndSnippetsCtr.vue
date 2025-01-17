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
import QueryResult from '@wsModels/QueryResult'
import QueryResultTabWrapper from '@wkeComps/QueryEditor/QueryResultTabWrapper.vue'
import DataTable from '@wkeComps/QueryEditor/DataTable.vue'
import EditableCell from '@wkeComps/QueryEditor/EditableCell.vue'
import {
  QUERY_MODE_MAP,
  NODE_CTX_TYPE_MAP,
  QUERY_LOG_TYPE_MAP,
  OS_CMD,
} from '@/constants/workspace'

const props = defineProps({
  dim: { type: Object, required: true },
  queryMode: { type: String, required: true },
  queryTabId: { type: String, required: true },
  dataTableProps: { type: Object, required: true },
})

const { HISTORY, SNIPPETS } = QUERY_MODE_MAP
const { CLIPBOARD, INSERT } = NODE_CTX_TYPE_MAP
const store = useStore()
const { t } = useI18n()
const typy = useTypy()
const {
  map2dArr,
  copyTextToClipboard,
  dateFormat,
  lodash: { cloneDeep, xorWith, isEqual },
} = useHelpers()

const LOG_TYPES = Object.values(QUERY_LOG_TYPE_MAP)
const DATE_FORMAT_TYPE = 'E, dd MMM yyyy'
const TABS = [
  { id: HISTORY, label: t('history') },
  { id: SNIPPETS, label: t('snippets') },
]
const TAB_NAV_HEIGHT = 20
const MENU_OPTS = [
  {
    title: t('copyToClipboard'),
    children: [{ title: 'SQL', type: CLIPBOARD, action: txtOptHandler }],
  },
  {
    title: t('placeToEditor'),
    children: [{ title: 'SQL', type: INSERT, action: txtOptHandler }],
  },
]

const selectedItems = ref([])
const logTypesToShow = ref([])
const isConfDlgOpened = ref(false)
const actionCellData = ref(null)
const tableHeaders = ref([])
// states for editing table cell
const isEditing = ref(false)
const changedCells = ref([]) // cells have their value changed

const query_history = computed(() => store.state.prefAndStorage.query_history)
const query_snippets = computed(() => store.state.prefAndStorage.query_snippets)
const activeMode = computed({
  get: () => props.queryMode,
  set: (v) => QueryResult.update({ where: props.queryTabId, data: { query_mode: v } }),
})
const persistedQueryData = computed(() => {
  switch (activeMode.value) {
    case HISTORY:
      return query_history.value
    case SNIPPETS:
      return query_snippets.value
    default:
      return []
  }
})
const fields = computed(() => Object.keys(typy(persistedQueryData.value, '[0]').safeObjectOrEmpty))
const headers = computed(() =>
  fields.value.map((field) => {
    const header = {
      text: field,
      capitalize: true,
    }
    // assign default width to each column to have better view
    switch (field) {
      case 'date':
        header.width = 150
        header.useCellSlot = true
        header.dateFormatType = DATE_FORMAT_TYPE
        break
      case 'connection_name':
        header.width = 215
        break
      case 'time':
        header.width = 90
        break
      case 'action':
        header.useCellSlot = true
        header.valuePath = 'name'
        break
      // Fields for SNIPPETS
      case 'name':
        header.width = 240
        header.useCellSlot = isEditing.value
        break
      case 'sql':
        header.useCellSlot = isEditing.value
    }
    return header
  })
)
// result-data-table auto adds an order number header, so plus 1
const idxOfDateCol = computed(() => fields.value.findIndex((h) => h === 'date') + 1)
const rows = computed(() => persistedQueryData.value.map((item) => Object.values(item)))
const currRows = computed(() => {
  let data = persistedQueryData.value
  if (
    activeMode.value === HISTORY &&
    logTypesToShow.value.length &&
    logTypesToShow.value.length < LOG_TYPES.length
  )
    data = data.filter((log) => logTypesToShow.value.includes(log.action.type))
  return data.map((item) => Object.values(item))
})
const tableData = computed(() => {
  if (persistedQueryData.value.length) return { data: currRows.value, fields: fields.value }
  return {}
})
const editableFields = computed(() => fields.value.filter((f) => f === 'name' || f === 'sql'))
const defExportFileName = computed(
  () => `MaxScale Query ${activeMode.value === HISTORY ? 'History' : 'Snippets'}`
)
const confDlgTitle = computed(() =>
  activeMode.value === HISTORY
    ? t('clearSelectedQueries', { targetType: t('queryHistory') })
    : t('deleteSnippets')
)

function onDelete() {
  isConfDlgOpened.value = true
}

function deleteSelectedRows() {
  const targetMatrices = cloneDeep(selectedItems.value).map(
    (row) => row.filter((_, i) => i !== 0) // Remove # col
  )
  const newMaxtrices = xorWith(rows.value, targetMatrices, isEqual)
  // Convert to array of objects
  const newData = map2dArr({ fields: fields.value, arr: newMaxtrices })
  store.commit(`prefAndStorage/SET_QUERY_${activeMode.value}`, newData)
  selectedItems.value = []
}

function txtOptHandler({ opt, data }) {
  const rowData = map2dArr({
    fields: fields.value,
    arr: [data.row.filter((_, i) => i !== 0)], // Remove # col
  })
  let sql, name
  switch (activeMode.value) {
    case HISTORY: {
      name = rowData[0].action.name
      sql = rowData[0].action.sql
      break
    }
    case SNIPPETS:
      sql = rowData[0].sql
  }
  const { INSERT, CLIPBOARD } = NODE_CTX_TYPE_MAP
  // if no name is defined when storing the query, sql query is stored to name
  const sqlTxt = sql ? sql : name
  switch (opt.type) {
    case CLIPBOARD:
      copyTextToClipboard(sqlTxt)
      break
    case INSERT:
      typy(props.dataTableProps, 'placeToEditor').safeFunction(sqlTxt)
      break
  }
}

function formatDate(cell) {
  return dateFormat({
    value: cell,
    formatType: DATE_FORMAT_TYPE,
  })
}

function toggleEditMode() {
  isEditing.value = !isEditing.value
}

function confirmEditSnippets() {
  const cells = cloneDeep(changedCells.value)
  const snippets = cloneDeep(query_snippets.value)
  cells.forEach((c) => {
    delete c.objRow['#'] // Remove # col
    const idxOfRow = query_snippets.value.findIndex((item) => isEqual(item, c.objRow))
    if (idxOfRow > -1) snippets[idxOfRow] = { ...snippets[idxOfRow], [c.colName]: c.value }
  })
  store.commit('prefAndStorage/SET_QUERY_SNIPPETS', snippets)
  toggleEditMode()
}

function toCellData({ rowData, cell, colName }) {
  const objRow = tableHeaders.value.reduce((o, c, i) => ((o[c.text] = rowData[i]), o), {})
  const rowId = objRow['#'] // Using # col as unique row id as its value isn't alterable
  return { id: `rowId-${rowId}_colName-${colName}`, rowId, colName, value: cell, objRow }
}

function onChangeCell({ item, hasChanged }) {
  const targetIndex = changedCells.value.findIndex((c) => c.id === item.id)
  if (hasChanged) {
    if (targetIndex === -1) changedCells.value.push(item)
    else changedCells.value[targetIndex] = item
  } else if (targetIndex > -1) changedCells.value.splice(targetIndex, 1)
}
</script>

<template>
  <QueryResultTabWrapper :dim="dim" :showFooter="false">
    <template #default="{ tblDim }">
      <KeepAlive>
        <DataTable
          :key="activeMode"
          v-model:selectedItems="selectedItems"
          :customHeaders="headers"
          :data="tableData"
          :height="tblDim.height"
          :width="tblDim.width"
          :groupByColIdx="idxOfDateCol"
          :draggableCell="!isEditing"
          :menuOpts="MENU_OPTS"
          showSelect
          :toolbarProps="{
            customFilterActive: Boolean(logTypesToShow.length),
            defExportFileName,
            exportAsSQL: false,
            onDelete,
          }"
          v-bind="dataTableProps"
          @get-table-headers="tableHeaders = $event"
        >
          <template #toolbar-left-append>
            <VTabs
              v-model="activeMode"
              hide-slider
              :height="TAB_NAV_HEIGHT"
              class="d-inline-flex workspace-tab-style"
            >
              <VTab
                v-for="tab in TABS"
                :key="tab.id"
                :value="tab.id"
                class="px-3 text-uppercase border--table-border"
                selectedClass="v-tab--selected font-weight-medium"
              >
                {{ tab.label }}
              </VTab>
            </VTabs>
          </template>
          <template v-if="activeMode === HISTORY" #filter-menu-content-append>
            <FilterList
              v-model="logTypesToShow"
              :label="$t('logTypes')"
              :items="LOG_TYPES"
              :maxHeight="200"
              hideSelectAll
              hideSearch
              :activatorProps="{ density: 'default', size: 'small' }"
            />
          </template>
          <template #toolbar-right-prepend="{ showBtn }">
            <TooltipBtn
              v-if="showBtn && activeMode === SNIPPETS"
              square
              variant="text"
              size="small"
              color="primary"
              @click="isEditing ? confirmEditSnippets() : toggleEditMode()"
            >
              <template #btn-content>
                <VIcon
                  :size="isEditing ? 16 : 14"
                  :icon="isEditing ? '$mdiCheckOutline' : 'mxs:edit'"
                />
              </template>
              {{ isEditing ? $t('doneEditing') : $t('edit') }}
            </TooltipBtn>
          </template>
          <template #header-connection_name="{ data: { maxWidth, activatorID } }">
            <GblTooltipActivator
              :data="{ txt: 'Connection Name', activatorID }"
              activateOnTruncation
              :maxWidth="maxWidth"
            />
          </template>
          <template v-if="activeMode === SNIPPETS" #header-name>
            {{ $t('prefix') }}
          </template>
          <template #date="{ on, highlighterData, data: { cell } }">
            <span
              v-mxs-highlighter="{ ...highlighterData, txt: formatDate(cell) }"
              class="text-truncate"
              v-on="on"
            >
              {{ formatDate(cell) }}
            </span>
          </template>
          <template #action="{ on, highlighterData, data: { cell }, activatorID }">
            <div
              v-mxs-highlighter="{ ...highlighterData, txt: cell.name }"
              class="text-truncate"
              @mouseover="actionCellData = { data: cell, activatorID }"
              v-on="on"
            >
              {{ cell.name }}
            </div>
          </template>
          <!-- TODO: Redesign the edit feature, so that editable cols will be rendered in a dialog. The 
          SqlEditor should be used for editing sql text' -->
          <template
            v-for="field in editableFields"
            #[field]="props"
            :key="`${field}-${props.data.cell}`"
          >
            <EditableCell
              v-if="isEditing"
              :data="
                toCellData({ rowData: props.data.rowData, cell: props.data.cell, colName: field })
              "
              @on-change="onChangeCell"
            />
          </template>
          <template #result-msg-append>
            <i18n-t
              :keypath="activeMode === HISTORY ? 'historyTabGuide' : 'snippetTabGuide'"
              class="d-flex align-center pt-2"
              tag="span"
              scope="global"
            >
              <template #shortcut>
                &nbsp;<b>{{ OS_CMD }} + S</b>&nbsp;
              </template>
              <template #icon>
                &nbsp;
                <VIcon color="primary" size="16" icon="$mdiStarPlusOutline" />
                &nbsp;
              </template>
            </i18n-t>
          </template>
        </DataTable>
      </KeepAlive>
      <BaseDlg
        v-model="isConfDlgOpened"
        :title="confDlgTitle"
        saveText="delete"
        minBodyWidth="624px"
        :onSave="deleteSelectedRows"
      >
        <template #form-body>
          <p class="mb-4">
            {{
              $t('info.clearSelectedQueries', {
                quantity: selectedItems.length === rows.length ? $t('entire') : $t('selected'),
                targetType: $t(activeMode === HISTORY ? 'queryHistory' : 'snippets'),
              })
            }}
          </p>
        </template>
      </BaseDlg>
      <VTooltip
        v-if="actionCellData"
        location="top"
        :activator="`#${$typy(actionCellData, 'activatorID').safeString}`"
      >
        <table class="action-table-tooltip px-1">
          <caption class="text-left font-weight-bold mb-3 pl-1">
            {{
              $t('queryResInfo')
            }}
            <VDivider class="border-color--separator" />
          </caption>
          <tr v-for="(value, key) in actionCellData.data" :key="`${key}`">
            <template v-if="key !== 'type'">
              <td>{{ key }}</td>
              <td
                :class="{ 'text-truncate': key !== 'response' }"
                :style="{
                  maxWidth: '600px',
                  whiteSpace: key !== 'response' ? 'nowrap' : 'pre-line',
                }"
              >
                {{ value }}
              </td>
            </template>
          </tr>
        </table>
      </VTooltip>
    </template>
  </QueryResultTabWrapper>
</template>

<style lang="scss" scoped>
.action-table-tooltip {
  border-spacing: 0;
  td {
    font-size: 0.875rem;
    height: 24px;
    vertical-align: middle;
    &:first-of-type {
      padding-right: 16px;
    }
  }
}
</style>
