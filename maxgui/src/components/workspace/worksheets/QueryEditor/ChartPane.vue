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
import { CHART_TYPE_MAP, CHART_AXIS_TYPE_MAP } from '@/constants/workspace'
import { objectTooltip } from '@/components/common/Charts/customTooltips'

const props = defineProps({
  chartConfig: { type: Object, required: true },
  isMaximized: { type: Boolean, required: true },
  height: { type: Number, default: 0 },
})
const emit = defineEmits(['update:modelValue', 'update:isMaximized', 'close-chart'])
const {
  lodash: { uniqueId },
  dateFormat,
  exportToJpeg,
} = useHelpers()
const typy = useTypy()
const { LINE, SCATTER, BAR_VERT, BAR_HORIZ } = CHART_TYPE_MAP
const { CATEGORY, LINEAR, TIME } = CHART_AXIS_TYPE_MAP

const uniqueTooltipId = ref(uniqueId('tooltip_'))
const dataPoint = ref(null)
const chartToolHeight = ref(0)
const chartToolRef = ref(null)
const chartCtrRef = ref(null)
const chartRef = ref(null)

const isChartMaximized = computed({
  get: () => props.isMaximized,
  set: (v) => emit('update:isMaximized', v),
})

const tableData = computed(() => props.chartConfig.tableData)
const chartData = computed(() => props.chartConfig.chartData)
const labels = computed(() => chartData.value.labels)
const axisTypeMap = computed(() => props.chartConfig.axisTypeMap)
const axisKeyMap = computed(() => props.chartConfig.axisKeyMap)
const type = computed(() => props.chartConfig.type)
const hasTrendline = computed(() => props.chartConfig.hasTrendline)
const chartWidth = computed(() => {
  if (autoSkipTick(axisTypeMap.value.x)) return 'unset'
  return `${Math.min(labels.value.length * 15, 15000)}px`
})

const chartHeight = computed(() => {
  let height = props.height - (chartToolHeight.value + 12)
  if (!autoSkipTick(axisTypeMap.value.y)) {
    /** When there is too many data points,
     * first, get min value between "overflow" height (labels.length * 15)
     * and max height threshold 15000. However, when there is too little data points,
     * the "overflow" height is smaller than container height, container height
     * should be chosen to make chart fit to its container
     */
    height = Math.max(height, Math.min(labels.value.length * 15, 15000))
  }
  return `${height}px`
})
const chartStyle = computed(() => ({ height: chartHeight.value, minWidth: chartWidth.value }))
const chartOptions = computed(() => {
  const options = {
    layout: { padding: { left: 12, bottom: 12, right: 24, top: 24 } },
    animation: { active: { duration: 0 } },
    onHover: (e, el) => {
      e.native.target.style.cursor = el[0] ? 'pointer' : 'default'
    },
    scales: {
      x: {
        type: axisTypeMap.value.x,
        title: {
          display: true,
          text: axisKeyMap.value.x,
          font: { size: 14 },
          padding: { top: 16 },
          color: '#424f62',
        },
        beginAtZero: true,
        ticks: getAxisTicks({ axisId: 'x', axisType: axisTypeMap.value.x }),
      },
      y: {
        type: axisTypeMap.value.y,
        title: {
          display: true,
          text: axisKeyMap.value.y,
          font: { size: 14 },
          padding: { bottom: 16 },
          color: '#424f62',
        },
        beginAtZero: true,
        ticks: getAxisTicks({ axisId: 'y', axisType: axisTypeMap.value.y }),
      },
    },
    plugins: {
      tooltip: {
        callbacks: {
          label(context) {
            dataPoint.value = tableData.value[context.dataIndex]
          },
        },
        external: (context) =>
          objectTooltip({
            context,
            tooltipId: uniqueTooltipId.value,
            dataPoint: dataPoint.value,
            axisKeyMap: axisKeyMap.value,
            alignTooltipToLeft: context.tooltip.caretX >= chartCtrRef.value.clientWidth / 2,
          }),
      },
    },
  }
  if (props.chartConfig.isHorizChart) options.indexAxis = 'y'
  return options
})

watch(chartData, (v) => {
  if (!typy(v, 'datasets[0].data[0]').safeObject) removeTooltip()
})

