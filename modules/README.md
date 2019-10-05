# Module Tree

This directory contains dynamically loadable functionality to libircd.

#### Layout

#### Approach

Unlike most of the module systems found in traditional C free software
projects, our approach is oriented around *global symbol* availability to the
address space rather than explicit imports from self-contained modules. This
direction is made viable by C++ and advances in the compiler and linker
toolchains. The result is significantly simpler and more convenient for
developers to work with.

- Modules are loaded with `RTLD_GLOBAL` on both posix and windows platforms.
Use of C++ namespaces, visibility attributes, `STB_GNU_UNIQUE`, etc are
adequate to make this modernization.

- All project code is built to silently weaken undefined symbols. This means
a complicated interface declared in a header, like a class interface with
public and private and static members -- typical in C++ -- can be included
by itself into any part of the project without knowing where the definitions
of that interface are until they are *first used* at runtime. If said
definitions are not available because they are in an unloaded module, a C++
exception is thrown directly from the callsite.

## Getting started

The header `mods/mapi.h` is specific to modules and included for modules in
addition to the core `ircd.h`. Both of these are included automatically
via the compiler's command-line and the developer does not have to `#include`
either in the module.

1. Every loadable module requires a static `ircd::mapi::header` with the explicit
name of `IRCD_MODULE`. This is an object which will be sought by the module
loader in libircd.

```
// Example of a trivial module

ircd::mapi::header
IRCD_MODULE
{
	"My Module", []
	{
		puts("hi\n");
	}
};

```

2. Add an `_la_SOURCES` entry for your module in the appropriate place in
Makefile.am.

3. Add the module `.la` name to the appropriate LTLIBRARIES list
in Makefile.am.
