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
import { LOADING_TIME, COMMON_OBJ_OP_TYPE_MAP } from '@/constants'
import { OVERLAY_TRANSPARENT_LOADING } from '@/constants/overlayTypes'
import { http } from '@/utils/axios'
import { delay, getAppEle, uuidv1, getCurrentTimeStamp } from '@/utils/helpers'

export function useTypy() {
  const vm = getCurrentInstance()
  return vm.appContext.config.globalProperties.$typy
}

export function useLogger() {
  const vm = getCurrentInstance()
  return vm.appContext.config.globalProperties.$logger
}

export function useHelpers() {
  const vm = getCurrentInstance()
  return vm.appContext.config.globalProperties.$helpers
}

export function useHttp() {
  return http
}

/**
 *
 * @param {string} param.key - default key name
 * @param {boolean} param.isDesc - default isDesc state
 * @returns
 */
export function useSortBy({ key = '', isDesc = false }) {
  const sortBy = ref({ key, isDesc })
  function toggleSortBy(key) {
    if (sortBy.value.isDesc)
      sortBy.value = { key: '', isDesc: false } // non-sort
    else if (sortBy.value.key === key) sortBy.value = { key, isDesc: !sortBy.value.isDesc }
    else sortBy.value = { key, isDesc: false }
  }
  function compareFn(a, b) {
    const aStr = String(a[sortBy.value.key])
    const bStr = String(b[sortBy.value.key])
    return sortBy.value.isDesc ? bStr.localeCompare(aStr) : aStr.localeCompare(bStr)
  }
  return { sortBy, toggleSortBy, compareFn }
}

export function useLoading() {
  const isMounting = ref(true)
  const store = useStore()
  const overlay_type = computed(() => store.state.mxsApp.overlay_type)
  const loading = computed(() =>
    isMounting.value || overlay_type.value === OVERLAY_TRANSPARENT_LOADING ? 'primary' : false
  )
  onMounted(async () => await delay(LOADING_TIME).then(() => (isMounting.value = false)))
  return loading
}

export function useGoBack() {
  const store = useStore()
  const router = useRouter()
  const route = useRoute()
  const prev_route = computed(() => store.state.prev_route)
  return () => {
    switch (prev_route.value.name) {
      case 'login':
        router.push('/dashboard/servers')
        break
      case undefined: {
        /**
         * Navigate to parent path. e.g. current path is /dashboard/servers/server_0,
         * it navigates to /dashboard/servers/
         */
        const parentPath = route.path.slice(0, route.path.lastIndexOf('/'))
        if (parentPath) router.push(parentPath)
        else router.push('/dashboard/servers')
        break
      }
      default:
        router.push(prev_route.value.path)
        break
    }
  }
}

export function useCommonObjOpMap(objType) {
  const { DESTROY } = COMMON_OBJ_OP_TYPE_MAP
  const { t } = useI18n()
  const goBack = useGoBack()
  const { deleteObj } = useMxsObjActions(objType)
  return {
    map: {
      [DESTROY]: {
        title: `${t('destroy')} ${t(objType, 1)}`,
        type: DESTROY,
        icon: 'mxs:delete',
        iconSize: 18,
        color: 'error',
        info: '',
        disabled: false,
      },
    },
    handler: async ({ op, id }) => {
      if (op.type === COMMON_OBJ_OP_TYPE_MAP.DESTROY) await deleteObj(id)
      goBack()
    },
  }
}

/**
 * To use this composable, a mousedown event needs to be listened on drag target
 * element and do the followings assign
 * assign true to `isDragging` and event.target to `dragTarget`
 * @returns {DragAndDropData} Reactive object: { isDragging, dragTarget }
 */
