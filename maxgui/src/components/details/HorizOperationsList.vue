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
import IconGroupWrapper from '@/components/details/IconGroupWrapper.vue'
defineProps({
  data: {
    type: Array,
    default: () => [],
    validator: (v) => (v && Array.isArray(v) ? v.every(Array.isArray) : true),
  },
  handler: { type: Function, default: () => null },
})
</script>

<template>
  <IconGroupWrapper v-for="(operations, i) in data" :key="i" :multiIcons="operations.length > 1">
    <template #default="{ props }">
      <TooltipBtn
        v-for="op in operations"
        :key="op.title"
        :tooltipProps="{ location: 'bottom' }"
        variant="text"
        :disabled="op.disabled"
        v-bind="props"
        @click="handler(op)"
      >
        <template #btn-content>
          <VIcon
            v-if="op.icon"
            :color="op.disabled ? '' : op.color"
            :size="op.iconSize"
            :icon="op.icon"
          />
        </template>
        {{ op.title }}
      </TooltipBtn>
    </template>
  </IconGroupWrapper>
</template>
