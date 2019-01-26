## Network Interface

#### open()

The open() call combines DNS resolution, TCP connecting and SSL handshaking.
A dedicated options structure is provided to tweak the specifics of the process.

#### close()

Like the open() sequence, the close() sequence composes a complex of 
multiple operations to close_notify the SSL session with cryptographic
soundness and then disconnect the TCP session. A dedicated options
structure is provided to tweak details.

### Conveyance

*Four* strategies are available to work with the socket. We have it all
covered. The `read()` and `write()` suites contain functions postfixed
by their behavior:

#### _all()

A call to `read_all()` or `write_all()` will yield the calling `ircd::ctx`
until the supplied buffers are filled or transmitted, respectively.

It is strongly advised to set a timeout in the options structure when making
calls of this type.

An `ircd::ctx` is required to make this call, it does not work on the main
stack.

#### _few()

A call to `*_few()` also yields the `ircd::ctx`, but the yield only lasts until
some data is sent or received. Once some activity takes place, as much as
possible is accomplished; there will not be any more yielding.

It is strongly advised to set a timeout in the options structure when making
calls of this type.

An `ircd::ctx` is required to make this call, it does not work on the main
stack.

#### _any()

A call to `*_any()` has true non-blocking behavior. It does not require an
`ircd::ctx`. This call tries to conduct as much IO as possible without
blocking. Internally it may be a loop of the `_one()` strategy that works
through as much of the buffers until error.

#### _one()

A call to `*_one()` also has true non-blocking behavior and does not require
an `ircd::ctx`. This call makes one syscall which may not fill or transmit
all of the user buffers even if the kernel has more capacity to do so. To
maximize the amount of data read or written to kernelland use the `_any()`
strategy.