export function useDragAndDrop(emitter) {
  const DRAG_TARGET_ID = 'target-drag'
  const isDragging = ref(false)
  const dragTarget = ref(null)

  watch(isDragging, (v) => {
    if (v) addDragEvts()
    else removeDragEvts()
  })
  onBeforeUnmount(() => removeDragEvts())

  /**
   * This copies inherit styles from srcNode to dstNode
   * @param {Object} payload.srcNode - html node to be copied
   * @param {Object} payload.dstNode - target html node to pasted
   */
  function copyNodeStyle({ srcNode, dstNode }) {
    const computedStyle = window.getComputedStyle(srcNode)
    Array.from(computedStyle).forEach((key) =>
      dstNode.style.setProperty(
        key,
        computedStyle.getPropertyValue(key),
        computedStyle.getPropertyPriority(key)
      )
    )
  }

  function removeTargetDragEle() {
    const elem = document.getElementById(DRAG_TARGET_ID)
    if (elem) elem.parentNode.removeChild(elem)
  }

  function addDragTargetEle(e) {
    const cloneNode = dragTarget.value.cloneNode(true)
    cloneNode.setAttribute('id', DRAG_TARGET_ID)
    cloneNode.textContent = dragTarget.value.textContent
    copyNodeStyle({ srcNode: dragTarget.value, dstNode: cloneNode })
    cloneNode.style.position = 'absolute'
    cloneNode.style.top = e.clientY + 'px'
    cloneNode.style.left = e.clientX + 'px'
    cloneNode.style.zIndex = 9999
    getAppEle().appendChild(cloneNode)
  }

  function addDragEvts() {
    document.addEventListener('mousemove', onDragging)
    document.addEventListener('mouseup', onDragEnd)
  }

  function removeDragEvts() {
    document.removeEventListener('mousemove', onDragging)
    document.removeEventListener('mouseup', onDragEnd)
  }

  function onDragging(e) {
    e.preventDefault()
    if (isDragging.value) {
      removeTargetDragEle()
      addDragTargetEle(e)
      emitter('on-dragging', e)
    }
  }

  function onDragEnd(e) {
    e.preventDefault()
    if (isDragging.value) {
      removeTargetDragEle()
      emitter('on-dragend', e)
      isDragging.value = false
    }
  }

  return { isDragging, dragTarget }
}

export function useEventDispatcher(KEY) {
  const eventData = ref(null)
  provide(KEY, eventData)
  /**
   * @param {string} name
   * @param {any} [payload] - The payload associated with the event
   */
  return (name, payload) => {
    eventData.value = { id: uuidv1(), name, payload }
  }
}

/**
 * @param {object} start - proxy object
 * @param {object} end - proxy object
 */
export function useElapsedTimer(start, end) {
  const count = ref(0)
  const isRunning = computed(() => start.value && !end.value)
  const elapsedTime = computed(() =>
    isRunning.value ? 0 : parseFloat(((end.value - start.value) / 1000).toFixed(4))
  )

  watch(
    isRunning,
    (v) => {
      if (v) updateCount()
      else count.value = 0
    },
    { immediate: true }
  )

  function updateCount() {
    if (!isRunning.value) return
    const now = getCurrentTimeStamp()
    count.value = parseFloat(((now - start.value) / 1000).toFixed(4))
    requestAnimationFrame(updateCount)
  }

  return { isRunning, count, elapsedTime }
}

/**
 * @param {object} param.data - proxy object. Array of objects to process.
 * @param {string} param.field - The field in each object that contains an array of strings or objects.
 * @param {string} [param.subField] - Subfield to extract values from objects within the array specified by `field`.
 * @return {object} proxy object
 */
export function useCountUniqueValues({ data, field, subField }) {
  const total = ref(0)
  watch(
    data,
    (v) => {
      const allItems = v.flatMap((item) => {
        if (Array.isArray(item[field]))
          return subField ? item[field].map((obj) => obj[subField]) : item[field]
        return []
      })
      total.value = Array.from(new Set(allItems)).length
    },
    { immediate: true, deep: true }
  )
  return total
}

