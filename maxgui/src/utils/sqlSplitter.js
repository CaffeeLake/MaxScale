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
import { splitQuery, mysqlSplitterOptions } from 'dbgate-query-splitter'

export function findCustomDelimiter(extractedPart) {
  const delimiterMatch = extractedPart.match(/DELIMITER\s+(\S+)/i)
  if (delimiterMatch) return delimiterMatch[1].trim()
  return null
}

function addTerminatorInfo(sql, statements) {
  let pos = 0 // Start position of the extracted part
  const defDelimiter = ';' // Default delimiter

  statements.forEach((stmt) => {
    // Get the part that was extracted by splitQuery function
    const extractedPart = sql.substring(pos, stmt.trimStart.position).trim()
    stmt.delimiter = findCustomDelimiter(extractedPart) || defDelimiter
    // Update the pos
    pos = stmt.trimEnd.position
  })

  return statements
}

/**
 * This function splits the query into statements accurately for most cases
 * except compound statements. It requires the presence of DELIMITER to split
 * correctly.
 * For example: below sql will be splitted accurately into 1 statement.
 * DELIMITER //
 * IF (1>0) THEN BEGIN NOT ATOMIC SELECT 1; END ; END IF;
 * DELIMITER ;
 * @param {string} sql
 * @returns {[Error|undefined, string[]|undefined]}
 */
export default (sql) => {
  try {
    const statements = splitQuery(sql, { ...mysqlSplitterOptions, returnRichInfo: true })
    addTerminatorInfo(sql, statements)
    return [null, statements]
  } catch (e) {
    return [e, null]
  }
}
