This directory contains lower-level interfaces which generally require including RocksDB
for symbols which cannot be forward declared. These are used internally by `ircd/db.cc`
and are not necessary for developers wishing to use `ircd::db`. The public interfaces
to `ircd::db` are one directory up.

The only essential public interface here in the standard include stack is `database.h`
itself. Also note that `ircd::db::database` is typedef'ed to `ircd::database`.
