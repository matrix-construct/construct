# IRCd Library API

#### Project Namespaces

* `IRCD_`     Preprocessor #define and macros.
* `RB_`       Build system related preprocessor #defines and macros (legacy).
* `ircd_`     C namespace and demangled bindings.
* `ircd::`    C++ namespace scope.
* `X-IRCd-`   HTTP header key namespace.
* `ircd.`     Matrix event type namespace.
* `ircd_`     Environmental variables (see: conf.h).

#### What To Include

##### libircd headers are organized into several aggregates groups

As a C++ project there are a lot of header files. Header files depend on other
header files. We don't expect the developer of a compilation unit to figure out
an exact list of header files necessary to include for that unit. Instead we
have aggregated groups of header files which are then precompiled. These
aggregations are mostly oriented around a specific project dependency.

> Note: The term 'stack' may be found in place of the preferred term 'group'
in other documentation.

- Standard Include group `<ircd/ircd.h>` is the main header group. This group
involves the standard library and most of libircd. This is what an embedder
will be working with. These headers will expose our own interfaces wrapping
3rd party dependencies which are not included there.

	There are actually two files in play here: `<ircd/ircd.h>` and
	`<ircd/ircd.pic.h>`. The latter is generated dynamically and will not
	exist until `make` creates it. We have to offer two different
	pre-compilations: one with `-fPIC` and one without.

- Boost ASIO include group `<ircd/asio.h>` is a header group exposing the
boost::asio library. We only involve this header in compilation units working
directly with asio for networking et al. Involving this header file slows down
compilation compared with the standard group.

- Boost Spirit include group `<ircd/spirit.h>` is a header group exposing the
spirit parser framework to compilation units which involve formal grammars.
Involving this header is a *monumental* slowdown when compiling.

- JavaScript include group `<ircd/js/js.h>` is a header group exposing symbols
from the SpiderMonkey JS engine. Alternatively, <ircd/js.h> is part of the
standard include group which includes any wrapping to hide SpiderMonkey.

- MAPI include group `<ircd/mapi.h>` is the standard header group for modules.
This group is an extension to the standard include group but has specific
tools for pluggable modules which are not part of the libircd core.

#### Invocation

> `libircd` is **not thread-safe**. It does not protect exposed interfaces with
locks. Embedders must access `libircd` from a single thread.

Initialize the library with a call to `ircd::init()`.
