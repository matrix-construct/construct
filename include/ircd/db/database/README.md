This directory contains lower-level interfaces which generally require including RocksDB
for symbols which cannot be forward declared. These are mostly used internally by
`ircd/db.cc` and are not necessary for developers wishing to use `ircd::db`.

The public interfaces to `ircd::db` are one directory up.
