## BUILD

*Most users will need follow the standalone build instructions in the next section.*

```
./autogen.sh
./configure
make
sudo make install
```

### BUILD (standalone)

This section is intended to allow building with dependencies that have not
made their way to mainstream systems. Important notes that may affect you:

- GCC: Ubuntu Xenial (16.04) users must use a PPA to obtain GCC-7 or greater; don't
forget to `export CXX=g++-7` before running `./configure` on that system.

- Boost: The required version is available through `apt` as `boost-all-dev` on
Ubuntu Cosmic (18.10). All earlier releases (including 18.04 LTS) can configure
with `--with-included-boost` as instructed below.

- ~~RocksDB: The required version is available through `apt` as `librocksdb-dev` on
Ubuntu Disco (19.04). All earlier releases (including 18.04 LTS) can configure
with `--with-included-rocksdb` as instructed below.~~

- RocksDB: At this time we advise **all users** including those on 19.04 to
configure with `--with-included-rocksdb` until regressions in your RocksDB
package have been fixed.


#### STANDALONE BUILD PROCEDURE

```
./autogen.sh
mkdir build
```

> The install directory may be this or another place of your choosing. If you decide
elsewhere, make sure to change the `--prefix` in the `./configure` statement below.

```
./configure --prefix=$PWD/build --with-included-boost --with-included-rocksdb
```

> The `--with-included-*` will fetch, configure **and build** the dependencies included
as submodules.

```
make install
```

## Installation Addendum

#### Additional build options

Development builds should follow the same instructions as the standalone
section above while taking note of the following `./configure` options:

##### Debug mode

```
--enable-debug
```
Full debug mode. Includes additional code within `#ifdef RB_DEBUG` sections.
Optimization level is `-Og`, which is still valgrind-worthy. Debugger support
is `-ggdb`. Log level is `DEBUG` (maximum). Assertions are enabled.


##### Release modes (for distribution packages)

Options in this section may help distribution maintainers create packages.
Users building for themselves (whether standalone or fully installed) probably
don't need anything here.

```
--enable-generic
```
Sets `-mtune=generic` as `native` is otherwise the default.


##### Compact mode

```
--enable-compact
```
Create the smallest possible resulting output. This will optimize for size
(if optimization is enabled), remove all debugging, strip symbols, and apply
any toolchain-feature or #ifdef in code that optimizes the output size.

_This feature is experimental. It may not build or execute on all platforms
reliably. Please report bugs._


##### Manually enable assertions

```
--enable-assert
```
Implied by `--enable-debug`. This is useful to specifically enable `assert()`
statements when `--enable-debug` is not used.


##### Manually enable optimization

```
--enable-optimize
```
This manually applies full release-mode optimizations even when using
`--enable-debug`. Implied when not in debug mode.


##### Logging level

```
--with-log-level=
```
This manually sets the level of logging. All log levels at or below this level
will be available. When a log level is not available, all code used to generate
its messages will be entirely eliminated via *dead-code-elimination* at compile
time.

The log levels are (from logger.h):
```
7  DEBUG      Maximum verbosity for developers.
6  DWARNING   A warning but only for developers (more frequent than WARNING).
5  DERROR     An error but only worthy of developers (more frequent than ERROR).
4  INFO       A more frequent message with good news.
3  NOTICE     An infrequent important message with neutral or positive news.
2  WARNING    Non-impacting undesirable behavior user should know about.
1  ERROR      Things that shouldn't happen; user impacted and should know.
0  CRITICAL   Catastrophic/unrecoverable; program is in a compromised state.
```

When `--enable-debug` is used `--with-log-level=DEBUG` is implied. Otherwise
for release mode `--with-log-level=INFO` is implied. Large deployments with
many users may consider lower than `INFO` to maximize optimization and reduce
noise.
