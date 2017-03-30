## IRCd Database

IRCd's database is presented here primarily as a persistent Object store.
In other words, the structure presented by the database can be represented
with JSON. This is built from the primitives of `column`, `row` and `cell`.

#### Columns
While a simple key-value store could naively store a JSON document as a textual
value, we provide additional structure schematized before opening a database:
Every member of a JSON object is a `column` in this database. To address members
within nested objects, we specify a `column` with a "foo.bar.baz" path syntax. This
puts all columns at the same level in our code, even though they may represent
deeply nested values.

#### Rows
Since `columns` are technically independent key-value stores (they have their own
index), when an index key is the same between columns we call this a `row`. For basic
object storage the schema is such that we use the same keys between all columns. For
example, an index would be a username in a user database. The user database itself
takes the form of a single JSON object and any member lookup happens on a user's row.

#### Cells
A `cell` is a single value in a `column` indexed by a key that should be able to form
a `row` between columns. Consider the following near-json expression:

	users["root"] = {"password", "foobar"};

In the users database, we find the `column` "password" and the `row` for "root" and
set that `cell` to "foobar"

Consider these expressions for objects at some depth:

	users["root"] = {"password.plaintext", "foobar"};
	users["root"] = {"password", {"plaintext, "foobar"}};

The column is always found as "password.plaintext". We find it (and can iterate its members
if it were an object) by string-manipulating these full paths which all sit in a single map
and are always open, even if the cell is empty for some row.

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
