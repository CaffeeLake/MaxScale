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
import ServerForm from '@/components/common/ObjectForms/ServerForm.vue'
import { MXS_OBJ_TYPE_MAP } from '@/constants'

const modulesMockData = [
  {
    attributes: {
      module_type: 'servers',
      parameters: [
        {
          description: 'Server address',
          mandatory: false,
          modifiable: true,
          name: 'address',
          type: 'string',
        },
        {
          default_value: 3306,
          description: 'Server port',
          mandatory: false,
          modifiable: true,
          name: 'port',
          type: 'count',
        },
        {
          description: 'Server UNIX socket',
          mandatory: false,
          modifiable: true,
          name: 'socket',
          type: 'string',
        },
      ],
    },
    id: 'servers',
  },
]
const moduleParamsProps = { modules: modulesMockData, validate: vi.fn() }

describe('ServerForm', () => {
  let wrapper
  beforeEach(() => {
    wrapper = mount(ServerForm, {
      props: {
        allServices: [],
        allMonitors: [],
        moduleParamsProps,
      },
    })
  })

  it(`Should pass expected data to ModuleParameters`, () => {
    const moduleParameters = wrapper.findComponent({ name: 'ModuleParameters' })
    const {
      $props: { defModuleId, modules, validate },
      $attrs: { mxsObjType },
    } = moduleParameters.vm
    expect(defModuleId).toBe(MXS_OBJ_TYPE_MAP.SERVERS)
    expect(modules).toStrictEqual(wrapper.vm.$props.moduleParamsProps.modules)
    expect(mxsObjType).toBe(MXS_OBJ_TYPE_MAP.SERVERS)
    assert.isFunction(validate)
  })

  it(`Should pass expected data ResourceRelationships`, () => {
    const resourceRelationships = wrapper.findComponent({ name: 'ResourceRelationships' })
    const { type, items, initialValue, multiple } = resourceRelationships.vm.$attrs
    expect(type).toBe('services')
    expect(items).toStrictEqual(wrapper.vm.servicesList)
    expect(initialValue).toStrictEqual(wrapper.vm.defaultServiceItems)
    expect(multiple).toBeDefined()
  })

  const listTestCases = ['monitorsList', 'servicesList']
  listTestCases.forEach((property) =>
    it(`Should compute ${property} as expected`, () => {
      expect(wrapper.vm[property]).toBeInstanceOf(Array)
      wrapper.vm[property].forEach((obj) => {
        assert.containsAllKeys(obj, ['id', 'type'])
      })
    })
  )

  const withRelationshipTestCases = [{ withRelationship: true }, { withRelationship: false }]

  withRelationshipTestCases.forEach(({ withRelationship }) => {
    it(`Should ${
      withRelationship ? '' : 'not'
    } render ResourceRelationships components`, async () => {
      await wrapper.setProps({ withRelationship })
      expect(
        wrapper.findAllComponents({
          name: 'ResourceRelationships',
        }).length
      ).toBe(withRelationship ? 2 : 0)
    })

    it(`getValues method should return expected values when
        withRelationship props is ${withRelationship}`, async () => {
      await wrapper.setProps({ withRelationship })
      const values = wrapper.vm.getValues()
      if (withRelationship)
        expect(values).toStrictEqual({
          attributes: { parameters: wrapper.vm.changedParams },
          relationships: {
            monitors: { data: wrapper.vm.selectedMonitor },
            services: { data: wrapper.vm.selectedServices },
          },
        })
      else expect(values).toStrictEqual({ attributes: { parameters: wrapper.vm.changedParams } })
    })
  })
})
