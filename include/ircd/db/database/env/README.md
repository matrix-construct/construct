This directory contains extremely low-level backend interfaces which allow IRCd to
customize the embedded behavior of RocksDB through its callback and virtual interfaces.

These interfaces are not useful to developers wishing to use the `ircd::db` database
interface. These interfaces are furthermore not useful to developers wishing to add
functionality to the `ircd::db` interface either.

You are now two levels away from the public `ircd::db` interface included in the
standard include stack.
