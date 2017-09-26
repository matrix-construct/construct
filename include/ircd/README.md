# IRCd Library

This library can be embedded by developers creating their own server or those
who simply want to use the library of routines it provides. See the section for
`Using libircd`.

The purpose of `libircd` is to facilitate the execution of a server which
handles requests from end-users. The library hosts a set of pluggable modules
which may introduce the actual application features (or the "business logic")
of the server. These additional modules are found in the `modules/` directory;

### Using libircd

##### libircd can be embedded in your application with very minimal overhead.

This allows you to customize and extend the functionality of the server and have
control over its execution, or, simply use library routines provided by the library
without any daemonization. Including libircd headers will not include any other
headers beyond those in the standard library, with minimal impact on your project's
compile complexity. The prototypical embedding of `libircd` is `charybdis` found in
the `charybdis/` directory.

##### libircd runs only one server at a time.

Keeping with the spirit of simplicity of the original architecture, `libircd`
continues to be a "singleton" object which uses globals and keeps actual server
state in the library itself. In other words, **only one IRC daemon can exist
within a process's address space at a time.** This is actually a profitable
design decision for making IRCd easier to understand for contributors.

##### libircd is single-threaded✝

The library is based around the `boost::asio::io_service` event loop. It is still
an asynchronous event-based system. We process one event at a time; developers must
not block execution. Events are never processed concurrently on different threads✝.

However, there are some ✝'s here which must be addressed. We have introduced
additional standard threads to libircd with the purpose of "offloading" operations
from some library dependencies that don't cooperate asynchronously. This ensures the
"main thread" running the actual event loop is never blocked in any case. Furthermore,
some 3rd party dependencies like RocksDB (and boost::asio's DNS resolver) may
introduce threads into the address space which they handle privately.

##### libircd introduces userspace threading

IRCd presents an interface introducing stackful coroutines, a.k.a. userspace context
switching, or green threads. The library does not use callbacks as the way to break
up execution when waiting for events. Instead, we harken back to the simple old ways
of synchronous programming, where control flow and data are easy to follow.

##### libircd innovates with formal grammars

We leverage the boost::spirit system of parsing and printing through formal grammars,
rather than writing our own parsers manually. In addition, we build several tools
on top of such formal devices like a type-safe format string library acting as a
drop-in for ::sprintf(), but accepting objects like std::string without .c_str()
and prevention of outputting unprintable/unwanted characters that may have been
injected into the system somewhere prior.


