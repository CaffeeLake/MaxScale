/*
 * Copyright (c) 2020 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2026-04-08
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */
require("./common.js")();

exports.command = "reload <command>";
exports.desc = "Reload objects";
exports.handler = function () {};
exports.builder = function (yargs) {
  yargs
    .command(
      "service <service>",
      "Reloads the database users of this service",
      function (yargs) {
        return yargs.usage("Usage: reload service <service>");
      },
      function (argv) {
        maxctrl(argv, function (host) {
          return doRequest(host, "services/" + argv.service + "/reload", { method: "POST" });
        });
      }
    )
    .command(
      "tls",
      "Reload TLS certificates",
      function (yargs) {
        return yargs
          .epilog("This command reloads the TLS certificates for all listeners and servers in MaxScale.")
          .usage("Usage: reload service <service>");
      },
      function (argv) {
        maxctrl(argv, function (host) {
          return doRequest(host, "maxscale/tls/reload", { method: "POST" });
        });
      }
    )
    .usage("Usage: reload <command>")
    .help()
    .wrap(null)
    .demandCommand(1, helpMsg);
};
