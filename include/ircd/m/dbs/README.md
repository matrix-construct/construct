# Database Schema

This system provides local storage for all events and related metadata using
the `events` database. The database is divided into several columns.

Writing to the database must only occur through the single `write()` call,
which operates using transactions. In fact, `write()` itself only builds
transactions and does not actually modify the database until the user commits
the transaction.

Reading from the database can occur more directly by referencing the columns
and using the `ircd::db` API's.

```
Note for casual developers: This is low-level API. It is highly likely what
you are looking for has a real interface somewhere in ircd::m.
```

### Column Overview

There are two categories of columns: Direct event data, and indirect metadata.

The only data stored by the server is in the form of matrix events in rooms.
No arbitrary application data is stored in the database. For example, there
is no "accounts column" storing some user account information; instead this
would be implemented as some matrix event with the type like `ircd.account`
and the `content` containing our arbitrary data.

The only metadata stored in addition to the original event data optimizes and
enhances the original event data. Again, no arbitrary application data is
stored here. Everything stored here helps to facilitate the service of events
inside rooms, for any reasonable purpose, which we then build the application
layer on top of.

#### Direct columns

Direct data consists of original event JSON in addition to several direct
property columns. The fundamental event JSON is stored in `_event_json`.
Select properties from the original JSON are also stored in tuned columns
(see: event_column.h).

The event_column(s) all store duplicate data from the original _event_json
but limited to a single specific property. The index key is an event_idx
(just like _event_json). These columns are useful for various optimizations
at the cost of the additional space consumed.

When conducting a point lookup of an event property with m::get() or with
keys masked in m::event::fetch::opts, these columns handle the query iff
all desired properties can be satisfied from these columns; otherwise
if a property is sought which does not have an active corresponding column
here the _event_json is used transparently to satisfy the query.

Cache and storage details here can be tuned specific to each property. This
makes reading faster and cache footprints more compact, holding much larger
datasets without eviction; in addition to not disrupting the widely shared
_event_json cache during a simple iteration of one property for all events 
on the server, etc.
