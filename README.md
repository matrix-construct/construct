# This — is The **Construct**

[![Chat in #test:zemos.net](https://img.shields.io/matrix/test:zemos.net.svg?label=Chat%20in%20%23test%3Azemos.net&logo=matrix&server_fqdn=matrix.org&style=for-the-badge&color=brightgreen)](https://matrix.to/#/#test:zemos.net) [![](https://img.shields.io/badge/License-BSD-brightgreen.svg?label=%20license&style=for-the-badge&color=brightgreen)]() [![](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?label=contributions&style=for-the-badge&color=brightgreen)]()

The community's own Matrix server. It is designed to be fast and highly scalable,
and to be developed by volunteer contributors over the internet. This mission
strives to make the software easy to understand, modify, audit, and extend.

Matrix is about giving you control over your communication; Construct is about
giving you control over Matrix. Your privacy and security matters. We encourage
you to contribute new ideas and are liberal in accepting experimental features.

## Installation

### Dependencies

- **Boost** library 1.66+
- **RocksDB** library 5.16.6.
- **Sodium** library for curve ed25519.
- **OpenSSL** library for HTTPS TLS / X.509.
- **Magic** library for MIME type recognition.

##### OPTIONAL

- **zlib** or **lz4** or **snappy** database compressions.
- **GraphicsMagick** for media thumbnails.
- **jemalloc** for dynamic memory.

##### BUILD TOOLS

- **GNU C++** compiler, ld.gold, automake, autoconf, autoconf2.13,
autoconf-archive, libtool.

- A platform capable of loading dynamic shared objects at runtime is required.

<!--

#### Platforms

[![Construct](https://img.shields.io/SemVer/v0.0.0-dev.png)](https://github.com/jevolk/charybdis/tree/master)

| <sub> Continuously Integrated Host </sub>   | <sub> Compiler </sub>    | <sub> Third party </sub> | <sub> Status </sub> |
|:------------------------------------------- |:------------------------ |:------------------------ |:------------------- |
| <sub> Linux Ubuntu 16.04 Xenial </sub>      | <sub> GCC 6       </sub> | <sub> Boost 1.66 </sub>  | [![POSIX Build Status](https://travis-ci.org/jevolk/charybdis.svg?branch=master)](https://travis-ci.org/jevolk/charybdis) |
| <sub> Linux Ubuntu 16.04 Xenial </sub>      | <sub> GCC 8       </sub> | <sub> Boost 1.66 </sub>  | [![POSIX Build Status](https://travis-ci.org/jevolk/charybdis.svg?branch=master)](https://travis-ci.org/jevolk/charybdis) |
| <sub> Linux Ubuntu 18.04 Xenial </sub>      | <sub> GCC 6       </sub> | <sub> Boost 1.66 </sub>  | [![POSIX Build Status](https://travis-ci.org/jevolk/charybdis.svg?branch=master)](https://travis-ci.org/jevolk/charybdis) |

-->

### Getting Started

1. At this phase of development the best thing to do is pull the master branch
and use the latest head.

2. See the [BUILD](https://github.com/matrix-construct/construct/wiki/BUILD) instructions to compile Construct from source.

3. See the [SETUP](https://github.com/matrix-construct/construct/wiki/SETUP) instructions to run Construct for the first time.

4. See the [TUNING](https://github.com/matrix-construct/construct/wiki/TUNING) guide to optimize Construct for your deployment.

##### TROUBLESHOOTING

See the [TROUBLESHOOTING](https://github.com/matrix-construct/construct/wiki/Troubleshooting-problems) guide for solutions to possible
problems.

See the [FREQUENTLY ASKED QUESTIONS](https://github.com/matrix-construct/construct/wiki/FAQ) for answers to the most common
perplexities.

## Developers

##### DOCUMENTATION

Generate doxygen using `doxygen ./Doxyfile` the target
directory is `doc/html`. Browse to `doc/html/index.html`.

##### ARCHITECTURE GUIDE

See the [ARCHITECTURE](https://github.com/matrix-construct/construct/wiki/ARCHITECTURE) summary for design choices and
things to know when starting out.

##### DEVELOPMENT STYLE GUIDE

See the [STYLE](https://github.com/matrix-construct/construct/wiki/STYLE) guide for an admittedly tongue-in-cheek lecture on
the development approach.

## Roadmap

##### TECHNOLOGY

- [x] Phase Zero: **Core libircd**: Utils; Modules; Contexts; JSON; Database; HTTP; etc...
- [x] Phase One: **Matrix Protocol**: Core VM; Core modules; Protocol endpoints; etc...
- [ ] Phase Two: **Construct Cluster**: Kademlia sharding of events; Maymounkov's erasure codes.

##### DEPLOYMENT

```
Operating a Construct server which is open to public user registration is unsafe. Local users may
be able to exceed resource limitations and deny service to other users.
```

- [x] **Personal**: Dozens of users. Few default restrictions; higher log output.
- [ ] **Company**: Hundreds of users. Moderate default restrictions.
- [ ] **Public**: Thousands of users. Untrusting configuration defaults.

> Due to the breadth of the Matrix client/server protocol we can only endorse
production use of Construct gradually while local user restrictions are
developed. This notice applies to locally registered users connecting with
clients, it does not apply to federation.
