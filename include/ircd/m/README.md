# Matrix Protocol

### Introduction

*The authoritative place for learning about matrix is at [matrix.org](https://matrix.org) but
it may be worthwhile to spend a moment and consider this introduction which explains things
by distilling the formal core of the protocol before introducing all of the networking and
communicative accoutrements...*

#### Identity

The Matrix-ID or `mxid` is a universally unique plain-text string allowing
an entity to be addressed internet-wide which is fundamental to the matrix
federation in contrast to the traditional IRC server/network. An example of an
mxid:  "@user:host" where `host` is a public DNS name, `user` is a party to
`host`, and the '@' character is replaced to convey type information. The
character, called a `sigil`, is defined to be '!' for `room_id` identifiers,
'$' for `event_id` identifiers, '#' for room aliases, and '@' for users.

#### Event

The fundamental primitive of this protocol is the `event` object. This object
contains some set of key/value pairs and the protocol defines a list of such keys
which are meaningful to the protocol. Other keys which are not meaningful to the
protocol can be included directly in the `event` object but there are no guarantees
for if and how a party will pass these keys. To dive right in, here's the list
of recognized keys for an `event`:

```
auth_events, content, depth, event_id, hashes, membership, origin, origin_server_ts, prev_events,
prev_state, room_id, sender, signatures, state_key, type
```

In the event structure, the value for `sender` and `room_id` and `event_id` are
all an `mxid` of the appropriate type.

The `event` object is also the *only* fundamental primitive of the protocol; in other
words: everything is an `event`. All information is conveyed in events, and governed
by rules for proper values behind these keys. The rest of the protocol specification
describes an *abstract state machine* which has its state updated by an event, in
addition to providing a standard means for communication of events between parties
over the internet. That's it.

#### Timeline

The data tape of the matrix machine consists of a singly-linked list of `event`
objects with each referencing the `event_id` of its preceding parent somewhere
in the `prev_` keys; this is called the `timeline`. Each event is signed by its
creator and affirms all events in the chain preceding it. This is a very similar
structure to that used by software like Git, and Bitcoin. It allows looking back
into the past from any point, but doesn't force a party to accept a future and
leaves dispute resolution open-ended (which will be explained later).

#### State

The `state` consists of a subset of events which are accumulated according to a
few rules when playing the tape through the machine. Events which are selected
as `state` will overwrite a matching previously selected `state event` and thus
reduce the number of events in this set to far less than the entire `timeline`.
The `state` is then used to satisfy queries for deciding valid transitions for
the machine. This is like the "work tree" in Git when positioned at some commit.

* Events with a `state_key` are considered state.

* The identity of a `state event` is the concatenation of the `room_id`
value with the `type` value with the `state_key` value. Thus an event
with the same `room_id, type, state_key` replaces an older event in `state`.

* Some `state_key` values are empty strings `""`. This is a convention for
singleton `state` events, like an `m.room.create` event. The `state_key`
is used to represent a set, like with `m.room.member` events, where the
value of the `state_key` is a user `mxid`.

#### Rooms

The `room` structure encapsulates an instance of the matrix machine. A room
consists of a `state` built by a `timeline` of `event` objects. The matrix
protocol specifies certain `event` types which are recognized to affect
the behavior of the `room`. Here's a list of some of those events:

```
m.room.name
m.room.create
m.room.topic
m.room.avatar
m.room.aliases
m.room.canonical_alias
m.room.join_rules
m.room.power_levels
m.room.member
m.room.message
...
```

Some of these events are `state` events and some are ephemeral. These will be
detailed later. All `m.room.*` namespaced events govern the functionality of the
room. Rooms may contain events of any type, but we don't invent new `m.room.*`
type events ourselves. This project tends to create events in the namespace
`ircd.*` These events should not alter the room's functionality for a client
with knowledge of only the published `m.room.*` events wouldn't understand.
