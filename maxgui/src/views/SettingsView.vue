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
const store = useStore()
const { t } = useI18n()
const {
  parameters: mxsParameters,
  fetch: fetchMaxScaleParameters,
  patch: patchParameters,
} = useMxsParams()
const fetchModuleParams = useFetchModuleParams()

const activeTab = ref(null)
const tabs = [t('maxScaleParameters')]

const module_parameters = computed(() => store.state.module_parameters)
const search_keyword = computed(() => store.state.search_keyword)

const paramsInfo = computed(() => {
  const parameters = module_parameters.value
  if (!parameters.length) return []
  // hard code type for child parameter of log_throttling
  const log_throttingIndex = parameters.findIndex((param) => param.name === 'log_throttling')
  const log_throttling = parameters[log_throttingIndex]
  if (!log_throttling) return []
  const log_throttling_child_params = [
    {
      name: 'count',
      type: 'count',
      modifiable: true,
      default_value: log_throttling.default_value.count,
      description: 'Positive integer specifying the number of logged times',
    },
    {
      name: 'suppress',
      type: 'duration',
      modifiable: true,
      unit: 'ms',
      default_value: log_throttling.default_value.suppress,
      description: 'The suppressed duration before the logging of a particular error',
    },
    {
      name: 'window',
      type: 'duration',
      modifiable: true,
      unit: 'ms',
      default_value: log_throttling.default_value.window,
      description: 'The duration that a particular error may be logged',
    },
  ]
  const left = parameters.slice(0, log_throttingIndex + 1)
  const right = parameters.slice(log_throttingIndex + 1)
  return [...left, ...log_throttling_child_params, ...right]
})

async function fetchParamsInfo() {
  await fetchModuleParams('maxscale')
}

async function fetchParams() {
  await fetchMaxScaleParameters()
}

async function updateParams(data) {
  await patchParameters({ data, callback: fetchParams })
}

watch(activeTab, async (v) => {
  if (v === t('maxScaleParameters')) await Promise.all([fetchParamsInfo(), fetchParams()])
})
</script>

<template>
  <ViewWrapper>
    <VSheet class="mt-2 fill-height">
      <portal to="view-header__right"><GlobalSearch /></portal>
      <VTabs v-model="activeTab">
        <VTab v-for="tab in tabs" :key="tab" :value="tab">{{ tab }} </VTab>
      </VTabs>
      <VWindow v-model="activeTab" class="fill-height">
        <VWindowItem v-for="tab in tabs" :key="tab" :value="tab" class="fill-height pt-5">
          <template v-if="tab === $t('maxScaleParameters')">
            <VCol cols="10" class="fill-height">
              <ParametersTable
                v-if="paramsInfo.length && !$typy(mxsParameters).isEmptyObject"
                :data="mxsParameters"
                :paramsInfo="paramsInfo"
                :confirmEdit="updateParams"
                :treeTableProps="{ search: search_keyword }"
              />
            </VCol>
          </template>
        </VWindowItem>
      </VWindow>
    </VSheet>
  </ViewWrapper>
</template>
