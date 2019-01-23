## IRCd Database

The database here is a strictly schematized key-value grid built from the primitives of
`column`, `cell` and `row`. We use the database as both a flat-object store (matrix
event db) using cells, columns and rows; as well as binary data block store (matrix
media db) using just a single column.

#### Columns
A column is a key-value store. We specify columns in the database descriptors when
opening the database and (though technically somewhat possible) don't change them
during runtime. The database must be opened with the same descriptors in properly
aligned positions every single time. All keys and values in a column can be iterated.
Columns can also be split into "domains" of keys (see: `db/index.h`) based on the
key's prefix, allowing a seek and iteration isolated to just one domain.

In practice we create a column to present a single property in a JSON object. There
is no recursion and we also must know the name of the property in advance to specify
a descriptor for it. Applied, we create a column for each property in the Matrix
event object† and then optimize that column specifically for that property.

```
{
	"origin_server_ts": 15373977384823,
	"content": {"msgtype": "m.text", "body": "hello"}
}

```

For the above example consider two columns: first `origin_server_ts` is optimized for
timestamps by having values which are fixed 8 byte signed integers; next the content
object is stored in whole as JSON text in the content column. Recursion is not yet
supported but theoretically we can create more columns to hold nested properties
if we want further optimizations.

† Note that the application in the example has been further optimized; not
every property in the Matrix event has a dedicated column, and an additional
meta-column is used to store full JSON events (albeit compressed) which is
may be selected to improve performance by making a single larger request
rather than several smaller requests to compose the same data.

#### Rows
Since `columns` are technically independent key-value stores (they have their own
index), when an index key is the same between columns we call this a `row`. In
the Matrix event example, each property of the same event is sought together in a
`row`. A row seek is optimized and the individual cells are queried concurrently and
iterated in lock-step together.

There is a caveat to rows even though the queries to individual cells are
made concurrently: each cell still requires an individual request to the
storage device. The device has a fixed number of total requests it can service
concurrently.

#### Cells
A `cell` is a gratuitious interface representing of a single value in a `column` with
a common key that should be able to form a `row` between columns. A `row` is comprised
of cells.

### Important notes

!!!
The database system is plugged into the userspace context system to facilitate IO. This means
that an expensive database call (mostly on the read side) that has to do disk IO will suspend
your userspace context. Remember that when your userspace context resumes on the other side
of the call, the state of IRCd and even the database itself may have changed. We have a suite
of tools to mitigate this.
!!!

* While the database schema is modifiable at runtime (we can add and remove columns on
the fly) the database is very picky about opening the exact same way it last closed.
This means, for now, we have the full object schema explicitly specified when the DB
is first opened. All columns exist for the lifetime of the DB, whether or not you have
a handle to them.
