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
import { find } from '@/tests/utils'
import MigrationManage from '@wkeComps/DataMigration/MigrationManage.vue'
import { ETL_ACTION_MAP, ETL_STATUS_MAP } from '@/constants/workspace'

const taskStub = { status: ETL_STATUS_MAP.COMPLETE, id: 'id' }

const actionHandlerMock = vi.hoisted(() => vi.fn())

vi.mock('@wsServices/etlTaskService', async (importOriginal) => ({
  default: {
    ...(await importOriginal),
    actionHandler: actionHandlerMock,
  },
}))

describe('MigrationManage', () => {
  let wrapper
  beforeEach(() => {
    wrapper = mount(MigrationManage, { props: { task: taskStub } })
  })
  afterEach(() => vi.clearAllMocks())

  it(`Should render quick action button when shouldShowQuickActionBtn is true`, () => {
    expect(find(wrapper, 'quick-action-btn').exists()).toBe(true)
  })

  it(`Should render EtlTaskManage when shouldShowQuickActionBtn is false`, async () => {
    await wrapper.setProps({ task: { ...taskStub, status: ETL_STATUS_MAP.INITIALIZING } })
    expect(wrapper.findComponent({ name: 'EtlTaskManage' }).exists()).toBe(true)
  })

  it(`Should pass expected data to EtlTaskManage`, async () => {
    await wrapper.setProps({ task: { ...taskStub, status: ETL_STATUS_MAP.INITIALIZING } })
    const {
      $attrs: { modelValue },
      $props: { task, types },
    } = wrapper.findComponent({ name: 'EtlTaskManage' }).vm
    expect(modelValue).toBe(wrapper.vm.isMenuOpened)
    expect(task).toStrictEqual(wrapper.vm.$props.task)
    expect(types).toStrictEqual(wrapper.vm.ACTION_TYPES)
  })

  it(`actionTypes should be an array with expected strings`, () => {
    expect(wrapper.vm.ACTION_TYPES).toStrictEqual([
      ETL_ACTION_MAP.CANCEL,
      ETL_ACTION_MAP.DELETE,
      ETL_ACTION_MAP.DISCONNECT,
      ETL_ACTION_MAP.MIGR_OTHER_OBJS,
      ETL_ACTION_MAP.RESTART,
    ])
  })

  it(`quickActionBtnData should return an object with expected fields`, () => {
    expect(wrapper.vm.quickActionBtnData).toBeTypeOf('object')
    assert.containsAllKeys(wrapper.vm.quickActionBtnData, ['type', 'txt'])
  })

  it(`Should handle quickActionHandler method as expected`, async () => {
    await wrapper.vm.quickActionHandler()
    expect(actionHandlerMock).toHaveBeenCalledWith({
      type: wrapper.vm.quickActionBtnData.type,
      task: wrapper.vm.$props.task,
    })
  })
})
