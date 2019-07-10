# Client Sync

#### Organization

This directory contains modules which compose the `/sync` endpoint content.

Each module in this directory creates content within some property of the
`/sync` JSON response to fulfill a certain feature (refer to the [matrix
c2s spec](https://matrix.org/docs/spec/client_server/r0.5.0#get-matrix-client-r0-sync)).

`sync::item`'s register themselves at different locations of the `/sync`
response tree and provide two handler functions. The `/sync/` `GET` handler
facilitates an iteration over these modules to build the response.

>  First note that even though this is a directory of modules, the `/sync`
resource itself is a single endpoint and not a directory. The `../sync.cc`
module alone services the `/sync` resource and invokes the modular
functionality in this directory.

> There are at least two ways to organize these modules, and this
directory represents one of those ways. Another way is to disperse them
throughout `/client/` near the endpoints related to their feature suite.
That still remains a viable option for debate as this system further matures.

### Methodology

As documented elsewhere, events processed by the server receive a unique
monotonic sequence integer (or index number, type: `m::event::idx`). The
`next_batch` and `since` tokens are these sequence integers. This single
number is fundamental for the `/sync` to achieve stateless and stable operation:
- Nothing is saved about a request being made. The since token alone has enough
information to fulfill a `/sync` request without hidden server-side state.
- The same request (same since token) produces a similar result each time,
again without any server-side state describing that token.

When a `/sync` request is made, the `since` token is compared with the
server's current sequence number. One of three branches is then taken:

- **Linear**: When the since token's "delta" from the current sequence number
ranges from 0 to some configurable limit (i.e 1024 or 4096), the client enters
_linear sync_: an iteration of events between the `since` token and the current
sequence is made where each event is tested for relevance to the client and
a response is then made. If nothing was found for the client in this iteration,
it falls back to _longpoll sync_ until an event comes through or timeout.

- **Longpoll**: When the since token is 1 greater than the current sequence
number, the client enters _longpoll sync_: It waits for the next appropriate
event which is then sent immediately. The `next_batch` will then be 1 greater
than the sequence number of that event. The implementation of _longpoll sync_
is a specialization of _linear sync_, using the same handlers.

- **Polylog**: When the since token's "delta" exceeds the threshold for a
_linear sync_ the client enters _polylog sync_. This is common when no
`since` token is supplied at all (equal to 0) which is known as an initial
sync. In this mode, a series of point-lookups are made to compose the response
content. This involves iterating the rooms relevant to a user and checking if
the sequence numbers for events in the timeline fall within the `since` window
so they can be included. This requires a lot of random access in contrast to
_linear sync_; each room has to be queried at least once for every invocation
of _polylog sync_. The goal for the threshold between polylog and linear
is to invoke the cheaper mode: Even though polylog usually involves a
minimum of many queries, it is more efficient than a linear iteration of all
events on the server.

### Implementation

Each `/sync` module implements two primary functions:

- A complex composer for a response in _polylog_ mode. This requires the
handler itself to find all the events to include for a satisfying response.
This handler is invoked once for a `since` token during the single pass and
is responsible for building JSON inside the object the handler is responsible
for in the larger `/sync` tree.

- A simple composer for a response in _linear_ mode. This requires the handler
to evaluate the event argument being passed. If the event is to be included
in the `/sync` response, the _linear_ handler builds a complete `/sync` JSON
object tree around this event, which indicates where it is placed in the `/sync`
tree.
