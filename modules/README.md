# IRCd Module Tree

This directory contains dynamically loadable functionality to libircd. These
modules are not mere extensions -- they provide essential application
functionality, but are not always required to be directly linked in libircd
proper. Most application-specific functionality (i.e "business logic") is
contained in modules within this tree. At the time of this writing, a
significant amount of matrix functionality is still contained within libircd
proper, but it is intended that as many definitions as possible are pushed out
into modules.

#### Layout

The `modules/` directory is primarily shaped the same as the HTTP resource
tree in which most of its modules register themselves in. This is **not**
automatic and the mere inclusion of files/directories in `modules/`
does not automatically expose them over HTTP.

Note that the installation layout is not the same as the development source
layout (i.e in git). Upon installation, the module tree is collapsed into a
single directory and installed into `$prefix/lib/modules/construct/$directory_$module.so`;
this may be subject to improvement.

- `client/` contains resource handlers for the `/_matrix/client/` API
- `federation/` contains resource handlers for the `/_matrix/federation/` API
- `key/` contains resource handlers for the `/_matrix/key/` API
- `media/` contains resource handlers for the `/_matrix/media/` API
- `js/` contains modules specific to the unreleased javascript embedding.
- `s_` modules provide utility to the server's operation.
- `m_` modules implement protocol logic for matrix event types.
- `vm_` modules provide the processing logic for matrix events.

#### Approach

We use a hybrid system to build modules with two techniques:

- Self-contained shared objects loaded for explicit import and export
of their symbols. This is the classical module which is safely loaded, unloaded
and accessed at any time via the `ircd::mods::import` system. This approach is
preferred for the majority of module and extension cases. Definitions in these
modules are not declared in `include/ircd` unless a "thunk" definition inside
libircd contains am `ircd::mods::import` calling out to the module (this is not
generated; developer must place this).

- "core" modules which directly implement symbols declared in `include/ircd`
headers. When this type of module is not loaded those headers declare an
undefined symbol and accesses will immediately crash. This approach is used
when large interfaces are implemented by modules and individual imports
defined in libircd are too cumbersome. This involves global binding made
possible by the dynamic linker. This approach is satisfactory because of
C++ symbol namespacing and modern compiler and linker controls. These modules
*can* still be unloaded (and are during shutdown) but only after every other
module which has accessed its symbols has also been unloaded. At this time,
we have no access or control over that reference system and the reasons for
a failure to unload may not be immediately clear or easy to trace.

#### Loading

Modules may be automatically loaded based on their prefix. Certain prefixes
are sought by libircd for auto-loading, arbitrarily (i.e there is no magic
autoloading prefix; see: `ircd/m/m.cc` for explicit prefixes). A module may
also be loaded when an attempt is made to call into it (i.e with `m::import`
or low-level with `ircd::mods::*`). Furthermore, a module may be loaded by
the console `mod` command suite, specifically `mod load`. Otherwise the
module is not loaded.

#### Unloading

Unlike previous incarnations of IRCd, every module is [eventually] "unloadable"
and the notion of a "core" module is modernized based on the dynamic linker
technology available.

Many central linkages within libircd (and direct links made by other modules)
are imports which tolerate the unavailability of a module by using weak-pointers
and throwing exceptions. Unlike the Atheme approach as well, importing from a
module does not prevent unloading due to dependency; the unloading of a module is
propagated to linksites through the weak_ptr's dependency graph and exceptions
are thrown from the link upon next use.

Furthermore, modules are free to leverage the ircd::ctx system to wait
synchronously for events which are essential for a clean load or unload, even
during static initialization and destruction. This is a useful feature which
allows all modules to be robustly loadable and unloadable at **any time**
and **every time** in an intuitive manner.

No modules used through imports in this system "stick" whether by accident or
on purpose upon unload. Though we all know that dlclose() does not guarantee
a module is actually unloaded, we expend the extra effort to ensure there is
no reason why dlclose() would not unload the module. This static destruction
of a module is verified and alarms will go off if it is actually "stuck" and
the developer *must* fix this.

## Getting started

The header `mods/mapi.h` is specific to modules and included for modules in
addition to the core `ircd.h`. Both of these are included automatically
via the compiler's command-line and the developer does not have to `#include`
either in the module.

Every loadable module requires a static `ircd::mapi::header` with the explicit
name of `IRCD_MODULE`. This is an object which will be sought by the module
loader in libircd. Nothing else is required.

#### Developer's Approach

The ideal module in this system is one that is self-contained in that its only
interface is through "registrations" to various facilities provided by libircd.
The module can still freely read and write to state within libircd or other
modules.

- An example of such is an HTTP resource (see: ircd/resource.h) registration
of a URL to handle incoming HTTP requests.

- Another example is a Matrix Event Hook (see: ircd/m/hook.h) to process
matrix events.

Next is a module providing definitions and making them available through
exposition (i.e `extern "C"`). In this approach, function definitions are
provided in the module but a "central interface" may be provided by libircd.
That central interface is tolerant of the function call failing when the module
is not available (or automatically tries to load it, see: ircd/m/import.h and
ircd/mods/*.h etc).

Note that many modules use any of these approaches at the same time.
