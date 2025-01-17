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
const props = defineProps({
  modelValue: { type: Array, required: true },
  label: { type: String, default: '' },
  items: { type: Array, required: true }, // array of strings
  maxHeight: { type: [Number, String], default: 'unset' },
  maxWidth: { type: Number, default: 220 },
  activatorClass: { type: String, default: '' },
  returnIndex: { type: Boolean, default: false },
  changeColorOnActive: { type: Boolean, default: false },
  activatorProps: {
    type: Object,
    default: () => ({ xSmall: true, outlined: true, color: 'primary' }),
  },
  reverse: { type: Boolean, default: false }, // reverse the logic, modelValue contains unselected items
  hideSelectAll: { type: Boolean, default: false },
  hideSearch: { type: Boolean, default: false },
  hideFilterIcon: { type: Boolean, default: false },
  multiple: { type: Boolean, default: true },
})
const emit = defineEmits(['update:modelValue'])

const { ciStrIncludes, lodash, immutableUpdate } = useHelpers()
const typy = useTypy()

const filterTxt = ref('')
const isOpened = ref(false)

const itemsList = computed(() =>
  props.items.filter((str) => ciStrIncludes(`${str}`, filterTxt.value))
)

const isAllSelected = computed(() =>
  props.reverse ? props.modelValue.length === 0 : props.modelValue.length === props.items.length
)

const indeterminate = computed(() => {
  if (props.reverse) return !(props.modelValue.length === props.items.length || isAllSelected.value)
  return !(props.modelValue.length === 0 || isAllSelected.value)
})

const activatorClasses = computed(() => {
  const classes = [props.activatorClass, 'text-capitalize px-3']
  if (props.changeColorOnActive) {
    classes.push('change-color-btn')
    if (isOpened.value) classes.push('change-color-btn--active text-primary border-color--primary')
  }
  return classes
})

const btnProps = computed(() => lodash.pickBy(props.activatorProps, (v, key) => key !== 'color'))

const activatorColor = computed(() => typy(props.activatorProps, 'color').safeString || 'primary')

function selectAll() {
  emit(
    'update:modelValue',
    props.returnIndex ? props.items.map((_, i) => i) : lodash.cloneDeep(props.items)
  )
}

function deselectAll() {
  emit('update:modelValue', [])
}
function toggleAll(v) {
  props.reverse === v ? deselectAll() : selectAll()
}
function deselectItem({ item, index }) {
  emit(
    'update:modelValue',
    immutableUpdate(props.modelValue, {
      $splice: [[props.modelValue.indexOf(props.returnIndex ? index : item), 1]],
    })
  )
}

function selectItem({ item, index }) {
  emit(
    'update:modelValue',
    immutableUpdate(props.modelValue, {
      [props.multiple ? `$push` : '$set']: [props.returnIndex ? index : item],
    })
  )
}

function toggleItem({ v, item, index }) {
  props.reverse === v ? deselectItem({ item, index }) : selectItem({ item, index })
}
</script>

<template>
  <VMenu
    v-model="isOpened"
    transition="slide-y-transition"
    content-class="full-border filter-list"
    :close-on-content-click="false"
    offset="-1px"
  >
    <template #activator="{ props, isActive }">
      <slot name="activator" :data="{ props, isActive, label }">
        <VBtn
          variant="outlined"
          :class="activatorClasses"
          :color="changeColorOnActive ? 'unset' : activatorColor"
          v-bind="{ ...props, ...btnProps }"
        >
          <VIcon v-if="!hideFilterIcon" size="12" class="mr-1" icon="mxs:filter" />
          {{ label }}
          <template #append>
            <VIcon
              :size="10"
              :class="[isActive ? 'rotate-up' : 'rotate-down']"
              icon="mxs:menuDown"
            />
          </template>
        </VBtn>
      </slot>
    </template>
    <VList :max-width="maxWidth" :max-height="maxHeight">
      <template v-if="!hideSearch">
        <VListItem class="py-0 px-0">
          <VTextField
            v-model="filterTxt"
            class="filter-list__search"
            :placeholder="$t('search')"
            hide-details
            tile
            variant="plain"
            density="compact"
          />
        </VListItem>
        <VDivider />
      </template>
      <template v-if="!hideSelectAll">
        <VListItem class="py-0 px-2" link>
          <VCheckboxBtn
            :modelValue="isAllSelected"
            density="compact"
            class="filter-list__checkbox"
            :label="$t('selectAll')"
            :indeterminate="indeterminate"
            @update:modelValue="toggleAll"
          />
        </VListItem>
        <v-divider />
      </template>
      <VListItem v-for="(item, index) in itemsList" :key="`${index}`" class="py-0 px-2" link>
        <VCheckboxBtn
          :modelValue="
            reverse
              ? !modelValue.includes(returnIndex ? index : item)
              : modelValue.includes(returnIndex ? index : item)
          "
          density="compact"
          class="filter-list__checkbox"
          @update:modelValue="toggleItem({ v: $event, item, index })"
        >
          <template #label>
            <div class="w-100 fill-height pos--relative">
              <GblTooltipActivator
                v-mxs-highlighter="{ keyword: filterTxt, txt: item }"
                activateOnTruncation
                :data="{ txt: String(item) }"
                :max-width="maxWidth - 52"
                fillHeight
              />
            </div>
          </template>
        </VCheckboxBtn>
      </VListItem>
    </VList>
  </VMenu>
</template>

<style lang="scss" scoped>
.change-color-btn {
  border-color: colors.$text-subtle;
  color: colors.$navigation;
  &:focus::before {
    opacity: 0;
  }
  .v-btn__append .v-icon {
    color: rgba(0, 0, 0, 0.54);
  }
  &--active {
    .v-btn__append .v-icon {
      color: inherit;
    }
  }
}
.filter-list__checkbox :deep(label) {
  width: 100%;
  font-size: 0.875rem !important;
  color: colors.$navigation !important;
}
</style>
