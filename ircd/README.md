# IRCd Library Definitions

This directory contains definitions and linkage for `libircd`

The purpose of `libircd` is to facilitate the execution of a server which
handles requests from end-users. The library hosts a set of pluggable modules
which may introduce the actual application features (or the "business logic")
of the server.

> The executable linking and invoking `libircd` may be referred to as the
"embedding" or "user" or "executable" interchangably in this documentation.

## Organization

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

Compilation units are primarily oriented around the inclusion of a specific
dependency which is not involved in the [ircd.h include group](../include/ircd#what-to-include).

For example, the `magic.cc` unit was created to include `<magic.h>`
internally and then provide definitions to our own interfaces in `ircd.h`. We
don't include `<magic.h>` from `ircd.h` nor do we include it from
any other compilation unit. This simply isolates `libmagic` as a dependency.
