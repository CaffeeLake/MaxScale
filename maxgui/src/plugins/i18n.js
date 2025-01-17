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
import { createI18n } from 'vue-i18n'
import { en as vuetifyEn } from 'vuetify/locale'
import { t as typy } from 'typy'

function loadLocaleMessages() {
  const locales = import.meta.glob('@/locales/[A-Za-z0-9-_,\\s]+.json', { eager: true })
  const messages = {}
  Object.keys(locales).forEach((key) => {
    const matched = key.match(/([A-Za-z0-9-_]+)\./i)
    if (matched && matched.length > 1) {
      const locale = matched[1]
      messages[locale] = locales[key].default
    }
  })
  return messages
}
const i18n = createI18n({
  legacy: false,
  allowComposition: true,
  locale: import.meta.env.VITE_I18N_LOCALE || 'en',
  fallbackLocale: import.meta.env.VITE_I18N_FALLBACK_LOCALE || 'en',
  messages: { ...loadLocaleMessages(), $vuetify: vuetifyEn },
})

export const globalI18n = typy(i18n, 'global').safeObjectOrEmpty
export default {
  install: (app) => app.use(i18n),
}
