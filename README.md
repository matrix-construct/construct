# This — is The **Construct**

<a href="share/webapp">
	<img align="right" src="https://i.imgur.com/TIf8kEC.png">
</a>

It all started in 1988 when Jarkko Oikarinen developed a free and open source server
at the University of Oulu which facilitated real-time communication over the internet.
Its [derivatives](https://upload.wikimedia.org/wikipedia/commons/d/d8/IRCd_software_implementations.png)
have underpinned the major networks for decades.

The protocol has since stagnated, largely abandoned except by evangelists. A
growing number of proprietary cloud services have filled the vacuum of
innovation. Free software projects are relegated to collaborating on non-free
platforms for a basic rich user experience. The consensus is that decentralization
is needed, and users not be limited
to some arcane retro-environment.

<a href="https://github.com/vector-im/riot-web/">
	<img align="right" src="https://i.imgur.com/DUuGSrH.png" />
</a>

**This is the Construct** — Federated team collaboration built with the
[WASM Virtual Machine](https://webassembly.org). Construct makes the chat room programmable.
Applications govern all their basic functions. Everything from the appearance of a room as seen by
users to some continuous-integration running in the background for developers
is customizable by embedding apps around your room.

Clients and servers implement a minimal ABI based on their capabilities while
the community at large builds all features as apps. A variety of apps have
already been created by developers for their own needs. Most choose to make
them available to the community, licensed as free software, so you can import
them and enrich your own rooms.

## Installation

<a href="https://github.com/tulir/gomuks">
	<img align="right" src="https://i.imgur.com/YMUAULE.png" />
</a>

[![Chat in #test:zemos.net](https://img.shields.io/matrix/test:zemos.net.svg?label=Chat%20in%20%23test%3Azemos.net&logo=matrix&server_fqdn=matrix.org&style=for-the-badge&color=brightgreen)](https://matrix.to/#/#test:zemos.net)

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

2. See the [BUILD](doc/BUILD.md) instructions to compile Construct from source.

3. See the [SETUP](doc/SETUP.md) instructions to run Construct for the first time.

##### TROUBLESHOOTING

See the [TROUBLESHOOTING](doc/TROUBLESHOOTING.md) guide for solutions to possible
problems.

## Developers

<a href="https://github.com/mujx/nheko">
	<img align="right" src="https://i.imgur.com/GQ91GOK.png" />
</a>

[![](https://img.shields.io/badge/License-BSD-brightgreen.svg?label=%20license&style=for-the-badge&color=brightgreen)]() [![](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?label=contributions&style=for-the-badge&color=brightgreen)]()

##### DOCUMENTATION

Generate doxygen using `/usr/bin/doxygen tools/doxygen.conf` the target
directory is `doc/html`. Browse to `doc/html/index.html`.

##### ARCHITECTURE GUIDE

See the [ARCHITECTURE](doc/ARCHITECTURE.md) summary for design choices and
things to know when starting out.

##### DEVELOPMENT STYLE GUIDE

See the [STYLE](doc/STYLE.md) guide for an admittedly tongue-in-cheek lecture on
the development approach.

## Roadmap

##### FEATURE EXPERIENCE

- [x] Phase One: **Matrix clients** using HTTPS.
- [ ] Phase Two: Legacy IRC network **TS6 protocol**.

##### TECHNOLOGY

- [x] Phase Zero: **Core libircd**: Utils; Modules; Contexts; JSON; Database; HTTP; etc...
- [x] Phase One: **Matrix Protocol**: Core VM; Core modules; Protocol endpoints; etc...
- [ ] Phase Two: **Construct Cluster**: Kademlia sharding of events; Maymounkov's erasure codes.

##### DEPLOYMENT

```
Operating a Construct server which is open to public user registration is unsafe. Local users may
be able to exceed resource limitations and deny service to other users.
```

- [x] **Personal**: One or few users. Few default restrictions; higher log output.
- [ ] **Company**: Hundreds of users. Moderate default restrictions.
- [ ] **Public**: Thousands of users. Untrusting configuration defaults.

> Due to the breadth of the Matrix client/server protocol we can only endorse
production use of Construct gradually while local user restrictions are
developed. This notice applies to locally registered users connecting with
clients, it does not apply to federation.
