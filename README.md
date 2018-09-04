# This — is The **Construct**

<img align="right" src="https://i.imgur.com/TIf8kEC.png" />

#### Internet Relay Chat daemon: *Matrix Construct*

IRCd was a free and open source server which facilitated real-time communication over the
internet. It was started by Jarkko Oikarinen in 1988 at the University of Oulu and [its
derivatives](https://upload.wikimedia.org/wikipedia/commons/d/d8/IRCd_software_implementations.png)
underpinned the major IRC networks for decades.

Due to its age and stagnation since the mid-2000's, a growing number of proprietary cloud services
are now filling the vacuum of innovation. In 2014 a new approach was proposed to reinvigorate
real-time communication for free and open source software: a *federation of networks* known as
*the matrix*.

<h4 align="right">
	IRCd has been rewritten for the global federation of networks &nbsp;&nbsp&nbsp;
</h4>

<img align="right" src="https://i.imgur.com/DUuGSrH.png" />

**This is the Construct** — the first Matrix server written in C++. It is designed to be
fast and highly scalable, and to be community developed by volunteer contributors over
the internet. This mission strives to make the software easy to understand, modify, audit,
and extend. It remains true to its roots with its modular design and having minimal
requirements.

Even though all of the old code has been rewritten, the same spirit and
_philosophy of its predecessors_ is still obvious throughout.

Similar to the legacy IRC protocol's origins, Matrix wisely leverages technologies in vogue
for its day to aid the virility of implementations. A vibrant and growing ecosystem
[already exists](https://matrix.org/docs/projects/try-matrix-now.html).


#### Dependencies

- **Boost** (1.66 or later)
Replacing libratbox with the rich and actively developed libraries.

- **RocksDB** (based on LevelDB):
A lightweight and embedded database superseding sqlite3.

- **Sodium** (NaCl crypto):
Provides ed25519 required for the Matrix Federation.

- **OpenSSL** (libssl/libcrypto):
Provides HTTPS TLS / X.509 / etc.

- **GNU C++ compiler**, **automake**, **autoconf**, **autoconf2.13**,
**autoconf-archive**, **libtool**, **shtool**

##### Additional dependencies

- **libmagic** (~Optional~):
Content MIME type recognition.

- **zlib** or **lz4** or **snappy** (Optional):
Provides compression for the database, etc.

*Notes*:
- libircd requires a platform capable of loading dynamic shared objects at runtime.


#### Platforms

[![Construct](https://img.shields.io/SemVer/v0.0.0-dev.png)](https://github.com/jevolk/charybdis/tree/master)

| <sub> Continuously Integrated Host </sub>   | <sub> Compiler </sub>    | <sub> Third party </sub> | <sub> Status </sub> |
|:------------------------------------------- |:------------------------ |:------------------------ |:------------------- |
| <sub> Linux Ubuntu 16.04 Xenial </sub>      | <sub> GCC 6       </sub> | <sub> Boost 1.66 </sub>  | [![POSIX Build Status](https://travis-ci.org/jevolk/charybdis.svg?branch=master)](https://travis-ci.org/jevolk/charybdis) |


## Installation

<a href="https://github.com/tulir/gomuks">
	<img align="right" src="https://i.imgur.com/YMUAULE.png" />
</a>

### BUILD

*Please follow the standalone build instructions in the next section until this
notice is removed.*

```
./autogen.sh
./configure
make
sudo make install
```

### BUILD (standalone)

*Intended to allow building with dependencies that have not made their way
to mainstream systems.*

```
./autogen.sh
mkdir build
```

- The install directory may be this or another place of your choosing.
- If you decide elsewhere, make sure to change the `--prefix` in the `./configure`
statement below.

```
CXX=g++-6 ./configure --prefix=$PWD/build --with-included-boost --with-included-rocksdb
```

- Many systems alias `g++` to an older version. To be safe, specify a version manually
in `CXX`. This will also build the submodule dependencies with that version.
- The `--with-included-*` will fetch, configure **and build** the dependencies included
as submodules.

```
make install
```

### SETUP

- For standalone builds you will need to add the `deps/boost/lib` directory
in your git repo to the library path:
`export LD_LIBRARY_PATH=/path/to/src/deps/boost/lib:$LD_LIBRARY_PATH`

- To ease these instructions we will refer to your server as `host.tld`. For
those familiar with matrix: this is your origin and mxid `@user:host.tld`
hostpart. If your DNS uses `matrix.host.tld` that subdomain is not involved
when we refer to `host.tld` unless we explicitly mention to involve it.


1. Execute
	```
	bin/construct host.tld
	````
	> There is no configuration file.

	> Log messages will appear in terminal concluding with notice `IRCd RUN`.


2. Strike ctrl-c on keyboard
	> The command-line console will appear.


3. Create a general listener socket by entering the following command:
	- If you have existing TLS certificates, replace those parts of the
	command with paths to your certificate and key, respectively. If you
	do not, those files will be created and self-signed in the current
	directory; another target path may be specified.

	```
	net listen matrix 0.0.0.0 8448 host.tld.crt host.tld.crt.key
	```

	> The Matrix Federation Tester should now pass. Browse to
	https://matrix.org/federationtester/api/report?server_name=host.tld and
	verify `"AllChecksOK": true`

4. Relax restrictions for self-signed certificates.
	- Due to the current nature of the federation and the number of
	unsophisticated personal deployments we have little choice but to
	advise this step. We cannot, in good faith, ship this software
	configured insecurely by default; therefor we leave this step to
	you.

	```
	conf set ircd.net.open.allow_self_signed true
	```

5. To use a web-based client like Riot, configure the "webroot" directory
to point at Riot's `webapp/` directory by entering the following:
	```
	conf set ircd.webroot.path /path/to/riot-web/webapp/
	mod reload webroot
	```

6. Browse to `https://host.tld:8448/` and register a user.


## Addendum

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


## Developers

<a href="https://github.com/mujx/nheko">
	<img align="right" src="https://i.imgur.com/GQ91GOK.png" />
	<br />
</a>

[![](https://img.shields.io/badge/License-BSD-brightgreen.svg)]() [![](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat-square)]()

 Generate doxygen using `/usr/bin/doxygen tools/doxygen.conf` the target
 directory is `doc/html`. Browse to `doc/html/index.html`.

## Plan

#### Roadmap for service

- [x] **Phase One**: Matrix clients using HTTPS.
- [ ] **Phase Two**: Legacy IRC network TS6 protocol.
- [ ] **Phase Three**: Legacy IRC clients using RFC1459 / RFC2812 legacy grammars.

#### Roadmap for deployments

The deployment mode is a macro of configuration variables which tune the daemon
for how it is being used. Modes mostly affect aspects of local clients.

- [x] **Personal**: One or few users. Few default restrictions; higher log output.
- [ ] **Company**: Hundreds of users. Moderate default restrictions.
- [ ] **Public**: Thousands of users. Untrusting configuration defaults.

#### Roadmap for innovation

- [x] Phase Zero: **Core libircd**: Utils; Modules; Contexts; JSON; Database; HTTP; etc...
- [x] Phase One: **Matrix Protocol**: Core VM; Core modules; Protocol endpoints; etc...
- [ ] Phase Two: **Construct Cluster**: Kademlia sharding of events; Maymounkov's erasure codes.
