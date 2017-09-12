# IRCd Library

The purpose of `libircd` is to facilitate the execution of a server which
handles requests from end-users. The library hosts a set of pluggable modules
which introduce the actual application features (or the "business logic") of
the server. These additional modules are found in the `modules/` directory;
see the section for `Developing a module` for more information. This library
can be embedded by developers creating their own server or those who simply
want to use the routines it provides; see the section for `Using libircd`.

### Using libircd

`libircd` can be embedded in your application. This allows you to customize and
extend the functionality of the server and have control over its execution, or,
simply use library routines provided by the library without any daemonization.
The prototypical embedding of `libircd` is `charybdis` found in the `charybdis/`
directory.

Keeping with the spirit of simplicity of the original architecture, `libircd`
continues to be a "singleton" object which uses globals and keeps actual server
state. In other words, only one IRC daemon can exist within a process's address
space at any time. This is actually a profitable design decision for making
IRCd easier to understand for contributors. The original version of this library
was created at the dawn of the era of dynamic shared objects and began as an
abstraction of code from the server executable. This was done so that additional
feature modules could be created while all sharing the same maps of routines.

The library is based around the `boost::asio::io_service` event loop. It is
nominally single threaded and serializes operations on a single asio strand.
In other words, most code is executed on the thread where you call `ios.run()`;
this is referred to as the "main thread." If ios.run() is called on multiple
threads no concurrency will occur. IRCd occasionally uses global and static
variables; the expectation is that these will not be contended outside of the
main thread. The library may spawn additional threads, mostly from 3rd party
libraries and only under daemonization. We don't like this, and try to prevent
it, but it may happen under certain circumstances. These are all dealt with
internally and shouldn't affect the users of the library.


### Developing a module

libircd facilitates the development of dynamic shared modules which implement
specific application logic used in the server.


### Hacking on libircd

#### Style

##### Misc

* When using a `switch` over an `enum` type, put what would be the `default` case after/outside
of the `switch` unless the situation specifically calls for one. We use -Wswitch so changes to
the enum will provide a good warning to update any `switch`.

* Prototypes should name their argument variables to make them easier to understand, except if
such a name is redundant because the type carries enough information to make it obvious. In
other words, if you have a prototype like `foo(const std::string &message)` you should name
`message` because std::string is common and *what* the string is for is otherwise opaque.
OTOH, if you have `foo(const options &, const std::string &message)` one should skip the name
for `options &` as it just adds redundant text to the prototype.
