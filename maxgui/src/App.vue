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
import AppLayout from '@/layouts/AppLayout.vue'
import NoLayout from '@/layouts/NoLayout.vue'
import LoadingOverlay from '@/components/LoadingOverlay.vue'
import usersService from '@/services/usersService'
import { LOGO } from '@/constants'

const store = useStore()
const route = useRoute()
const logger = useLogger()

const is_session_alive = computed(() => store.state.mxsApp.is_session_alive)

const layouts = { AppLayout, NoLayout }
const layout = computed(() => layouts[route.meta.layout])

onBeforeMount(async () => {
  logger.info(LOGO)
  await usersService.authCheck()
})

onMounted(() => {
  const overlay = document.getElementById('global-overlay')
  if (overlay) {
    overlay.style.display = 'none'
  }
})

watch(
  () => route.path,
  () => store.commit('SET_SEARCH_KEYWORD', '')
)

watch(is_session_alive, async (v) => {
  if (!v) await usersService.logout()
})
</script>

<template>
  <VApp id="mxs-app">
    <LoadingOverlay />
    <component :is="layout" />
    <GblTooltip />
  </VApp>
</template>
