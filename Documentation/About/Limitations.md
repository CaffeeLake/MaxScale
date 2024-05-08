# Limitations and Known Issues within MariaDB MaxScale

This document lists known issues and limitations in MariaDB MaxScale and its
plugins. Since limitations are related to specific plugins, this document is
divided into several sections.

[TOC]

## Configuration limitations

In versions 2.1.2 and earlier, the configuration files are limited to 1024
characters per line. This limitation was increased to 16384 characters in
MaxScale 2.1.3. MaxScale 2.3.0 increased this limit to 16777216 characters.

In versions 2.2.12 and earlier, the section names in the configuration files
were limited to 49 characters. This limitation was increased to 1023 characters
in MaxScale 2.2.13.

### Multiple MaxScales on same server

Starting with MaxScale 2.4.0, on systems with Linux kernels 3.9 or newer due to
the addition of SO_REUSEPORT support, it is possible for multiple MaxScale
instances to listen on the same network port if the directories used by both
instances are completely separate and there are no conflicts which can cause
unexpected splitting of connections. This will only happen if users explicitly
tell MaxScale to ignore the default directories and will not happen in normal
use.

## Security limitiations

### MariaDB 10.2

The parser of MaxScale correctly parses `WITH` statements, but fails to
collect columns, functions and tables used in the `SELECT` defining the
`WITH` clause.

Consequently, the database firewall will **not** block `WITH` statements
where the `SELECT` of the `WITH` clause refers to forbidden columns.

## MariaDB Default Values

MaxScale assumes that certain configuration parameters in MariaDB are set to
their default values. These include but are not limited to:

* `autocommit`: Autocommit is enabled for all new connections.
* `tx_read_only`: Transactions use `READ WRITE` permissions by default.

## Query Classification

Follow the [MXS-1350](https://jira.mariadb.org/browse/MXS-1350) Jira issue
to track the progress on this limitation.

XA transactions are not detected as transactions by MaxScale. This means
that all XA commands will be treated as unknown commands and will be
treated as operations that potentially modify the database (in the case of
readwritesplit, the statements are routed to the master).

MaxScale **will not** track the XA transaction state which means that any
SELECT queries done inside an XA transaction can be routed to servers that
are not part of the XA transaction.

This limitation can be avoided on the client side by disabling autocommit
before any XA transactions are done. The following example shows how a
simple XA transaction is done via MaxScale by disabling autocommit for the
duration of the XA transaction.

```
SET autocommit=0;
XA START 'MyXA';
INSERT INTO test.t1 VALUES(1);
XA END 'MyXA';
XA PREPARE 'MyXA';
XA COMMIT 'MyXA';
SET autocommit=1;
```

## Prepared Statements

For its proper functioning, MaxScale needs in general to be aware of the
transaction state and _autocommit_ mode. In order to be that, MaxScale
parses statements going through it.

However, if a transaction is commited or rolled back, or the autocommit
mode is changed using a prepared statement, MaxScale will miss that and its
internal state will be incorrect, until the transaction state or autocommit
mode is changed using an explicit statement.

For instance, after the following sequence of commands, MaxScale will still
think _autocommit_ is on:
```
set autocommit=1
PREPARE hide_autocommit FROM "set autocommit=0"
EXECUTE hide_autocommit
```

To ensure that MaxScale functions properly, do not commit or rollback a
transaction or change the autocommit mode using a prepared statement.

## Protocol limitations

### Limitations with MySQL/MariaDB Protocol support (MariaDBClient)

* Compression is not included in the server handshake.

* If a `KILL [CONNECTION] <ID>` statement is executed, MaxScale will intercept
  it. If the ID matches a MaxScale session ID, it will be closed by sending
  modified `KILL` commands of the same type to all backend server to which the
  session in question is connected to. This results in behavior that is similar
  to how MariaDB does it. If the `KILL CONNECTION USER <user>` form is given,
  all connections with a matching username will be closed instead.

* MariaDB MaxScale does not support `KILL QUERY ID <query_id>` type
  statements. If a query by a query ID is to be killed, it needs to be done
  directly on the backend databases.

* Any `KILL` commands executed using a prepared statement are ignored by
  MaxScale. If any are executed, it is highly likely that the wrong connection
  ends up being killed.

* If a `KILL` connection kills a session that is connected to a readwritesplit
  service that has `transaction_replay` or `delayed_retry` enabled, it is
  possible that the query is retried even if the connection is killed. To avoid
  this, use `KILL QUERY` instead.

* A `KILL` on one service can cause a connection from another service to be
  closed even if it uses a different protocol.

* The change user command (COM_CHANGE_USER) only works with standard
  authentication.

* If a COM_CHANGE_USER succeeds on MaxScale yet fails on the server the session
  ends up in an inconsistent state. This can happen if the password of the
  target user is changed and MaxScale uses old user account data when processing
  the change user. In such a situation, MaxScale and server will disagree on the
  current user. This can affect e.g. reconnections.

## Authenticator limitations

### Limitations in the MySQL authenticator (MariaDBAuth)

* MySQL old style passwords are not supported. MySQL versions 4.1 and newer use
a new authentication protocol which does not support pre-4.1 style passwords.

* When users have different passwords based on the host from which they connect
MariaDB MaxScale is unable to determine which password it should use to connect
to the backend database. This results in failed connections and unusable
usernames in MariaDB MaxScale.

## Filter limitations

### Database Firewall limitations (dbfwfilter)

The Database Firewall filter does not support multi-statements. Using them will
result in an error being sent to the client.

### Tee filter limitations (tee)

The Tee filter does not support binary protocol prepared statements. The
execution of a prepared statements through a service that uses the tee filter is
not guaranteed to succeed on the service where the filter branches to as it does
on the original service.

This possibility exists due to the fact that the binary protocol prepared
statements are identified by a server-generated ID. The ID sent to the client
from the main service is not guaranteed to be the same that is sent by the
branch service.

## Monitor limitations

A server can only be monitored by one monitor. Two or more monitors monitoring
the same server is considered an error.

### Limitations with Galera Cluster Monitoring (galeramon)

The default master selection is based only on MIN(wsrep_local_index). This
can be influenced with the server priority mechanic described in the
[Galera Monitor](../Monitors/Galera-Monitor.md) manual.

## Router limitations

Refer to individual router documentation for a list of their limitations.
