# Userspace Context Switching

The `ircd::ctx` subsystem is a userspace threading library meant to regress
the asynchronous callback pattern back to synchronous suspensions. This is
essentially a full elaboration of a `setjmp() / longjmp()` between independent
stacks, but justified with modern techniques and comprehensive integration
throughout IRCd.

### Foundation

This library is based in `boost::coroutine / boost::context` which wraps
the register save/restores in a cross-platform way in addition to providing
properly `mmap(NOEXEC)'ed` etc memory appropriate for stacks on each platform.

`boost::asio` has then added its own comprehensive integration with the above
libraries eliminating the need for us to worry about a lot of boilerplate to
de-async the asio networking calls. See: [boost::asio::spawn](http://www.boost.org/doc/libs/1_65_1/boost/asio/spawn.hpp).

This is a nice boost, but that's as far as it goes. The rest is on us here to
actually make a threading library.

### Interface

We mimic the standard library `std::thread` suite as much as possible (which
mimics the `boost::thread` library) and offer alternative threading primitives
for these userspace contexts rather than those for operating system threads in
`std::` such as `ctx::mutex` and `ctx::condition_variable` and `ctx::future`
among others.

* The primary user object is `ircd::context` (or `ircd::ctx::context`) which has
an `std::thread` interface.

### Context Switching

A context switch has the overhead of a heavy function call -- a function with
a bunch of arguments (i.e the registers being saved and restored). We consider
this _fast_ and our philosophy is to not think about the context switch
_itself_ as a bad thing to be avoided for its own sake.

This system is also fully integrated both with the IRCd core
`boost::asio::io_service` event loop and networking systems. There are actually
several types of context switches going on here built on two primitives:

* Direct jump: This is the fastest switch. Context `A` can yield to context `B`
directly if `A` knows about `B` and if it knows that `B` is in a state ready to
resume from a direct jump _and_ that `A` will also be further resumed somehow.
This is not always suitable in practice so other techniques may be used instead.

* Queued wakeup: This is the common default and safe switch. This is where
the context system integrates with the `boost::asio::io_service` event loop.
The execution of a "slice" as we'll call a yield-to-yield run of non-stop
computation is analogous to a function posted to the `io_service` in the
asynchronous pattern. Context `A` can enqueue context `B` if it knows about `B`
and then choose whether to yield or not to yield. In any case the `io_service`
queue will simply continue to the next task which isn't guaranteed to be `B`.
