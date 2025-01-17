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

import mount from '@/tests/mount'
import CollapsibleCtr from '@/components/common/CollapsibleCtr.vue'

describe('CollapsibleCtr.vue', () => {
  let wrapper
  beforeEach(() => {
    wrapper = mount(CollapsibleCtr, {
      shallow: false,
      props: { title: 'CollapsibleCtr title' },
    })
  })

  it('Should hide content when the toggle button is clicked', async () => {
    await wrapper.find('[data-test="toggle-btn"]').trigger('click')
    expect(wrapper.find('[data-test="content"]').attributes().style).toBe('display: none;')
  })

  const slots = ['title-append', 'header-right', 'default']
  slots.forEach((slot) =>
    it(`Should render ${slot} slot `, () => {
      const slotContent = `<div class="${slot}"></div>`
      wrapper = mount(CollapsibleCtr, {
        shallow: false,
        slots: { [slot]: slotContent },
        props: { title: '' },
      })
      expect(wrapper.find(`.${slot}`).html()).toBe(slotContent)
    })
  )
})