export function useValidationRule() {
  const { t } = useI18n()
  const typy = useTypy()
  return {
    validateRequired: (v) => !!v || v === 0 || t('errors.requiredField'), // null, undefined, and empty string will return the msg
    validateRequiredStr: (v) => !typy(v).isEmptyString || t('errors.requiredField'),
    validateRequiredArr: (v) => typy(v).safeArray.length > 0 || t('errors.requiredField'),
    validateNonNegative: (v) => (typy(v).isNumber && v >= 0) || t('errors.negativeNum'),
    validatePositiveNum: (v) => (typy(v).isNumber && v > 0) || t('errors.largerThanZero'),
    validateInteger: (v) => (typy(v).isNumber && /^[-]?\d+$/g.test(v)) || t('errors.nonInteger'),
    validateHexColor: (v) => Boolean(v.match(/^#[0-9A-F]{6}$/i)) || t('errors.hexColor'),
  }
}

export function useCtxMenu() {
  const data = ref({
    isOpened: false,
    type: '',
    item: {},
    activatorId: '',
    target: [0, 0], // coord
  })
  const isOpened = computed(() => data.value.isOpened)

  watch(isOpened, (v) => {
    // clear data
    if (!v) {
      data.value.item = {}
      data.value.activatorId = ''
    }
  })

  function openCtxMenu({ e, type, activatorId = '', item = {} }) {
    data.value = {
      isOpened: true,
      type,
      item,
      activatorId: item.id || activatorId,
      target: [e.clientX, e.clientY],
    }
  }

  return { data, openCtxMenu }
}

export function useZoomAndPanController() {
  const panAndZoom = ref({ x: 0, y: 0, k: 1, transition: false, eventType: '' })
  const isFitIntoView = ref(false)

  watch(
    panAndZoom,
    (v) => {
      if (v.eventType && v.eventType == 'wheel') isFitIntoView.value = false
    },
    { deep: true }
  )

  /**
   * @param {object} param.extent - graph extent
   * @param {object} param.dim - graph dimension
   * @param {array} param.scaleExtent - e.g. [0.25, 2]
   * @param {number} [param.paddingPct]
   * @returns {number} zoom ratio
   */
  function calcFitZoom({ extent: { minX, maxX, minY, maxY }, dim, scaleExtent, paddingPct = 2 }) {
    const graphWidth = maxX - minX
    const graphHeight = maxY - minY
    const xScale = (dim.width / graphWidth) * (1 - paddingPct / 100)
    const yScale = (dim.height / graphHeight) * (1 - paddingPct / 100)
    // Choose the minimum scale among xScale, yScale, and the maximum allowed scale
    let k = Math.min(xScale, yScale, scaleExtent[1])
    // Clamp the scale value within the scaleExtent range
    k = Math.min(Math.max(k, scaleExtent[0]), scaleExtent[1])
    return k
  }

  /**
   * Auto adjust (zoom in or out) the contents of a graph
   * @param {object} param
   * @param {number} [param.v] - zoom value
   * @param {boolean} [param.isFitIntoView] - if it's true, v param will be ignored
   * @param {object} param.extent
   * @param {array} param.scaleExtent
   * @param {object} param.dim
   * @param {boolean} [param.transition] - true by default
   */
  function zoomTo({
    v,
    isFitIntoView: isFitIntoViewValue = false,
    extent,
    scaleExtent,
    dim,
    paddingPct = 2,
    transition = true,
  }) {
    isFitIntoView.value = isFitIntoViewValue

    const { minX, minY, maxX, maxY } = extent
    const k = isFitIntoViewValue ? calcFitZoom({ extent, dim, scaleExtent, paddingPct }) : v
    const x = dim.width / 2 - ((minX + maxX) / 2) * k
    const y = dim.height / 2 - ((minY + maxY) / 2) * k

    panAndZoom.value = { x, y, k, transition, eventType: '' }
  }

  return { panAndZoom, isFitIntoView, zoomTo }
}
