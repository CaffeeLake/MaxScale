/*
 * Copyright (c) 2018 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2025-02-16
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */
require('./common.js')()

const classify_fields = [
    {'Parse result': 'attributes.parse_result'},
    {'Type mask': 'attributes.type_mask'},
    {'Operation': 'attributes.operation'},
    {'Has where clause': 'attributes.has_where_clause'},
    {'Fields': 'attributes.fields'},
    {'Functions': 'attributes.functions'}
]

exports.command = 'classify <statement>'
exports.desc = 'Classify statement'
exports.handler = function (argv) {
    maxctrl(argv, function (host) {
        return doRequest(host,
                         'maxscale/query_classifier/classify?sql=' + argv.statement,
                         (res) => {
                             if (res.data.attributes.functions) {
                                 var a = res.data.attributes.functions.map((f) => {
                                     return f.name + ': (' + f.arguments.join(', ') + ')'
                                 });

                                 res.data.attributes.functions = a;
                             }

                             return formatResource(classify_fields, res.data)
                         })
    })
}
exports.builder = function(yargs) {
    yargs
        .usage('Usage: classify <statement>')
        .epilog('Classify the statement using MaxScale and display the result. ' +
                'The possible values for "Parse result", "Type mask" and "Operation" ' +
                'can be looked up in ' +
                'https://github.com/mariadb-corporation/MaxScale/blob/2.3/include/maxscale/query_classifier.h')
        .help()
}
