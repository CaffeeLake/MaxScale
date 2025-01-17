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
import ObjSelect from '@/components/common/ObjSelect.vue'
import { lodash } from '@/utils/helpers'

const multipleChoiceItems = [
  {
    id: 'RWS-Router',
    type: 'services',
  },
  {
    id: 'RCR-Writer',
    type: 'services',
  },
  {
    id: 'RCR-Router',
    type: 'services',
  },
]

const singleChoiceItems = [
  { id: 'Monitor-Test', type: 'monitors' },
  { id: 'Monitor', type: 'monitors' },
]
const mountFactory = (opts) =>
  mount(
    ObjSelect,
    lodash.merge(
      {
        shallow: false,
        props: {
          // type is always plural by default, this makes translating process easier
          type: 'servers',
        },
        attrs: {
          items: [
            {
              attributes: { state: 'Down' },
              id: 'test-server',
              links: { self: 'https://127.0.0.1:8989/v1/servers/test-server' },
              type: 'servers',
            },
          ],
        },
      },
      opts
    )
  )
describe('ObjSelect.vue', () => {
  let wrapper

  beforeEach(() => {
    wrapper = mountFactory()
  })

  it(`Should show placeholder props to VSelect as expected`, async () => {
    const props = wrapper.findComponent({ name: 'VSelect' }).vm.$props
    expect(props.placeholder).not.toBe('')
    await wrapper.setProps({ showPlaceHolder: false })
    expect(props.placeholder).toBe('')
  })

  it(`Should use initialValue object`, () => {
    wrapper = mountFactory({
      attrs: {
        items: singleChoiceItems,
      },
      props: {
        modelValue: singleChoiceItems[0],
        type: 'monitors',
        initialValue: singleChoiceItems[0],
      },
    })
    const { modelValue } = wrapper.findComponent({ name: 'VSelect' }).vm.$props
    expect(modelValue).toBeTypeOf('object')
    expect(modelValue.id).toBe(singleChoiceItems[0].id)
  })

  it(`Should use initialValue array`, () => {
    const initialValue = [multipleChoiceItems[0], multipleChoiceItems[1]]
    wrapper = mountFactory({
      attrs: {
        items: multipleChoiceItems,
        multiple: true,
      },
      props: {
        modelValue: [multipleChoiceItems[0], multipleChoiceItems[1]],
        type: 'services',
        initialValue,
      },
    })
    const { modelValue } = wrapper.findComponent({ name: 'VSelect' }).vm.$props
    expect(modelValue).toBeInstanceOf(Array)
    modelValue.forEach((item, i) => expect(item.id).toBe(multipleChoiceItems[i].id))
  })
})
