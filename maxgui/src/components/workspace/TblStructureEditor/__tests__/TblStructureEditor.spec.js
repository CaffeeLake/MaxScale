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
import TblStructureEditor from '@wsComps/TblStructureEditor/TblStructureEditor.vue'
import { lodash } from '@/utils/helpers'
import { TABLE_STRUCTURE_SPEC_MAP } from '@/constants/workspace'
import {
  editorDataStub,
  charsetCollationMapStub,
} from '@wsComps/TblStructureEditor/__tests__/stubData'
import { createStore } from 'vuex'

const mockStore = createStore({
  state: {
    schemaInfo: {
      charset_collation_map: charsetCollationMapStub,
      def_db_charset_map: { test: 'utf8mb4' },
      engines: [],
    },
    workspace: { exec_sql_dlg: {} },
  },
  commit: vi.fn(),
})

const mountFactory = (opts) =>
  mount(
    TblStructureEditor,
    lodash.merge(
      {
        shallow: false,
        props: {
          modelValue: editorDataStub,
          dim: { width: 500, height: 800 },
          initialData: lodash.cloneDeep(editorDataStub),
          connReqData: {},
          activeSpec: '',
          lookupTables: { [editorDataStub.id]: editorDataStub },
        },
      },
      opts
    ),
    mockStore
  )

