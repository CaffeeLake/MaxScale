/*
 * Copyright (c) 2020 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2027-08-18
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */
/**
 * Function to create chartjs tooltip element
 * @param {Object} payload.tooltipModel - chartjs tooltipModel
 * @param {String} payload.tooltipId - tooltipId. Use to remove the tooltip when chart instance is destroyed
 * @param {Object} payload.position -  Chart position
 * @param {String} payload.className - Custom class name for tooltip element
 * @param {Boolean} payload.alignTooltipToLeft - To either align tooltip to the right or left.
 * If not provided, it aligns to center
 */
function createTooltipEle({ tooltipModel, tooltipId, position, className, alignTooltipToLeft }) {
    let tooltipEl = document.getElementById(tooltipId)
    // Create element on first render
    if (!tooltipEl) {
        tooltipEl = document.createElement('div')
        tooltipEl.id = tooltipId
        tooltipEl.className = [`chartjs-tooltip shadow-drop ${className}`]
        tooltipEl.innerHTML = '<table></table>'
        // append to #app div so that vuetify class can be used
        document.getElementById('app').appendChild(tooltipEl)
    }

    // Hide if no tooltip
    if (tooltipModel.opacity === 0) {
        tooltipEl.style.opacity = 0
        return
    }

    // Set caret Position
    tooltipEl.classList.remove(
        'above',
        'below',
        'no-transform',
        'chartjs-tooltip--transform-left',
        'chartjs-tooltip--transform-right',
        'chartjs-tooltip--transform-center'
    )
    if (tooltipModel.yAlign) tooltipEl.classList.add(tooltipModel.yAlign)
    else tooltipEl.classList.add('no-transform')

    if (typeof alignTooltipToLeft === 'boolean') {
        if (alignTooltipToLeft) tooltipEl.classList.add('chartjs-tooltip--transform-left')
        else tooltipEl.classList.add('chartjs-tooltip--transform-right')
    } else tooltipEl.classList.add('chartjs-tooltip--transform-center')

    // Display, position, and set styles for font
    tooltipEl.style.opacity = 1
    tooltipEl.style.position = 'absolute'
    tooltipEl.style.left = `${position.left + tooltipModel.caretX}px`
    tooltipEl.style.top = `${position.top + tooltipModel.caretY + 10}px`
    tooltipEl.style.fontFamily = tooltipModel._bodyFontFamily
    tooltipEl.style.fontStyle = tooltipModel._bodyFontStyle
    return tooltipEl
}

/**
 * Custom tooltip to show tooltip for multiple datasets. X axis value will be used
 * as the title of the tooltip. Body of the tooltip is generated by using
 * tooltipModel.body
 * @param {Object} payload.tooltipModel - chartjs tooltipModel
 * @param {String} payload.tooltipId - tooltipId. Use to remove the tooltip when chart instance is destroyed
 * @param {Object} payload.position -  chart canvas position (getBoundingClientRect)
 */
export function streamTooltip({ tooltipModel, tooltipId, position, alignTooltipToLeft }) {
    // Tooltip Element
    let tooltipEl = createTooltipEle({ tooltipModel, tooltipId, position, alignTooltipToLeft })
    // Set Text
    if (tooltipModel.body) {
        let titleLines = tooltipModel.title || []
        let bodyLines = tooltipModel.body.map(item => item.lines)

        let innerHtml = '<thead>'

        titleLines.forEach(title => {
            innerHtml += '<tr><th>' + title + '</th></tr>'
        })
        innerHtml += '</thead><tbody>'

        bodyLines.forEach((body, i) => {
            let colors = tooltipModel.labelColors[i]
            let style = 'background:' + colors.borderColor
            style += '; border-color:' + colors.borderColor
            style += '; border-width: 2px;margin-right:4px'
            let span = '<span class="chartjs-tooltip-key" style="' + style + '"></span>'
            innerHtml += '<tr><td>' + span + body + '</td></tr>'
        })
        innerHtml += '</tbody>'

        let tableRoot = tooltipEl.querySelector('table')
        tableRoot.innerHTML = innerHtml
    }
}

/**
 * Rendering object tooltip for a single dataset by using provided dataPointObj
 * @param {Object} payload.tooltipModel - chartjs tooltipModel
 * @param {String} payload.tooltipId - tooltipId. Use to remove the tooltip when chart instance is destroyed
 * @param {Object} payload.position -  chart canvas position (getBoundingClientRect)
 * @param {Object} payload.dataPoint - data point object
 */
export function objectTooltip({
    tooltipModel,
    tooltipId,
    position,
    dataPoint,
    alignTooltipToLeft,
}) {
    // Tooltip Element
    let tooltipEl = createTooltipEle({
        tooltipModel,
        tooltipId,
        position,
        alignTooltipToLeft,
        className: 'query-editor-chart-tooltip',
    })
    // Set Text
    if (tooltipModel.body) {
        let innerHtml = '<tbody>'
        Object.keys(dataPoint.dataPointObj).forEach(key => {
            if (key !== 'x' && key !== 'y') {
                //bold x,y axes value
                const boldClass = `${
                    key === dataPoint.scaleLabelX || key === dataPoint.scaleLabelY
                        ? 'font-weight-black'
                        : ''
                }`
                innerHtml += `
                <tr>
                    <td class="mxs-color-helper text-small-text ${boldClass}">
                     ${key}
                     </td>
                    <td class="mxs-color-helper text-navigation ${boldClass}">
                        ${dataPoint.dataPointObj[key]}
                    </td>
                </tr>`
            }
        })
        innerHtml += '</tbody>'
        let tableRoot = tooltipEl.querySelector('table')
        tableRoot.innerHTML = innerHtml
    }
}
