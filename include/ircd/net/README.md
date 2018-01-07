## IRCd Networking

#### open()

The open() call combines DNS resolution, TCP connecting and SSL handshaking.
A dedicated options structure is provided to tweak the specifics of the process.

#### close()

Like the open() sequence, the close() sequence composes a complex of 
multiple operations to close_notify the SSL session with cryptographic
soundness and then disconnect the TCP session. A dedicated options
structure is provided to tweak details.

#### read()

To keep things simple, this system has no notion of non-blocking or even
asynchronous reads. In other words, you call read() when you know there
is something to be read(). If there is nothing your ircd::ctx will yield
until the call is interrupted/canceled by some timeout etc (which is
something you might want too in certain contexts).

There are two possibilities: either the remote is faster than us or they
are slower than us.

* If the remote is faster than us, or faster than we want: we'll know data
is available but ignore it to slow them down (by not calling read()) --
a matter in which we may not have a choice anyway.

* If the remote is slower than us: you have to use a timer to put a limit
on how much time (and resource) is going to be spent on receiving their
data; eventually the daemon has to move on to serving other clients.

#### write()

This system uses two different techniques for sending data to remotes
intended for two different categories of transmission:

* write_all() is an intuitive synchronous send which yields the ircd::ctx
until all bytes have been written to the remote. This is intended for
serving simple requests or most other ctx-per-client models. A coarse timer
is generally used to prevent the remote from being too slow to receive but
we have some tolerance to wait a little for them.

* write_any()/write_one() is a non-blocking send intended for small message
mass-push models or some other custom flow control on larger messages. The
goal here is to figure out what to do when the socket buffer has filled
up because the remote has been too slow to receive data. The choice is
usually to either propagate the slowdown to the source or to drop the
remote so the daemon can move on without using up memory.

