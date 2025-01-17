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
const attrs = useAttrs()
const props = defineProps({
  modelValue: { type: [Array, Object], default: () => [] },
  initialValue: { type: [Array, Object], default: () => [] },
  type: { type: String, required: true },
  showPlaceHolder: { type: Boolean, default: true },
  required: { type: Boolean, default: false },
})
const emit = defineEmits(['update:modelValue', 'has-changed'])

const { lodash } = useHelpers()
const typy = useTypy()
const { t } = useI18n()

const hasChanged = computed(() => !lodash.isEqual(input.value, props.initialValue))
const isMultiple = computed(() => typy(attrs.multiple).isDefined)
const rules = computed(() => [
  (v) =>
    (v === null || v.length === 0) && props.required
      ? `${t(props.type, isMultiple.value ? 2 : 1)} is required`
      : true,
])

const input = computed({
  get: () => props.modelValue,
  set: (v) => emit('update:modelValue', v),
})

watch(hasChanged, (v) => emit('has-changed', v), { immediate: true })
watch(
  () => props.initialValue,
  (v) => {
    if (typy(v).safeArray.length || !typy(v).isEmptyObject) input.value = v
  },
  { deep: true, immediate: true }
)
</script>

<template>
  <VSelect
    v-model="input"
    item-title="id"
    :placeholder="showPlaceHolder ? $t('select', [$t(type, isMultiple ? 2 : 1)]) : ''"
    :no-data-text="$t('noEntityAvailable', [$t(type, isMultiple ? 2 : 1)])"
    :rules="rules"
    :hide-details="!props.required"
    return-object
    item-props
    validate-on="input"
  >
    <template v-if="isMultiple" #selection="{ item, index }">
      <span v-if="index === 0">
        {{ item.title }}
      </span>
      <span v-if="index === 1" class="text-caption text-grayed-out">
        (+{{ modelValue.length - 1 }} {{ $t('others', modelValue.length - 1) }})
      </span>
    </template>
  </VSelect>
</template>
