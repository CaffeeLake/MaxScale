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
import { SNACKBAR_TYPE_MAP } from '@/constants'
import { getErrorsArr, delay } from '@/utils/helpers'
import { logger } from '@/plugins/logger'

const CANCEL_MESSAGE = 'canceled'
/**
 * Default handler for error response status codes
 */
export async function defErrStatusHandler({ store, error }) {
  store.commit('mxsApp/SET_SNACK_BAR_MESSAGE', {
    text: getErrorsArr(error),
    type: SNACKBAR_TYPE_MAP.ERROR,
  })
  /* When request is dispatched in a modal, an overlay_type loading will be set,
   * Turn it off before returning error
   */
  if (store.state.mxsApp.overlay_type !== null)
    await delay(600).then(() => store.commit('mxsApp/SET_OVERLAY_TYPE', null))
}

export function isCancelled(error) {
  return error.toString().includes(CANCEL_MESSAGE)
}

export function handleNullStatusCode({ store, error }) {
  if (isCancelled(error))
    // request is cancelled by user, so no response is received
    logger.info(error.toString())
  else
    store.commit('mxsApp/SET_SNACK_BAR_MESSAGE', {
      text: ['Lost connection to MaxScale, please check if MaxScale is running'],
      type: SNACKBAR_TYPE_MAP.ERROR,
    })
}