let wrapper
describe('TblStructureEditor', () => {
  beforeEach(() => {
    wrapper = mountFactory({
      props: {
        'onUpdate:modelValue': (v) => wrapper && wrapper.setProps({ modelValue: v }),
        'onUpdate:activeSpec': (v) => wrapper && wrapper.setProps({ activeSpec: v }),
      },
    })
  })

  it(`Should pass expected data to TableOpts`, () => {
    const { modelValue, engines, charsetCollationMap, defDbCharset, isCreating, schemas } =
      wrapper.findComponent({
        name: 'TableOpts',
      }).vm.$props
    expect(modelValue).toStrictEqual(wrapper.vm.tblOpts)
    expect(engines).toStrictEqual(wrapper.vm.engines)
    expect(charsetCollationMap).toStrictEqual(wrapper.vm.charset_collation_map)
    expect(defDbCharset).toBe('utf8mb4')
    expect(isCreating).toBe(wrapper.vm.$props.isCreating)
    expect(schemas).toStrictEqual(wrapper.vm.schemas)
  })

  const specCases = {
    [TABLE_STRUCTURE_SPEC_MAP.COLUMNS]: 'ColDefinitions',
    [TABLE_STRUCTURE_SPEC_MAP.FK]: 'FkDefinitionsWrapper',
    [TABLE_STRUCTURE_SPEC_MAP.INDEXES]: 'IndexDefinitions',
  }
  Object.keys(specCases).forEach((spec) => {
    it(`Should render ${specCases[spec]} based on activeSpecTab`, async () => {
      wrapper.vm.activeSpecTab = spec
      await wrapper.vm.$nextTick()
      expect(wrapper.findComponent({ name: specCases[spec] }).exists()).toBe(true)
    })
  })

  it(`Should pass expected data to ColDefinitions`, async () => {
    wrapper.vm.activeSpecTab = TABLE_STRUCTURE_SPEC_MAP.COLUMNS
    await wrapper.vm.$nextTick()
    const {
      modelValue,
      charsetCollationMap,
      initialData,
      dim,
      defTblCharset,
      defTblCollation,
      colKeyCategoryMap,
    } = wrapper.findComponent({
      name: 'ColDefinitions',
    }).vm.$props
    expect(modelValue).toStrictEqual(wrapper.vm.defs)
    expect(charsetCollationMap).toStrictEqual(wrapper.vm.charset_collation_map)
    expect(initialData).toStrictEqual(wrapper.vm.initialDefinitions)
    expect(dim).toStrictEqual(wrapper.vm.tabDim)
    expect(defTblCharset).toStrictEqual(wrapper.vm.tblOpts.charset)
    expect(defTblCollation).toStrictEqual(wrapper.vm.tblOpts.collation)
    expect(colKeyCategoryMap).toStrictEqual(wrapper.vm.colKeyCategoryMap)
  })

  it(`Should pass expected data to FkDefinitionsWrapper`, async () => {
    wrapper.vm.activeSpecTab = TABLE_STRUCTURE_SPEC_MAP.FK
    await wrapper.vm.$nextTick()
    const {
      $props: { engine },
      $attrs: {
        modelValue,
        lookupTables,
        newLookupTables,
        allLookupTables,
        allTableColMap,
        refTargets,
        tablesColNameMap,
        tableId,
        dim,
        connReqData,
        charsetCollationMap,
      },
    } = wrapper.findComponent({
      name: 'FkDefinitionsWrapper',
    }).vm
    expect(engine).toStrictEqual(wrapper.vm.tblOpts.engine)
    expect(modelValue).toStrictEqual(wrapper.vm.keyCategoryMap)
    expect(lookupTables).toStrictEqual(wrapper.vm.$props.lookupTables)
    expect(newLookupTables).toStrictEqual(wrapper.vm.newLookupTables)
    expect(allLookupTables).toStrictEqual(wrapper.vm.allLookupTables)
    expect(allTableColMap).toStrictEqual(wrapper.vm.allTableColMap)
    expect(refTargets).toStrictEqual(wrapper.vm.refTargets)
    expect(tablesColNameMap).toStrictEqual(wrapper.vm.tablesColNameMap)
    expect(tableId).toBe(wrapper.vm.stagingData.id)
    expect(dim).toStrictEqual(wrapper.vm.tabDim)
    expect(connReqData).toStrictEqual(wrapper.vm.$props.connReqData)
    expect(charsetCollationMap).toStrictEqual(wrapper.vm.charset_collation_map)
  })

  it(`Should pass expected data to IndexDefinitions`, async () => {
    wrapper.vm.activeSpecTab = TABLE_STRUCTURE_SPEC_MAP.INDEXES
    await wrapper.vm.$nextTick()
    const { modelValue, tableColNameMap, dim, tableColMap } = wrapper.findComponent({
      name: 'IndexDefinitions',
    }).vm.$props
    expect(modelValue).toStrictEqual(wrapper.vm.keyCategoryMap)
    expect(tableColNameMap).toStrictEqual(wrapper.vm.tablesColNameMap[wrapper.vm.stagingData.id])
    expect(dim).toStrictEqual(wrapper.vm.tabDim)
    expect(tableColMap).toStrictEqual(wrapper.vm.allTableColMap[wrapper.vm.stagingData.id])
  })

  it('Should conditionally render `apply-btn`', async () => {
    expect(find(wrapper, 'apply-btn').exists()).toBe(true)
    await wrapper.setProps({ showApplyBtn: false })
    expect(find(wrapper, 'apply-btn').exists()).toBe(false)
  })

  it('Should not render `revert-btn` in creation mode', async () => {
    await wrapper.setProps({ isCreating: true })
    expect(find(wrapper, 'revert-btn').exists()).toBe(false)
  })

  it('Should disables the `apply-btn` when no changes are made', () => {
    expect(find(wrapper, 'apply-btn').vm.disabled).toBe(true)
  })

  it('renders toolbar-append slot content', () => {
    wrapper = mountFactory({
      slots: { 'toolbar-append': '<div data-test="toolbar-append">test div</div>' },
    })
    expect(find(wrapper, 'toolbar-append').exists()).toBe(true)
  })

  it('Should initially emit update:activeSpec event', () => {
    expect(wrapper.emitted('update:activeSpec')[0][0]).toStrictEqual(
      TABLE_STRUCTURE_SPEC_MAP.COLUMNS
    )
  })

  it('Should return accurate value for stagingData', () => {
    expect(wrapper.vm.stagingData).toStrictEqual(wrapper.vm.$props.modelValue)
  })

  it('Should return accurate value for hasChanged when there is a diff', async () => {
    expect(wrapper.vm.hasChanged).toBe(false)
    await wrapper.setProps({ modelValue: {} })
    expect(wrapper.vm.hasChanged).toBe(true)
  })
})
