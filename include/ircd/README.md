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

### Conventions

These are things you should know when mulling over the code as a whole.
Importantly, knowing these things will help you avoid various gotchas and not
waste your time debugging little surprises. You may or may not agree with some
of these choices (specifically the lack of choices in many cases) but that's
why they're explicitly discussed here.

#### Null termination

- We don't rely on null terminated strings. We always carry around two points
of data to indicate such vectoring. Ideally this is a pair of pointers
indicating the `begin`/`end` like an STL iterator range. `string_view` et al
and the `buffer::` suite work this way.

- Null terminated strings can still be used and we even still create them in
many places on purpose just because we can.

- Null terminated creations use the BSD `strl*` style and *not* the `strn*`
style. Take note of this. When out of buffer space, such an `strl*` style
will *always* add a null to the end of the buffer. Since we almost always
have vectoring data and don't really need this null, a character of the string
may be lost. This can happen when creating a buffer tight to the length of an
expected string without a `+ 1`. This is actually the foundation of a case
to move *back* to `strn*` style but it's not prudent at this time.

- Anything named `print*` like `print(mutable_buffer, T)` always composes null
terminated output into the buffer. These functions usually return a size_t
which count characters printed *not including null*. They may return a
`string_view`/`const_buffer` of that size (never viewing the null).

#### assert() volatility

- Consider any code inside a runtime `assert()` statement to **entirely**
disappear in optimized builds. Some implementations of `assert()` may only
elide the boolean check and thus preserve the inner statement and the effects
of its execution. We do not rely on this. Do not use `assert()` to check
return values of statements that need to be executed in optimized builds.

- Furthermore, consider the **assert statement itself to be physically erased**
from the code during optimized builds. Thus the following is a big mistake:

```
	if(foo)
		assert(!bar);

	if(baz)
		bam();
```
