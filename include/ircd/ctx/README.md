## Userspace Context Switching

The `ircd::ctx` subsystem is a userspace threading library meant to regress
the asynchronous callback pattern back to synchronous suspensions. These are
stackful coroutines which provide developers with more intuitive control in
environments which conduct frequent I/O which would otherwise break up a single
asynchronous stack into callback-hell.

### Motivation

Userspace threads are an alternative to using posix kernel threads as a way
to develop intuitively-stackful programs in applications which are primarily
I/O-bound rather than CPU-bound. This is born out of a recognition that a
single CPU core has enough capacity to compute the entirety of all requests for
an efficiently-written network daemon if I/O were instantaneous; if one
*could* use a single thread it is advantageous to do so right up until the
compute-bound is reached, rather than introducing more threads for any other
reason. The limits to single-threading and scaling beyond a single CPU is then
pushed to higher-level application logic: either message-passing between
multiple processes (or machines in a cluster), and/or threads which have
extremely low interference.

`ircd::ctx` allows for a very large number of contexts to exist, on the order
of thousands or more, and still efficiently make progress without the overhead
of kernel context switches. As an anecdotal example, a kernel context switch
from a contended mutex could realistically be five to ten times more costly
than a userspace context switch if not significantly more, and with effects
that are less predictable. Contexts will accomplish as much work as possible
in a "straight line" before yielding to the kernel to wait for the completion
of any I/O event.

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

* There is no preemptive interleaving of contexts. This makes every sequence
of instructions executed a natural transaction requiring no other method of
exclusion. It also allows for introspective conditions, i.e: if context switch
occurred: refresh value, else the old value is good. This is impossible in a
preemptive environment as the result may have changed at every step during and
after the process.

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

The most common context switching encountered and wielded by developers is the
`ctx::dock`, a non-locking condition variable. The power of the dock is in its:

1. Lightweight. It's just a ctx::list; two pointers for a list head, where the
nodes are the contexts themselves participating in the list.

2. Cooperative-condition optimized. When a context is waiting on a condition
it provides, the ctx system can run the function itself to test the condition
without waking up the context.

### When does Context Switching (yielding) occur?

Bottom line is that this is simply not javascript. There are no
stack-clairvoyant keywords like `await` which explicitly indicate to everyone
everywhere that the overall state of the program before and after any totally
benign-looking function call will be different. This is indeed multi-threaded
programming but in a very PG-13 rated way. You have to assume that if you
aren't sure some function has a "deep yield" somewhere way up the stack that
there is a potential for yielding in that function. Unlike real concurrent
threading everything beyond this is much easier.

* Anything directly on your stack is safe (same with real MT anyway).

* `static` and global assets are safe if you can assert no yielding. Such
an assertion can be made with an instance of `ircd::ctx::critical_assertion`.
Note that we try to use `thread_local` rather than `static` to still respect
any real multi-threading that may occur now or in the future.

* Calls which may yield and do IO may be marked with `[GET]` and `[SET]`
conventional labels but they may not be. Some reasoning about obvious yields
and a zen-like awareness is always recommended.
