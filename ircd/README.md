# IRCd Library Definitions

This directory contains definitions and linkage for `libircd`

### Overview

`libircd` is designed specifically as a shared object library. The purpose of its
shared'ness is to facilitate IRCd's modular design: IRCd ships with many other
shared objects which introduce the "business logic" and features of the daemon. If
`libircd` was not a shared object, every single module would have to include large
amounts of duplicate code drawn from the static library. This would be a huge drag
on both compilation and the runtime performance.

```
                           (module)   (module)
                               |         |
                               |         |
                               V         V
                             |-------------|
----------------------       |             | < ---- (module)
|                    |       |             |
|  User's executable | <---- |   libircd   |
|                    |       |             |
----------------------       |             | < ---- (module)
                             |-------------|
                               ^         ^
                               |         |
                               |         |
                           (module)   (module)

```

The user (which we may also refer to as the "embedder" elsewhere in
documentation) only deals directly with `libircd` and not the modules.
`libircd` is generally loaded with its symbols bound globally in the executable
and on most platforms cannot be unloaded (or even loaded) manually and has not
been tested to do so. As an aside, we do not summarily dismiss the idea of
reload capability and would like to see it made possible.
