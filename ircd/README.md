# IRCd Library Definitions

This directory contains definitions and linkage for `libircd`

The purpose of `libircd` is to facilitate the execution of a server which
handles requests from end-users. The library hosts a set of pluggable modules
which may introduce the actual application features (or the "business logic")
of the server.

> The executable linking and invoking `libircd` may be referred to as the
"embedding" or just the "executable" interchangably in this documentation.

##### libircd is single-threaded✝

The design of `libircd` is fully-asynchronous, single-thread-oriented. No code
in the library _blocks_ the process. All operations are conducted on top of
a single `boost::asio::io_service` which must be supplied by the executable
linking to `libircd`. That `io_service` must be orchestrated by the executable
at its discretion; typically the embedder's call to `ios.run()` is the only
place the process will _block_.

> Applications are limited by one or more of the following bounds:
- Computing: program is limited by the efficiency of the CPU over time.
- Space: program is limited by the space available for its dataset.
- I/O: program is limited by external events, disks, and networks.

`libircd` is dominated by the **I/O bound**. Its design is heavily optimized
for this assumption with its single-thread orientation. This methodology
ensures there is an _uninterrupted_, _uncontended_, _predictable_ execution
which is easy for developers to reason about intuitively with
sequential-consistency in a cooperative coroutine model.

If there are periods of execution which are computationally intense like
parsing, hashing, cryptography, etc: this is absorbed in lieu of thread
synchronization and bus contention. This system achieves scale through running
multiple independent instances which synchronize at the application-logic
level.

✝ However, don't start assuming a truly threadless execution for the entire
address space. If there is ever a long-running background computation or a call
to a 3rd party library which will do IO and block the event loop, we may use an
additional `std::thread` to "offload" such an operation. Thus we do have
a threading model, but it is heterogeneous.

##### libircd introduces userspace threading✝

IRCd presents an interface introducing stackful coroutines, a.k.a. userspace
context switching, a.k.a. green threads, a.k.a. fibers. The library avoids callbacks
as the way to break up execution when waiting for events. Instead, we harken back
to the simple old ways of synchronous programming where control flow and data are
easy to follow.

✝ If there are certain cases where we don't want a stack to linger which may
jeopardize the c10k'ness of the daemon the asynchronous pattern is still used.

##### libircd can be embedded in your application with very minimal overhead.

Linking to libircd from your executable allows you to customize and extend the
functionality of the server and have control over its execution, or, simply use
library routines provided by the library without any daemonization.

##### libircd runs only one server at a time.

Keeping with the spirit of simplicity of the original architecture, `libircd`
continues to be a "singleton" object which uses globals and keeps actual server
state in the library itself. In other words, **only one IRC daemon can exist
within a process's address space at a time.** Whether or not this was a pitfall
of the original design, it has emerged over the decades as a very profitable
decision for making IRCd an accessible open source internet project.

##### libircd leverages formal grammars

We utilize the `boost::spirit` system of parsing and printing through formal grammars,
rather than writing our own parsers manually. In addition, we build several tools
on top of such formal devices like a type-safe format string library acting as a
drop-in for `::sprintf()`, but accepting objects like `std::string` without `.c_str()`
and prevention of outputting unprintable/unwanted characters that may have been
injected into the system somewhere prior.

### Overview

`libircd` is designed specifically as a shared object library. The purpose of its
shared'ness is to facilitate IRCd's modular design: IRCd ships with many other
shared objects which introduce the "business logic" and features of the daemon. If
`libircd` was not a shared object, every single module would have to include large
amounts of duplicate code drawn from the static library. This would be a huge drag
on both compilation and the runtime performance.

```
                           (module)   (module)
                               |         |
                               |         |
                               V         V
                             |-------------|
----------------------       |             | < ---- (module)
|                    |       |             |
|  User's executable | <---- |   libircd   |
|                    |       |             |
----------------------       |             | < ---- (module)
                             |-------------|
                               ^         ^
                               |         |
                               |         |
                           (module)   (module)

```

The user (which we may also refer to as the "embedder" elsewhere in
documentation) only deals directly with `libircd` and not the modules.
`libircd` is generally loaded with its symbols bound globally in the executable
and on most platforms cannot be unloaded (or even loaded) manually and has not
been tested to do so. As an aside, we do not summarily dismiss the idea of
reload capability and would like to see it made possible.
