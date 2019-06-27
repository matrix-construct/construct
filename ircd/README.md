# IRCd Library Definitions

This directory contains definitions and linkage for `libircd`

The purpose of `libircd` is to facilitate the execution of a server which
handles requests from end-users. The library hosts a set of pluggable modules
which may introduce the actual application features (or the "business logic")
of the server.

##### Implied #include <ircd.h>

The `ircd.h` [standard include group](../include/ircd#what-to-include)
is pre-compiled and included *first* by default for every compilation unit in
this directory. Developers do not have to worry about including project headers
in a compilation unit, especially when creating and reorganizing either of them.

Note that because `ircd.h` is include _above_ any manually included header,
there is a theoretical possibility for a conflict. We make a serious effort
to prevent `ircd.h` from introducing pollution outside of our very specific
namespaces (see: [Project Namespaces](../include/ircd#project-namespaces)).

##### Dependency Isolation

Compilation units are primarily divided around the inclusion of a specific
header or third-party dependency which is not involved in the
[ircd.h include group](../include/ircd#what-to-include). Second to that, units
tend to be divided by namespace and the subsystem they're implementing. Units
can further be divided if they become unwieldy, but we bias toward large aggregate
units. Within these large units, there are divisions which group the definitions
by the `include/ircd/` header which declares them.

We do not included third-party headers in our own [headers](../include/ircd)
which are included by others. A developer of an `ircd::` interface can choose
to either forward declare third-party symbols in our headers, or wrap more
complicated functionality.
