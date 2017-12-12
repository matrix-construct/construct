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
auth_events
content
depth
event_id
hashes
membership
origin
origin_server_ts
prev_events
prev_state
room_id
sender
signatures
state_key
type

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
is a container of `event` objects in the form of a timeline. The query
complexity for information in a room timeline is as follows:

- Message (non-state) events in the timeline have a linear lookup time:
the timeline must be iterated in sequence to find a satisfying message.

- State events in the timeline have a logarithmic lookup: the implementation
is expected to maintain a map of the `type`,`state_key` values for events
present in the timeline.

The matrix protocol specifies certain `event` types which are recognized to
affect the behavior of the `room`; here is a list of some types:


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


#### Coherence

Matrix is specified as a directed acyclic graph of messages. The conversation of
messages moves in one direction: past to future. Messages only reference other
messages which have a lower degree of separation (depth) from the first message
in the graph (m.room.create). Specifically, each message makes a reference to all
known messages at the last depth.

* The strong ordering of this system contributes to an intuitive "light cone"
read coherence. Knowledge of any piece of information (like an event) offers
coherent knowledge of all known information which preceded it at that point.

* Write consistency is relaxed. Multiple messages may be issued at the same
depth from independent actors and multiple reference chains may form
independent of others. This provides the scalar for performance in a large
distributed internet system.

* Write incoherence must then be resolved with entry consistency because of the
relaxed release sequence. While parties broadcast all of their new messages,
they make no guarantees for their arrival integration with destinations at
the point of release. This wouldn't be as practical. This means a write which
wishes to be coherent can only use the best available state they have been
made aware of and commit a new message to it.

The system has no other method of resolving incoherence. As a future thought,
some form of release commitment will have to be integrated among at least a
subset of actors for a few important updates to the graph. For example, a
two-phase commit of an important state event *or the re-introduction of
the classic IRC mode change indicating a commitment to change state.*

References to previous events:


```
 [A0] <-- [A1] <-- [A2]       | A has seen B1 and includes a reference in A2
  ^                 |
  |        <---<----<
  |        |
  ^------ [B1] <-- [B2]       | B hasn't yet seen A1 or A2

[T0]  A release A0  :
[T1]  A release A1  :  B acquire A0
[T2]                :  B release B1
[T3]  A acquire B1  :  B release B2
[T4]  A release A2  :
```


Both actors will have their clock (depth) now set to 2 and will issue the
next new message at clock cycle 3 referencing all messages from cycle 2 to
merge the split in the illustration above which is happening.


```
 [A0] <-- [A1] <-- [A2]            [A4]      | A now sees B3, B2, and B1
 ^                 |  |            |
 |        <---<----<  ^--<--<   <--<
 |        |                 |   |
 ^------- [B1] <-- [B2] <-- [B3]             | B now sees A2, A1, and A0
```


### Implementation

#### Model

This system embraces the fact that "everything is an event." It then follows
that everything is a room. We use rooms for both communication and storage of
everything.

There is only one† backend database and it stores events. For example: there
is no "user accounts database" holding all of the user data for the server-
instead there is an `!accounts` *room*. To use these rooms as efficient
databases we categorize a piece of data with an event `type` and key it with
the event `state_key` and the value is the event `content`. Iteration of these
events is also possible. This is now a sufficient key-value store as good as
any other approach; better though, since such a databasing room retains all
features and distributed capabilities of any other room. We then focus our
efforts to optimize the behavior of a room, to the benefit of all rooms, and
all things.

† Under special circumstances other databases may exist but they are purely
slave to the events database: i.e one could `rm -rf` a slave database and it
would be rebuilt from the events database. These databases only exist if an
event is *truly* inappropriate and doesn't fit the model even by a stretch.
An example of this is the search-terms database which specializes in indexing
individual words to the events where they are found so content searches can be
efficient.

#### Flow

This is a single-writer/multiple-reader approach. The "core" is the only writer.
The write itself is just the saving of an event. This serves as a transaction
advancing the state of the machine with effects visible to all future
transactions and external actors.

The core takes the pattern of
`evaluate + exclude -> write commitment -> release sequence`. The single
writer approach means that we resolve all incoherence using exclusion or
reordering or rejection on entry and before any writing and release of the
event. Many ircd::ctx's can orbit the inner core resolving their evaluation
with the tightest exclusion occurring around the write at the inner core.
This also gives us the benefit of a total serialization at this point.

       :::::::
       |||||||      <-- evaluation + rejection
         \|/        <-- evaluation + exclusion / reordering
          !
          *         <-- actor serialized core write commitment
       //|||\\
     //|// \\|\\
    :::::::::::::   <-- release sequence propagation cone

The evaluation phase ensures the event commitment will work: that the event
is valid, and that the event is a valid transition of the machine according
to the rules. This process may take some time and many yields and IO, even
network IO -- if the server lacks a warm cache. During the evaluation phase
locks and exclusions may be acquired to maintain the validity of the
evaluation state through writing at the expense of other contexts contending
for that resource.

