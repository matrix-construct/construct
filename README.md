# This — is The **Construct**

<a href="share/webapp">
	<img align="right" src="https://i.imgur.com/TIf8kEC.png" />
</a>

It all started in 1988 when Jarkko Oikarinen developed a free and open source server
at the University of Oulu which facilitated real-time communication over the internet.
Its [derivatives](https://upload.wikimedia.org/wikipedia/commons/d/d8/IRCd_software_implementations.png)
have underpinned the major IRC networks for decades ever since.

Due to its age and stagnation since the mid-2000's, a growing number of proprietary cloud services
have filled the vacuum of innovation. In 2014 a new approach was proposed to reinvigorate
real-time communication for free and open source software: a *federation of networks*
known as *the matrix*.

<a href="https://github.com/vector-im/riot-web/">
	<img align="right" src="https://i.imgur.com/DUuGSrH.png" />
</a>

**This is the Construct** — the community's own Matrix server. It is designed to be
fast and highly scalable, and to be developed by volunteer contributors over
the internet. This mission strives to make the software easy to understand, modify, audit,
and extend. It remains true to its roots with its modular design and having minimal
requirements. Even though all of the old code has been rewritten, the same spirit and
_philosophy of its predecessors_ is still obvious throughout.

Matrix is about giving you control over your communication; Construct is about
giving you control over Matrix. Whether you are optimizing the entire protocol or
just filling a need only a few others might share: please support the project by
contributing back.

<h3 align="right">
	Join us in <a href="https://matrix.to/#/#test:zemos.net">#test:zemos.net</a>
	/ <a href="https://matrix.to/#/#zemos-test:matrix.org">#zemos-test:matrix.org</a>
</h3>

## Installation

<a href="https://github.com/tulir/gomuks">
	<img align="right" src="https://i.imgur.com/YMUAULE.png" />
</a>

### Dependencies

- **Boost** library 1.66+
- **RocksDB** library 5.16.6.
- **Sodium** library for curve ed25519.
- **OpenSSL** library for HTTPS TLS / X.509.
- **Magic** library for MIME type recognition.

##### Optional

- **zlib** or **lz4** or **snappy** database compressions.
- **GraphicsMagick** for media thumbnails.

##### Build tools

- **GNU C++ compiler**, automake, autoconf, autoconf2.13,
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
