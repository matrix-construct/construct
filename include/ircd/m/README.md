# Matrix Protocol

## Introduction

*The authoritative place for learning about matrix is at [matrix.org](https://matrix.org) but
it may be worthwhile to spend a moment and consider this introduction which explains things
by distilling the formal core of the protocol before introducing all of the networking and
communicative accoutrements...*

### Identity

The Matrix-ID or `mxid` is a universally unique plain-text string allowing
an entity to be addressed internet-wide which is fundamental to the matrix
federation in contrast to the traditional IRC server/network. An example of an
mxid:  "@user:host" where `host` is a public DNS name, `user` is a party to
`host`, and the '@' character is replaced to convey type information. The
character, called a `sigil`, is defined to be '!' for `room_id` identifiers,
'$' for `event_id` identifiers, '#' for room aliases, and '@' for users.

### Event

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
redacts
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

### Timeline

The data tape of the matrix machine consists of a singly-linked list of `event`
objects with each referencing the `event_id` of its preceding parents somewhere
in the `prev_` keys; this is called the `timeline`. Each event is signed by its
creator and affirms all referenced events preceding it. This is a very similar
structure to that used by software like Git, and Bitcoin. It allows looking back
into the past from any point.

### State

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

### Room

The `room` structure encapsulates an instance of the matrix machine. A room
is a container of `event` objects in the form of a timeline. The query
complexity for information in a room timeline is as follows:

- Ephemeral (non-state) events in the timeline have a linear lookup time:
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


Some of these events are state events and some are ephemeral (these will be
detailed later). All `m.room.*` namespaced events govern the functionality of the
room. Rooms may contain events of any `type`, but we don't invent new `m.room.*`
type events ourselves. This project tends to create events in the namespace
`ircd.*` These events should not alter the room's functionality for a client
with knowledge of only the published `m.room.*` events wouldn't understand.

### Server

Matrix rooms are intended to be distributed entities that are replicated by
all participating servers. It is important to make this clear by contrasting
it with a common assumption that rooms are "hosted" by some entity. For example
when one sees an `mxid` such as `!matrix:matrix.org` it is incorrect to assume
that `matrix.org` is "hosting" this room or that it plays any critical role in
its operation.

Servers are simply obliged to adhere to the rules of the protocol. There is no
cryptographic guarantee that rules will be followed (e.g. zkSNARK), and no
central server to query as an authority. Servers should disregard violations
of the protocol as the room unfolds from the point of its creation.

The notion of hosting does exist for room aliases. In the above example the
room mxid `!matrix:matrix.org` may be referred to by `#matrix:matrix.org`.
As an analogy, if one considers the room mxid to be an IP address then the
alias is like a domain name pointing to the IP address. The example alias is
hosted by `matrix.org` under their authority and may be directed to any room
mxid. The alias is not useful when its host is not available, but the room
itself is still available from all other servers.

### Communication

Servers communicate by broadcasting events to all other servers joined to the
room. When a server wishes to broadcast a message, it constructs an event which
references all previous events in the timeline which have not yet been
referenced. This forms a directed acyclic graph of events, or DAG.

The conversation of messages moves in one direction: past to future. Messages
only reference other messages which have a lower degree of separation indicated
by the `depth` from the first message in the graph (where `type` was
`m.room.create`).

* The monotonic increase in `depth` contributes to an intuitive "light cone"
read coherence. Knowledge of any piece of information (like an event) offers
strongly ordered knowledge of all known information which preceded it at
that point.

* Write consistency is relaxed. Multiple messages may be issued at the same
depth from independent actors and multiple reference trees may form
independent of others. This provides the scalar for performance in a large
distributed internet system.

#### DAG

The DAG is a tool which aids with the presentation of a coherent conversation
for the room in lieu of the relaxed write consistency as previously mentioned.
This tool is not very difficult to comprehend, it only becomes complicated in
the most academically contrived corner-cases -- most of which are born out of
malice.

In the following diagrams each box represents a message and their reference
connections in the timeline, which begins at the top and flows down. For the
sake of simplicity one can consider each message to always be sent by a
different server in the room.

```
Over the lifetime of the average room, for an overwhelming majority of that
time, the DAG is linear.

            [M00]
              |
            [M01]
              |
            [M02]
              |
            [M03]
              |
            [M04]
              |
```

Based on data collected for the Matrix chatroom workload, conflicts
occur about 3.5% of the time, and more than a simple conflict occurs
about 0.1% of the time. We will refer to these as periods of "turbulence."

```
1: 6848
2: 248
3: 7
4: 1
5: 1
6: 0
7: 0
```
Here is some data from #matrix-architecture:matrix.org. The number of
references an event makes is the key, and the count of events which has
made that number of references is the value.

```

            [M00]
              |
            [M01]
           /     \

      [M02]       [M02]

           \     /
            [M03]
              |
            [M04]
              |
```
Above: when two servers transmit at the same time.
Below: when three servers transmit at the same time:

```
            [M00]
              |
            [M01]
          /   |   \

   [M02]    [M02]    [M02]

          \   |   /
            [M03]
              |
            [M04]
              |
```
Below: when three servers transmit, and then a fourth server transmits
having only received two out of the three transmissions in the previous round.
```
            [M00]
              |
            [M01]
          /   |   \

   [M02]    [M02]    [M02]

       \        \     /
        \        [M03]

           \     /
            [M04]
              |
            [M05]
              |
```


```
                  [0]
           [1]           [B]

      [2]                     [A]


    [3]                         [9]


      [4]                     [8]

           [5]           [7]
                  [6]

```

#### Coherence

Keen observers may have realized by now this system is not fully coherent.
To be coherent, a system must leverage *entry consistency* and/or *release
consistency*. Translated to this system:

* *Entry* is the point where an event is created containing references to
all previous events. *Entry consistency* would mean that the knowledge
of all those references is revealed from all parties to the issuer such that
the issuer would not be issuing a conflicting event.

* *Release* is the act of broadcasting that event to other servers. *Release
consistency* would mean that the integration of the newly issued event does not
conflict at the point of acceptance by each and every party.

This system appears to strive for *eventual consistency*. To be pedantic, that
is not a third lemma supplementing the above: it's a higher order composite (like
mutual exclusion, or other algorithms). What this system wants to achieve is a
byzantine tolerance which can be continuously corrected as more information is
learned. This is a *tolerance*, not a *prevention*, because the relaxed write
consistency is of extreme practical importance.

