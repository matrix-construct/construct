# This — is The **Construct**

<img align="right" src="https://i.imgur.com/mHGxDyC.png" />

#### Internet Relay Chat daemon: *Matrix Construct*

IRCd was a free and open source server which facilitated real-time communication over the
internet. It was started by Jarkko Oikarinen in 1988 at the University of Oulu and [its
derivatives](https://upload.wikimedia.org/wikipedia/commons/d/d8/IRCd_software_implementations.png)
underpinned the major IRC networks for decades.

Due to its age and stagnation since the mid-2000's, a growing number of proprietary cloud services
are now filling the vacuum of innovation. In 2014 a new approach was proposed to reinvigorate
real-time communication for [free & open source software](https://en.wikipedia.org/wiki/Free_and_open-source_software):
a **federation of networks** known as *the matrix*.

**IRCd has been rewritten to implement the [Matrix Protocol](https://matrix.org/docs/spec/)**.

This is the opportunity to take back control of your communication and relegate the
opaque cloud services to being just another provider to the federation of networks.
Though not syntactically backwards-compatible with the legacy IRC protocol, it is easily
translated as a superset. Similar to the legacy IRC protocol's origins, it wisely leverages
technologies in vogue for its day to aid the virility of implementations. A vibrant and
growing ecosystem [already exists](https://matrix.org/docs/projects/try-matrix-now.html).

**This is the Construct** — the first Matrix server written in C++. It is designed to be
fast and highly scalable, and to be community developed by volunteer contributors over
the internet. This mission strives to make the software easy to understand, modify, audit,
and extend. It remains true to its roots with its modular design and having minimal
requirements. Even though all of the old code has been rewritten, the same spirit and
_philosophy of its predecessors_ is still obvious throughout.


## Installation

Getting up and running with Construct is easy. A deployment can scale from as little as
a low-end virtual machine running a stock linux distribution to a large load balanced
cluster operating in synchrony over a network.


#### Dependencies

- **Boost** (1.61 or later)
Replacing libratbox with the rich and actively developed libraries.

- **RocksDB** (based on LevelDB):
A lightweight and embedded database superseding sqlite3.

- **Sodium** (NaCl crypto):
Provides ed25519 required for the Matrix Federation.

- **OpenSSL** (libssl/libcrypto):
Provides HTTPS TLS / X.509 / etc.

- **snappy** (libsnappy compression):
A dependency of RocksDB.

*Additional dependencies:*

**zlib**, **GNU C++ compiler**, **automake**, **autoconf**, **autoconf2.13**,
**autoconf-archive**, **libtool**, **shtool**

*Upcoming dependencies*:

- **libmozjs** (Optional JavaScript embedding):
The matrix room is directly represented as a javascript object. :art:

- **libpbc** (Pairing Based Cryptography):
Heads up! Heavy items are falling from the ivory tower!

- **libgmp** (Custom Maths):
Experimental Post-Quantum Ideal Lattice Cryptography. :open_mouth:

*Notes*:
- libircd requires a platform capable of loading dynamic shared objects at runtime.


#### Platforms

[![Construct](https://img.shields.io/SemVer/v5.0.0-dev.png)](https://github.com/jevolk/charybdis/tree/master)

| <sub> Continuously Integrated Host </sub>   | <sub> Compiler </sub>    | <sub> Third party </sub> | <sub> Status </sub> |
|:------------------------------------------- |:------------------------ |:------------------------ |:------------------- |
| <sub> Linux Ubuntu 16.04 Xenial </sub>      | <sub> GCC 6       </sub> | <sub> Boost 1.61 </sub>  | [![POSIX Build Status](https://travis-ci.org/jevolk/charybdis.svg?branch=master)](https://travis-ci.org/jevolk/charybdis) |


### Building from git (production)

```
./autogen.sh
./configure
make
sudo make install
```


#### Building from git (DEVELOPER PREVIEW INSTRUCTIONS)

*This is only intended to allow development with dependencies that have not made
their way to mainstream systems yet.* **Not for release.**

The developer preview will install Construct in a specific directory isolated from the
system. It will avoid using system libraries by downloading and building the dependencies
from the submodules we have pinned here and build them the way we have configured. You may
need to set the `LD_LIBRARY_PATH` to the built libraries and/or maintain an intact build
directory.

```
./autogen.sh
mkdir build
```

- The install directory may be this or another place of your choosing.
- If you decide elsewhere, make sure to change the `--prefix` in the `./configure`
statement below.

```
CXX=g++-6 ./configure --prefix=$PWD/build --enable-debug --with-included-boost=shared --with-included-rocksdb=shared
```

- Many systems alias `g++` to an older version. To be safe, specify a version manually
in `CXX`. This will also build the submodule dependencies with that version.
- The `--with-included-*` will fetch, configure **and build** the dependencies included
as submodules. Include `=shared` for now until static libraries are better handled.

```
make
make install
```

## Developers

[![](https://img.shields.io/badge/License-BSD-brightgreen.svg)]() [![](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat-square)]()

 * Generate doxygen using `/usr/bin/doxygen tools/doxygen.conf` the target
 directory is doc/html. Browse to doc/html/index.html

### Plan

The roadmap for service is as follows:

- [x] Phase One: Matrix clients using HTTPS.
- [ ] Phase Two: Legacy IRC networks using TS6 protocol (Atheme Federation).
- [ ] Phase Three: Legacy IRC clients using RFC1459/RFC2812 legacy grammars.

The roadmap for innovation is as follows:

- [x] Phase Zero: **Core libircd**
> Utils; Modules; Userspace contexts; Format strings; JSON; Database; Networking; HTTP; etc...

- [x] Phase One: **Matrix Protocol**
> Core VM; Core modules; Protocol endpoints; Angular/JS client.

- [ ] Phase Two: **Construct Clustering**
> Kademlia sharding of events database; Maymounkov's erasure codes over shards.

- [ ] Phase Three: **Federation Scaling**
> Federation routing techniques; Aggregate signatures from pairing curves;
> All the other pairing gimmicks from Boneh/Bishop/Waters et al;

- [ ] Phase Four: Graduation
> Tromer/Virza's zkSNARK applied to JavaScript XDR evaluation verifying the distributed
> execution of a matrix room using MNT pairing curves.

- [ ] Phase Five: Dissertation
> Phase Two + Phase Four **_with RingLWE_**; GPU accelerated matrix multiplication for
> the number theoretic transform... Secure MultiParty Computation from Somewhat
> Homomorphic Encryption; Other gimmicks from Damgård et al...

- [ ] Phase Six: Habilitation
> Gentry's bootstrapping of Phase Five **_under fully homomorphic encryption_**.


### IRCd Library

 The purpose of `libircd` is to facilitate the execution of a server which
handles requests from end-users. The library hosts a set of pluggable modules
which may introduce the actual application features (or the "business logic")
of the server. These additional modules are found in the `modules/` directory;

This library can be embedded by developers creating their own server or those
who simply want to use the library of routines it provides.

##### libircd can be embedded in your application with very minimal overhead.

Linking to libircd from your executable allows you to customize and extend the
functionality of the server and have control over its execution, or, simply use
library routines provided by the library without any daemonization. Including
libircd headers will not include any other headers beyond those in the standard
library, with minimal impact on your project's compile complexity. The
prototypical embedding of `libircd` is `construct` found in the `construct/`
directory.

##### libircd runs only one server at a time.

Keeping with the spirit of simplicity of the original architecture, `libircd`
continues to be a "singleton" object which uses globals and keeps actual server
state in the library itself. In other words, **only one IRC daemon can exist
within a process's address space at a time.** Whether or not this was a pitfall
of the original design, it has emerged over the decades as a very profitable
decision for making IRCd an accessible open source internet project.

##### libircd is single-threaded✝

The library is based around the `boost::asio::io_service` event loop. It is
still an asynchronous event-based system. We process one event at a time;
developers must not block execution. While the `io_service` can be run safely
on multiple threads by the embedder's application, libircd will use a single
`io_service::strand`.

This methodology ensures there is an uninterrupted execution working through
a single event queue providing service. If there are periods of execution
which are computationally intense like parsing, hashing, cryptography, etc: this
is absorbed in lieu of thread synchronization and bus contention. Scaling this
system is done through running multiple instances which synchronize at the
application level.

✝ However, don't start assuming a truly threadless execution for the entire
address space. If there is ever a long-running background computation or a call
to a 3rd party library which will do IO and block the event loop, we may use an
additional `std::thread` to "offload" such an operation. Thus we do have
a threading model, but it is heterogeneous.

##### libircd introduces userspace threading✝

IRCd presents an interface introducing stackful coroutines, a.k.a. userspace context
switching, or green threads. The library avoids callbacks as the way to break up
execution when waiting for events. Instead, we harken back to the simple old ways
of synchronous programming where control flow and data are easy to follow.

✝ If there are certain cases where we don't want a stack to linger which may
jeopardize the c10k'ness of the daemon the asynchronous pattern is still used.

##### libircd leverages formal grammars

We utilize the `boost::spirit` system of parsing and printing through formal grammars,
rather than writing our own parsers manually. In addition, we build several tools
on top of such formal devices like a type-safe format string library acting as a
drop-in for `::sprintf()`, but accepting objects like `std::string` without `.c_str()`
and prevention of outputting unprintable/unwanted characters that may have been
injected into the system somewhere prior.
