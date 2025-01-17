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
import { MXS_OBJ_TYPE_MAP, ROUTING_TARGET_RELATIONSHIP_TYPES } from '@/constants'
import OverviewTbl from '@/components/dashboard/OverviewTbl.vue'

const store = useStore()
const { t } = useI18n()
const typy = useTypy()
const { ciStrIncludes } = useHelpers()

const HEADERS = [
  {
    title: 'Service',
    value: 'id',
    autoTruncate: true,
    cellProps: { class: 'px-0', style: { maxWidth: '150px' } },
    customRender: {
      renderer: 'AnchorLink',
      objType: MXS_OBJ_TYPE_MAP.SERVICES,
      props: { class: 'px-6' },
    },
  },
  {
    title: 'State',
    value: 'state',
    customRender: { renderer: 'StatusIcon', objType: MXS_OBJ_TYPE_MAP.SERVICES },
  },
  { title: 'Router', value: 'router', autoTruncate: true },
  { title: 'Current Sessions', value: 'connections', autoTruncate: true },
  { title: 'Total Sessions', value: 'total_connections', autoTruncate: true },
  {
    title: t('routingTargets'),
    value: 'routingTargets',
    autoTruncate: true,
    cellProps: { class: 'pa-0' },
    customRender: {
      renderer: 'RelationshipItems',
      objType: 'targets',
      mixTypes: true,
      props: { class: 'px-6' },
    },
    filter: (v, query) =>
      ciStrIncludes(typy(v).isArray ? v.map((v) => v.id).join(' ') : String(v), query),
  },
]

const allServices = computed(() => store.state.services.all_objs)
const totalMap = computed(() => ({ routingTargets: totalRoutingTargets.value }))

const items = computed(() => {
  const rows = []
  allServices.value.forEach((service) => {
    const {
      id,
      attributes: { state, router, connections, total_connections },
      relationships = {},
    } = service || {}

    const targets = Object.keys(relationships).reduce((arr, type) => {
      if (ROUTING_TARGET_RELATIONSHIP_TYPES.includes(type)) {
        arr = [...arr, ...typy(relationships[type], 'data').safeArray]
      }
      return arr
    }, [])
    const routingTargets = targets.length ? targets : t('noEntity', [t('routingTargets')])
    const row = { id, state, router, connections, total_connections, routingTargets }
    rows.push(row)
  })
  return rows
})

const totalRoutingTargets = useCountUniqueValues({
  data: items,
  field: 'routingTargets',
  subField: 'id',
})
</script>

<template>
  <OverviewTbl filter-mode="some" :headers="HEADERS" :data="items" :totalMap="totalMap" />
</template>
