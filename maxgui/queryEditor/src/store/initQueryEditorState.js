//TODO: Remove this file once ORM replacement is done
import { lodash } from '@share/utils/helpers'
import queryHelper from '@queryEditorSrc/store/queryHelper'

/**
 * @returns Return a new worksheet state
 */
export function defWorksheetState() {
    return {
        id: lodash.uniqueId(`WORKSHEET_${new Date().getUTCMilliseconds()}_`),
        name: 'WORKSHEET',
        ...queryHelper.syncStateCreator('schemaSidebar'),
    }
}
/**
 * @returns Return a new queryTab state
 */
export function defQueryTabState(wke_id) {
    return {
        id: lodash.uniqueId(`QUERY_TAB_${new Date().getUTCMilliseconds()}_`),
        name: 'Query Tab 1',
        wke_id_fk: wke_id,
        count: 1,
        ...queryHelper.syncStateCreator('editor'),
        ...queryHelper.syncStateCreator('queryResult'),
    }
}

const get_def_worksheets_arr = [defWorksheetState()]
const get_def_query_tabs = [defQueryTabState(get_def_worksheets_arr[0].id)]

export default {
    get_def_worksheets_arr,
    get_def_query_tabs,
}
