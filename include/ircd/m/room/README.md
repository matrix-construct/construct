## Matrix Room Interface

The headers in this directory as well as `../room.h` comprise the API for
Matrix rooms. These interfaces may conduct IO for both the local database
and the network; many calls may block an ircd::ctx for these purposes.

A room is composed from a timeline of events. We use several key-value
tables as the database for room in addition to the raw event data itself.

- room_events
This is the timeline for room events. We sort the keys of this table by
an event's `depth`. This table allows us to scan the room's events as
a collection. See: `m::room::messages`.

- room_state
This is the present state table for a room. We sort the keys of this table
by `(type,state_key)`. See: `m::room::state`.

- room_joined
This is the present joined members list. It is technically redundant to that
aspect of the room_state table but it is more efficient for us to maintain
a separate table. See: `m::room::origins` which uses this table. Other
interfaces are internally optimized by this table for some calls.

- room_head
This is the collection of forward extremities (unreferenced) for a room. This
is a fast-moving table that would otherwise be just a list in RAM; however
persisting this through the database prevents recalculating it on startup.

- state_node
This is a key-value store of nodes in a b-tree which is how we efficiently
represent the state of a room at any past event. See: `m::state` subsystem.
Note that `m::state` should not be confused with `m::room::state`. The latter
is the user interface to room state which you are probably looking for; the
former is the actual implementation of the b-tree and low-level details.
                    

### Sending & Transmission

The write interface for rooms is aggregated almost entirely in the index
header `../room.h`. These calls all converge on a single function `commit`
which sends a partial event to a room via `m::vm` evaluation.

These calls all take a `room` structure as an argument which will be further
explained in the Reading section. For now know that the `room` argument is
lightweight and trivially constructed from a `room_id`. It can take a pointer
to `m::vm::eval` options which offer extensive control over the transmission
process.

All of the transmitting calls will block an `ircd::ctx` but the extent to
which they do is configurable via the eval options. All of the calls return
an `event::id::buf` of the event which they just committed to the room.

Above the lowest-level `commit()` function there are two mid-level `send()`
suites. One suite creates and sends a state event to the room, the other
creates and sends a non-state event. The overloads are distinguished by an
extra state_key argument for the former.

Above the mid-level `send()` suites there is an accumulation of higher-level
convenience functions, like `message()` and `join()` et al.

### Reading & Access

The rest of these interfaces are read-only interfaces which present aspects of
the room as efficiently as possible.

##### m::room

The principal structure is `m::room` in `room/room.h`. There are no ways to
change an actual room through this interface, but an instance can be used with
calls that do. Instances of `room` are lightweight, maintaining a reference to
a `room_id` and an optional `event_id`. The data behind those references must
stay in scope for the duration of the `room` instance. 

When a `room` instance is used either for reading or writing, the `event_id`
indicates a cursory position in the room to conduct operations. The room will
be represented at that `event_id`. If no `event_id` is specified, the room
will be represented at the latest "present" state; note the present state of
a room is subject to change between calls.

The `room` class interface offers a convenience frontend which brings together
the basic elements of the `room::messages` and `room::state` interfaces. These
have different characteristics.

#### Other interfaces

The remainder of `room/` is comprised of specialized interfaces which operate
efficiently and friendly for their specific purpose.
