## Interface To Remote Servers

This system manages connections and requests to remote servers when this
server plays the role of client. This interface allows its user to make
concurrent requests using the promise/future pattern. It is built on top of
`ircd::ctx::future`, as well as many functions of `ircd::net` and also
uses `ircd::http`.

### API Overview

Developers will be concerned with the classes in `request.h` and the principal
`server::request`.

A construction of the `server::request` class immediately launches a request
to the remote target and processes the response in the background
asynchronously. All connection establishment and management will be handled
internally. The request object is somewhat "thin" and leaves buffer management
for both sending and receiving to the user; buffers must stay valid for the
duration of the request. The only internal buffer management is from a
response-content dynamic-allocation feature which we'll detail later.

An instance is itself a `ctx::future<http::code>` and can be time-`wait()`'ed
on and `get()`'ed; the latter returning a 20x HTTP code on success or throwing
an exception on any failure. By default a failure is any network error _and_
non-20x HTTP status codes. A `request::opts` option can be set to not throw
exceptions for any HTTP code if that behavior is desired instead.

A request can be canceled at any time, and the preferred method is simply
allowing the request to go out of scope and destruct. User buffers supplied
to the request do not have to be maintained any further after cancellation.

### Internal Overview

The `ircd::server` subsystem is *fully asynchronous*. It uses the callback
pattern operating on the main stack without any blocking operations. It is not
built on `ircd::ctx` to achieve maximum scale, servicing tens of thousands of
requests concurrently. It is a relatively small subsystem with a high performance
requirement providing a good motive to optimize this area specifically. Note
that *users of* `ircd::server` are expected to be on an `ircd::ctx` which is
required for the promise/future interface.

#### Structural

- Peer: Represents a remote host (or srv-cluster, or matrix-origin) we want to
communicate with. Instances of peers are held in a single large peers map keyed
by host, and this map comprises the only runtime state of the `ircd::server`
subsystem.

- Link: A single TCP socket to a peer. There may be multiple links to a peer
running concurrent operations.

- Tag: A single HTTP request on a link to a peer. Tags will target a peer and
be scheduled (and rescheduled) in the queue of the best link to a peer, or a
new link may be opened if options allow.
