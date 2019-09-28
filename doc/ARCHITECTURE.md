# Architectural Philosophy

### libircd

##### Single-threaded✝

The design of `libircd` is fully-asynchronous, oriented around a single-thread
event-loop. No code in the library _blocks_ the process. All operations are
conducted on top of a single `boost::asio::io_service` which must be supplied
by the executable linking to `libircd`. That `io_service` must be run by the
executable at its discretion; typically the embedder's call to `ios.run()` is
the only place the process will _block_.

The single-threaded approach ensures there is an _uninterrupted_, _uncontended_,
_predictable_ execution which is easy for developers to reason about intuitively
with sequential-consistency. This is ideal for the I/O-bound application being
facilitated. If there are periods of execution which are computationally intense
like parsing, hashing, cryptography, etc: this is absorbed in lieu of thread
synchronization and bus contention.

This system achieves scale through running multiple independent instances which
synchronize at the application-logic level through passing the application's own
messages.

✝ However, do not assume a truly threadless execution for the entire address
space. If there is ever a long-running background computation or a call to a
3rd party library which will block the event loop, we may use an additional
`std::thread` to "offload" such an operation. Thus we do have a threading model,
but it is heterogeneous.

##### Introduces userspace threading

IRCd presents an interface introducing stackful coroutines, a.k.a. userspace
context switching, a.k.a. green threads, a.k.a. fibers. The library avoids
callbacks as the way to break up execution when waiting for events. Instead, we
harken back to the simple old ways of synchronous programming where control
flow and data are easy to follow. If there are certain cases where we don't
want a stack to linger which may jeopardize the c10k'ness of the daemon the
asynchronous pattern is still used (this is a hybrid system).

Consider coroutines like "macro-ops" and asynchronous callbacks like
"micro-ops." The pattern tends to use a coroutine to perform a large and
complex operation which may involve many micro-ops behind the scenes. This
approach relegates the asynchronous callback pattern to simple tasks contained
within specific units which require scale, encapsulating the complexity away
from the rest of the project.

##### Runs only one server at a time

Keeping with the spirit of simplicity of the original architecture, `libircd`
continues to be a "singleton" object which uses globals and keeps actual server
state in the library itself. In other words, **only one IRC daemon can exist
within a process's address space at a time.** Whether or not this was a pitfall
of the original design, it has emerged over the decades as a very profitable
decision for making IRCd an accessible open source internet project.

##### Formal grammars, RTTI, exceptions

We utilize the `boost::spirit` system of parsing and printing through
compile-time formal grammars, rather than writing our own parsers manually.
In addition, we build several tools on top of such formal devices like a
type-safe format string library acting as a drop-in for `::sprintf()`, but
accepting objects like `std::string` without `.c_str()` and prevention of
outputting unprintable/unwanted characters that may have been injected into
the system somewhere prior.