watch(hasTrendline, (v) => {
  const dataset = typy(getChartInstance(), 'data.datasets[0]').safeObjectOrEmpty
  if (v)
    dataset.trendlineLinear = {
      colorMin: '#2d9cdb',
      colorMax: '#2d9cdb',
      lineStyle: 'solid',
      width: 2,
    }
  else delete dataset.trendlineLinear
})
onMounted(() =>
  nextTick(() => {
    chartToolHeight.value = chartToolRef.value.offsetHeight
  })
)
onBeforeUnmount(() => removeTooltip())

function getChartInstance() {
  return typy(chartRef.value, 'wrapper.chart').safeObjectOrEmpty
}

/**
 * Check if provided axisType is either LINEAR OR TIME type.
 * @param {String} param.axisType - CHART_AXIS_TYPE_MAP
 * @returns {Boolean} - should autoSkip the tick
 */
function autoSkipTick(axisType) {
  return axisType === LINEAR || axisType === TIME
}

/**
 * Get the ticks object
 * @param {String} param.axisType - CHART_AXIS_TYPE_MAP
 * @param {String} param.axisId- x or y
 * @returns {Object} - ticks object
 */
function getAxisTicks({ axisId, axisType }) {
  const autoSkip = autoSkipTick(axisTypeMap.value[axisType])
  const ticks = {
    autoSkip,
    callback: function (value) {
      // https://www.chartjs.org/docs/latest/axes/labelling.html#creating-custom-tick-formats
      const v = this.getLabelForValue(value)
      if (typy(v).isString && v.length > 10) return `${v.substr(0, 10)}...`
      return v
    },
  }
  if (autoSkip) ticks.autoSkipPadding = 15
  // only rotate tick label for the X axis and CATEGORY axis type
  if (axisId === 'x' && axisType === CATEGORY) {
    ticks.maxRotation = 90
    ticks.minRotation = 90
  }
  return ticks
}

function removeTooltip() {
  const tooltipEl = document.getElementById(uniqueTooltipId.value)
  if (tooltipEl) tooltipEl.remove()
}

function getDefFileName() {
  return `MaxScale ${type.value} Chart - ${dateFormat({
    value: new Date(),
  })}`
}

function exportChart() {
  exportToJpeg({ canvas: chartRef.value.$el, fileName: getDefFileName() })
}
</script>

<template>
  <div class="chart-pane d-flex flex-column fill-height">
    <div ref="chartToolRef" class="d-flex pt-2 pr-3">
      <VSpacer />
      <TooltipBtn
        icon
        variant="text"
        color="primary"
        density="compact"
        data-test="export-btn"
        @click="exportChart"
      >
        <template #btn-content>
          <VIcon size="16" icon="$mdiDownload" />
        </template>
        {{ $t('exportChart') }}
      </TooltipBtn>
      <TooltipBtn
        icon
        variant="text"
        color="primary"
        density="compact"
        data-test="maximize-toggle-btn"
        @click="isChartMaximized = !isChartMaximized"
      >
        <template #btn-content>
          <VIcon size="20" :icon="`$mdiFullscreen${isChartMaximized ? 'Exit' : ''}`" />
        </template>
        {{ isChartMaximized ? $t('minimize') : $t('maximize') }}
      </TooltipBtn>
      <TooltipBtn
        class="close-chart"
        icon
        variant="text"
        color="primary"
        density="compact"
        data-test="close-btn"
        @click="$emit('close-chart')"
      >
        <template #btn-content>
          <VIcon size="11" icon="mxs:close" />
        </template>
        {{ $t('close') }}
      </TooltipBtn>
    </div>
    <div ref="chartCtrRef" class="w-100 overflow-auto fill-height">
      <div class="canvas-container" :style="chartStyle">
        <LineChart
          v-if="type === LINE"
          ref="chartRef"
          hasVertCrossHair
          :data="chartData"
          :opts="chartOptions"
        />
        <ScatterChart
          v-else-if="type === SCATTER"
          ref="chartRef"
          :data="chartData"
          :opts="chartOptions"
        />
        <BarChart
          v-else-if="type === BAR_VERT || type === BAR_HORIZ"
          ref="chartRef"
          :data="chartData"
          :opts="chartOptions"
        />
      </div>
    </div>
  </div>
</template>
