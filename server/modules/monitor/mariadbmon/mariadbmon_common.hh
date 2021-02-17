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
#pragma once

/**
 * Common definitions for MariaDBMonitor module. Should be included in every header or source file.
 */

#define MXS_MODULE_NAME "mariadbmon"

#include <maxscale/ccdefs.hh>

#include <string>
#include <maxscale/json_api.hh>

/** Utility macros for printing both MXS_ERROR and json error */
#define PRINT_MXS_JSON_ERROR(err_out, format, ...) \
    do { \
        MXS_ERROR(format, ##__VA_ARGS__); \
        if (err_out) \
        { \
            *err_out = mxs_json_error_append(*err_out, format, ##__VA_ARGS__); \
        } \
    } while (false)

#define PRINT_ERROR_IF(log_mode, err_out, format, ...) \
    if (log_mode == Log::ON) \
    { \
        PRINT_MXS_JSON_ERROR(err_out, format, ##__VA_ARGS__); \
    } \

extern const int64_t SERVER_ID_UNKNOWN;
extern const int64_t GTID_DOMAIN_UNKNOWN;
extern const int PORT_UNKNOWN;
extern const char* const CN_HANDLE_EVENTS;

// Helper class for concatenating strings with a delimiter.
class DelimitedPrinter
{
private:
    DelimitedPrinter(const DelimitedPrinter&) = delete;
    DelimitedPrinter& operator=(const DelimitedPrinter&) = delete;
    DelimitedPrinter() = delete;
public:
    DelimitedPrinter(const std::string& separator);

    /**
     * Add to string.
     *
     * @param target String to modify
     * @param addition String to add. The delimiter is printed before the addition.
     */
    void cat(std::string& target, const std::string& addition);

    /**
     * Add to internal string.
     *
     * @param addition The string to add.
     */
    void cat(const std::string& addition);

    std::string message() const;

private:
    const std::string m_separator;
    std::string       m_current_separator;
    std::string       m_message;
};