> Many ircd::ctx are concurrently working their way through the core. The
> "velocity" is low when an ircd::ctx on this path may yield a lot for various
> IO and allow other events to be processed. The velocity increases when
> concurrent evaluation and reordering is no longer viable to maintain
> coherence. Any yielding of an ircd::ctx at a higher velocity risks stalling
> the whole core.

       :::::::       <-- event input              (low velocity)
       |||||||       <-- evaluation process       (low velocity)
         \|/         <-- serialization process    (higher velocity)

The write commitment saves the event to the database. This is a relatively
fast operation which probably won't even yield the ircd::ctx, and all
future reads to the database will see this write.

          !          <-- serial write commitment  (highest velocity)

The release sequence broadcasts the event so its effects can be consumed.
This works by yielding the ircd::ctx so all consumers can view the event
and apply its effects for their feature module or send the event out to
clients. This is usually faster than it sounds, as the consumers try not to
hold up the release sequence for more than their first execution-slice,
and copy the event if their output rate is slower.

          *         <-- event revelation (higher velocity)
       //|||\\
     //|// \\|\\
    :::::::::::::   <-- release sequence propagation cone (low velocity)

The entire core commitment process relative to an event riding through it
on an ircd::ctx has a duration tolerable for something like a REST interface,
so the response to the user can wait for the commitment to succeed or fail
and properly inform them after.

The core process is then optimized by the following facts:

	* The resource exclusion zone around most matrix events is either
	  small or non-existent because of its relaxed write consistency.

	* Writes in this implementation will not delay.

"Core dilation" is a phenomenon which occurs when large numbers of events
which have relaxed dependence are processed concurrently because none of
them acquire any exclusivity which impede the others.

       :::::::
       |||||||
       |||||||   <-- Core dilation; flow shape optimized for volume.
       |||||||
       /|||||\
      ///|||\\\
     //|/|||\|\\
    :::::::::::::

Close up of the charybdis's write head when tight to one schwarzschild-radius of
matrix room surface which propagates only one event through at a time.
Vertical tracks are contexts on their journey through each evaluation and exclusion
step to the core.

    Input Events                                            Phase
    ::::::::::::::::::::::::::::::::::::::::::::::::::::::  validation / dupcheck
    ||||||||||||||||||||||||||||||||||||||||||||||||||||||  identity/key resolution
    ||||||||||||||||||||||||||||||||||||||||||||||||||||||  verification
    |||| ||||||||||||||| ||||||||||||||| |||||||||||||||||  head resolution
    --|--|----|-|---|--|--|---|---|---|---------|---|---|-  graph resolutions
    ----------|-|---|---------|-------|-----------------|-  module evaluations
     \          |   |         |       |                  /
       ==       ==============|       |               ==    Lowest velocity locks
          \                   |       |             /
            ==                |       |          ==         Mid velocity locks
               \              |       |        /
                 ==           |      /      ==              High velocity locks
                    \         |     /     /
                      ==      =====/=  ==                   Highest velocity lock
                         \       /   /
                          \__   / __/
                             _ | _
                               !                            Write commitment


Above, two contexts are illustrated as contending for the highest velocity
lock. The highest velocity lock is not held for significant time, as the
holder has very little work left to be done within the core, and will
release the lock to the other context quickly. The lower velocity locks
may have to be held longer, but are also less exclusive to all contexts.

                               *                            Singularity
                             [   ]
               /-------------[---]-------------\
            /                :   :                \         Federation send
         /         /---------[---]---------\         \
                 /           :   :           \              Client sync
        out    /      /------[---]------\      \   out
             /       /       :   :       \       \
           /   out  /        |   |        \  out   \
                   /   out   /   \   out   \
                            /     \
                            return
                         | result to |
                         | evaluator |
                         -------------

Above, a close-up of the release sequence. The new event is being "viewed" by
each consumer context separated by the horizontal lines representing a context
switch from the perspective of the event travelling down. Each consumer
performs its task for how to propagate the commissioned event.

Each consumer has a shared-lock of the event which will hold up the completion
of the commitment until all consumers release that. The ideal consumer will only
hold their lock for a single context-slice while they play their part in applying
the event, like non-blocking copies to sockets etc. These consumers then go on
to do the rest of their output without the original event data which was memory
supplied by the evaluator (like an HTTP client). Then all locks acquired on
the entry side of the core can be released. The evaluator then gets the result
of the successful commitment.

#### Scaling

Scaling beyond the limit of a single CPU core can be done with multiple instances
of IRCd which form a cluster of independent actors. This cluster can extend
to other machines on the network too. The independent actors leverage the weak
write consistency and strong ordering of the matrix protocol to scale the same
way the federation scales.

Interference pattern of two IRCd'en:


```
  ::::::::::::::::::::::::::::::::::::
  --------\:::::::/--\:::::::/--------
           |||||||    |||||||
             \|/        \|/
              !          !
              *          *
           //|||\\    //|||\\
         //|// \\|\\//|// \\|\\
        /|/|/|\|\|\/|/|/|\|\|\|\
```