For *eventual consistency* to be coherent, the "seeds" of a correction have to
be planted early on before any fault. When the fault occurs, all deviations
can be corrected toward some single coherent state as each party learns more
information. Once all parties learn all information from the system, there is
no possibility for incoherence. The caveat is that some parties may need to
roll back certain decisions they made without complete information.

Consider the following: `Alice` is a room founder and has one other member
`Bob` who is an op. `Alice` outranks `Bob`. Consider the following scenario:

> 1. `Charlie` joins the room. Now the room has three members. Everyone is
> still in full agreement.
>
> 2. `GNAA` ddos's `Alice` so she can't reach the internet but she can still
> use her server on her LAN.
>
> 3. `Alice` likes `Charlie` so she gives him `+e` or some ban immunity.
>
> 4. `Bob` doesn't like `Charlie` so he bans him.

Now there is a classic byzantine fault. The internet sees a room with two
members `Alice` and `Bob` again while `Alice` sees a room with three: `Alice`, `Bob`
and `Charlie`.

> 5. `GNAA` stops the ddos.

This fault now has to be resolved. This is called "state conflict resolution"
and the matrix specification does not know how to do this. What is currently
specified is that `Alice` and `Bob` can only perform actions that are valid
with the knowledge they had when they performed them. In fact, that was true
in this scenario.

Intuitively, `Alice` needs to dominate the resolution because `Alice` outranks
`Bob`. `Charlie` must not be banned and the room must continue with three
members. Exactly how to roll back the ban and reinstate `Charlie` may seem
obvious but there are practicalities to consider: Perhaps `Alice` is ddosed for
something like a year straight and `Charlie` has entirely given up on socializing
over the internet. A seemingly random and irrelevant correction will be in store
for the room and the effects might be far more complicated.


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

#### Technique

The Matrix room, as described earlier, is a state machine underwritten by
timelines of events in a directed acyclic graph with eventual consistency.
To operate effectively, the state machine must respond to queries about
the state of the room at the point of any event in the past. This is similar
to a `git reset` to some specific commit where one can browse the work tree
as it appeared at that commit.

> Was X a member of room Y when event Z happened?

The naive approach is to trace the graph from the event backward collecting
all of the state events to then satisfy the query. Sequences of specific state
event types can be held by implementations to hasten such a trace.
Alternatively, a complete list of all state events can be created for each
modification to the state to avoid the reference chasing of a trace at the
cost of space.

Our approach is more efficient. We create a b-tree to represent the complete
state to satisfy any query in logarithmic time. When the state is updated,
only one path in the tree is modified and the root is stored with that event.
This structure is actually immutable: the previous versions of the affected
nodes are not discarded allowing past configurations of the tree to be
represented. We further benefit from the fact that each node is referenced by
the hash of its content for efficient reuse, as well as our database being
well compressed.

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
matrix room surface which propagates only one event through at a time. Vertical
tracks are contexts on their journey through each evaluation and exclusion step
to the core.

    Input Events                                            Phase
    ::::::::::::::::::::::::::::::::::::::::::::::::::::::  validation / dupcheck
    ||||||||||||||||||||||||||||||||||||||||||||||||||||||  identity/key resolution
    ||||||||||||||||||||||||||||||||||||||||||||||||||||||  verification
    |||| ||||||||||||||| ||||||||||||||| |||||||||||||||||  head resolution
    --|--|----|-|---|--|--|---|---|---|---------|---|---|-  graph resolutions
    ----------|-|---|---------|-------|-----------------|-  module evaluations
     \          |   |         |       |                  /  post-commit prefetching
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
lock. The highest velocity lock is not held for significant time.

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
