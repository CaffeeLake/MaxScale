/*
 * Copyright (c) 2023 MariaDB plc
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2029-02-28
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */
import limiter from 'sql-limiter'
import { findParenLevelToken } from 'sql-limiter/src/token-utils'
import { lodash } from '@/utils/helpers'

/**
 * Filter out limit and offset and its values tokens
 * @param {Array<object>} tokens
 * @param {object} limitToken
 * @param {object} offsetToken
 * @returns {Array<object>} filtered tokens
 */
function filterLimitOffsetTokens(tokens, limitToken, offsetToken) {
  let filteredTokens = []
  let skipMode = false
  let removingLimit = false
  let removingOffset = false

  for (const token of tokens) {
    if (lodash.isEqual(token, limitToken)) {
      skipMode = true
      removingLimit = true
      continue
    }

    if (lodash.isEqual(token, offsetToken)) {
      skipMode = true
      removingOffset = true
      continue
    }

    if (skipMode) {
      // Retain comments and whitespace
      if (token.type === 'whitespace' || token.type === 'comment') {
        filteredTokens.push(token)
        continue
      }
      if (removingLimit && token.type === 'comma') continue
      if ((token.type === 'number' || token.type === 'comma') && (removingLimit || removingOffset))
        continue

      skipMode = false
      removingLimit = false
      removingOffset = false
    }

    filteredTokens.push(token)
  }

  return filteredTokens
}

function findParenLevelKeywordToken({ tokens, startIndex, keyword }) {
  return findParenLevelToken(
    tokens,
    startIndex,
    (token) => token.type === 'keyword' && token.value === keyword
  )
}

/**
 * This function splits the query into statements accurately in most cases,
 * except compound statements, as it splits SQL text on the ";" delimiter
 * @param {string} sql
 * @returns {string[]}
 */
export function splitSQL(sql) {
  return limiter.getStatements(sql)
}

export function genStatement({ text = '', limit = undefined, offset = undefined, type = '' } = {}) {
  return { text, limit, offset, type }
}

/**
 * This function splits the SQL into statement classes parsed by sql-limiter
 * @param {string} sql
 * @returns {[Error|undefined, Class[]|undefined]}
 */
export function getStatementClasses(sql) {
  try {
    return [undefined, limiter.getStatementClasses(sql)]
  } catch (e) {
    return [e, undefined]
  }
}

/**
 * Enforce limit and offset for `select` statement
 * If existing limit exists, it will be lowered if it is larger than `limitNumber` specified.
 * If limit does not exist, it will be added.
 * If existing offset exists, it will not be altered unless shouldReplace is true
 * @param {object} param
 * @param {Class} param.statementClass - a statement class, returned by getStatementClasses
 * @param {number} param.limitNumber -- number to enforce for limit keyword
 * @param {number} param.offsetNumber -- offset number to inject
 * @param {boolean} param.shouldReplace -- If true, it injects or replaces the existing limit and offset values
 * @returns {[Error|undefined, undefined|{text: string, offset: number|undefined, limit: number|undefined, type: string}]}
 */
export function enforceLimitOffset({
  statementClass,
  limitNumber,
  offsetNumber,
  shouldReplace = false,
}) {
  try {
    let statement
    const limit = statementClass.enforceLimit(['limit', 'fetch'], limitNumber, shouldReplace)
    const offset = statementClass.injectOffset(offsetNumber, shouldReplace)
    const text = limiter.removeTerminator(statementClass.toString().trim())
    /**
     * sql-limiter treats the line-break after the last delimiter as a statement, so
     * text could be empty after trimming.
     */
    if (text)
      statement = genStatement({ text, limit, offset, type: limiter.getStatementType(text) })
    return [undefined, statement]
  } catch (e) {
    return [e, undefined]
  }
}

/**
 * @param {class} statementClass - select statement
 * @returns {[Error|undefined, undefined|{text: string, offset: undefined, limit: 0, type: 'select'}]}
 */
export function enforceNoLimit(statementClass) {
  try {
    let statement
    const { statementToken, tokens } = statementClass
    if (statementToken) {
      const limitToken = findParenLevelKeywordToken({
        tokens,
        startIndex: statementToken.index,
        keyword: 'limit',
      })
      const offsetToken = findParenLevelKeywordToken({
        tokens,
        startIndex: statementToken.index,
        keyword: 'offset',
      })
      if (limitToken)
        statementClass.tokens = filterLimitOffsetTokens(tokens, limitToken, offsetToken)
    }
    const text = limiter.removeTerminator(statementClass.toString().trim())
    if (text)
      statement = genStatement({
        text,
        limit: 0, // for api max_rows field which indicates no limit
        offset: undefined,
        type: 'select',
      })
    return [undefined, statement]
  } catch (e) {
    return [e, undefined]
  }
}