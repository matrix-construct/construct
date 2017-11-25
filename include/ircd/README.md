# IRCd Library

This library can be embedded by developers creating their own server or those
who simply want to use the library of routines it provides. See the section for
`Using libircd`.

The purpose of `libircd` is to facilitate the execution of a server which
handles requests from end-users. The library hosts a set of pluggable modules
which may introduce the actual application features (or the "business logic")
of the server. These additional modules are found in the `modules/` directory;

### Things to know about libircd

##### libircd can be embedded in your application with very minimal overhead.

Linking to libircd from your executable allows you to customize and extend the
functionality of the server and have control over its execution, or, simply use
library routines provided by the library without any daemonization. Including
libircd headers will not include any other headers beyond those in the standard
library, with minimal impact on your project's compile complexity. The
prototypical embedding of `libircd` is `construct` found in the `construct/`
directory.

##### libircd runs only one server at a time.

Keeping with the spirit of simplicity of the original architecture, `libircd`
continues to be a "singleton" object which uses globals and keeps actual server
state in the library itself. In other words, **only one IRC daemon can exist
within a process's address space at a time.** Whether or not this was a pitfall
of the original design, it has emerged over the decades as a very profitable
decision for making IRCd an accessible open source internet project.

##### libircd is single-threaded✝

The library is based around the `boost::asio::io_service` event loop. It is
still an asynchronous event-based system. We process one event at a time;
developers must not block execution. While the `io_service` can be run safely
on multiple threads by the embedder's application, libircd will use a single
`io_service::strand`.

This methodology ensures there is an uninterrupted execution working through
a single event queue providing service which is not otherwise computationally
bound. Even if there are periods of execution which are computationally intense
like parsing, hashing, signature verification etc -- this is insignificant
compared to the amortized cost of thread synchronization and bus contention
for a network application.

✝ However, don't start assuming a truly threadless execution. If there is
ever a truly long-running background computation or a call to a 3rd
party library which will do IO and block the event loop, we may use an
additional `std::thread` to "offload" such an operation. Thus we do have
a threading model, but it is heterogeneous.

##### libircd introduces userspace threading✝

IRCd presents an interface introducing stackful coroutines, a.k.a. userspace context
switching, or green threads. The library avoids callbacks as the way to break up
execution when waiting for events. Instead, we harken back to the simple old ways
of synchronous programming where control flow and data are easy to follow.

✝ If there are certain cases where we don't want a stack to linger which may
jeopardize the c10k'ness of the daemon the asynchronous pattern is still used.

##### libircd innovates with formal grammars

We leverage the boost::spirit system of parsing and printing through formal grammars,
rather than writing our own parsers manually. In addition, we build several tools
on top of such formal devices like a type-safe format string library acting as a
drop-in for ::sprintf(), but accepting objects like std::string without .c_str()
and prevention of outputting unprintable/unwanted characters that may have been
injected into the system somewhere prior.

### Developing libircd

##### libircd headers are organized into several aggregate "stacks"

As a C++ project there are a lot of header files. Header files depend on other
header files. We don't expect the developer of a compilation unit to figure out
an exact list of header files necessary to include for that unit. Instead we
have aggregated groups of header files which are then precompiled. These
aggregations are mostly oriented around a specific project dependency.

- Standard Include stack <ircd/ircd.h> is the main header group. This stack
involves the standard library and most of libircd. This is what an embedder
will be working with. These headers will expose our own interfaces wrapping
3rd party dependencies which are not included there. Note that the actual file
to properly include this stack is <ircd/ircd.h> (not stdinc.h).

- Boost ASIO include stack <ircd/asio.h> is a header group exposing the
boost::asio library. We only involve this header in compilation units working
directly with asio for networking et al. Involving this header file slows down
compilation compared with the standard stack.

- Boost Spirit include stack <ircd/spirit.h> is a header group exposing the
spirit parser framework to compilation units which involve formal grammars.
Involving this header is a *monumental* slowdown when compiling.

- JavaScript include stack <ircd/js/js.h> is a header group exposing symbols
from the SpiderMonkey JS engine. Alternatively, <ircd/js.h> is part of the
standard include stack which includes any wrapping to hide SpiderMonkey.

- MAPI include stack <ircd/mapi.h> is the standard header group for modules.
This stack is an extension to the standard include stack but has specific
tools for pluggable modules which are not part of the libircd core.
