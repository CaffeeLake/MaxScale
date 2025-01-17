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
import QueryResultTabWrapper from '@wkeComps/QueryEditor/QueryResultTabWrapper.vue'
import DataTable from '@wkeComps/QueryEditor/DataTable.vue'
import workspace from '@/composables/workspace'

const props = defineProps({
  data: { type: Object, required: true },
  dataTableProps: { type: Object, default: () => ({}) },
  dim: { type: Object, required: true },
  reload: { type: Function },
})

const typy = useTypy()

const queryData = computed(() => props.data)
const hasRes = computed(() => typy(queryData.value, 'data.attributes.sql').isDefined)
const resultSet = computed(
  () => typy(queryData.value, 'data.attributes.results[0]').safeObjectOrEmpty
)
const statement = computed(() => typy(resultSet.value, 'statement').safeObject)

const { isLoading, startTime, execTime, endTime } = workspace.useCommonResSetAttrs(queryData)
</script>

<template>
  <QueryResultTabWrapper
    :dim="dim"
    :isLoading="isLoading"
    :showFooter="isLoading || hasRes"
    :resInfoBarProps="{ result: resultSet, startTime, execTime, endTime }"
  >
    <template #default="{ tblDim }">
      <DataTable
        :data="resultSet"
        :height="tblDim.height"
        :width="tblDim.width"
        :toolbarProps="{ statement, onReload: reload }"
        v-bind="dataTableProps"
      >
        <template v-for="(_, name) in $slots" #[name]="slotData">
          <slot :name="name" v-bind="slotData" />
        </template>
      </DataTable>
    </template>
  </QueryResultTabWrapper>
</template>
