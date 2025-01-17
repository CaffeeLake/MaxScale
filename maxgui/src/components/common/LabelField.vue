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
defineOptions({ inheritAttrs: false })
const props = defineProps({
  modelValue: { type: [String, Number] },
  label: { type: String, required: true },
  customErrMsg: { type: String, default: '' },
})
const emit = defineEmits(['update:modelValue'])
const attrs = useAttrs()

const {
  lodash: { uniqueId },
} = useHelpers()
const typy = useTypy()
const { t } = useI18n()

const input = computed({
  get: () => props.modelValue,
  set: (v) => emit('update:modelValue', v),
})
const id = computed(() => attrs.id || `label-field-${uniqueId()}`)
const isRequired = computed(() => attrs.required || attrs.required === '')

const customRules = computed(() => typy(attrs, 'rules').safeArray)

const rules = computed(() => {
  if (customRules.value.length) return customRules.value
  return [(v) => !!v || props.customErrMsg || t('errors.requiredField')]
})
</script>

<template>
  <label class="label-field text-small-text" :class="{ 'label--required': isRequired }" :for="id">
    {{ label }}
  </label>
  <VTextField
    v-model.trim="input"
    :id="id"
    hide-details="auto"
    :rules="isRequired ? rules : []"
    v-bind="$attrs"
  >
    <template v-for="(_, name) in $slots" #[name]="slotData">
      <slot :name="name" v-bind="slotData" />
    </template>
  </VTextField>
</template>
