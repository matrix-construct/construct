# IRCd Library API

### Getting Around

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
