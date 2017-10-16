# IRCd<sup><img src="http://www.iconj.com/ico/p/s/ps5wyionii.ico" /></sup>

<img align="right" src="https://i.imgur.com/mHGxDyC.png" />

### Internet Relay Chat daemon: *Charybdis*

IRCd is a free and open source server which facilitates real-time communication over the
internet. It was started in 1988 by Jarkko Oikarinen in the University of Oulu and eventually
made its way to William Pitcock et al, whom after 2005 developed the project under the brand
*Charybdis*.

In 2014 a protocol was proposed to reinvigorate real-time communication in lieu of growing
proprietary competition and a lack of innovation from open source alternatives to
compete. This protocol is known as the **Matrix protocol**.

**IRCd now implements the Matrix protocol** using some of the latest techniques available
for modern C++ free software.


# Charybdis/5

Charybdis is designed to be fast and highly scalable, and to be community
developed by volunteer contributors over the internet. This mission strives to
make the software easy to understand, modify, audit, and extend.

Charybdis Five is the first implementation of *Matrix* written in C++. It remains
true to its roots for being highly scalable, modular and having minimal requirements.
Most of the old code has been rewritten but with the same architecture and spirit of
the original.


## Installation


#### Dependencies

**Boost** (1.61 or later) - We have replaced libratbox with the well known and actively
developed Boost libraries. These are included as a submodule in this repository.

**RocksDB** (based on LevelDB) - We replace sqlite3 with a lightweight and embedded database
and have furthered the mission of eliminating the need for external "IRC services"

*Other dependencies:* **sodium** (NaCl crypto), **OpenSSL**, **zlib**, **snappy** (for rocksdb)

*Build dependencies:* **gnu++14 compiler**, **automake**, **autoconf**, **autoconf2.13**,
**autoconf-archive**, **libtool**, **shtool**


#### Downloading Charybdis

`git clone https://github.com/charybdis-ircd/charybdis`
`cd charybdis`
`git checkout 5`
	- Verify you have the latest source tree and **are on the Matrix branch**.


### Building from git (production)

`./autogen.sh`
`./configure`
`make`
`sudo make install`


#### Building from git (DEVELOPER PREVIEW INSTRUCTIONS)

*This is only intended to allow development with dependencies that have not made
their way to mainstream systems yet.* **Not for release.**

The developer preview will install charybdis in a specific directory isolated from the
system. It will avoid using system libraries by downloading and building the dependencies
from the submodules we have pinned here and build them the way we have configured. You may
need to set the `LD_LIBRARY_PATH` to the built libraries and/or maintain an intact build
directory.

`./autogen.sh`
`mkdir build`
- The install directory may be this or another place of your choosing.
- If you decide elsewhere, make sure to change the `--prefix` in the `./configure`
statement below.

`CXX=g++-6 ./configure --prefix=$PWD/build --enable-debug --with-included-boost=shared --with-included-rocksdb=shared`
- Many systems alias `g++` to an older version. To be safe, specify a version manually
in `CXX`. This will also build the submodule dependencies with that version.
- The `--with-included-*` will fetch, configure **and build** the dependencies included
as submodules. Include `=shared` for now until static libraries are better handled.

`make`
`make install`


### Platforms

[![Charybdis](http://img.shields.io/SemVer/v5.0.0-dev.png)](https://github.com/charybdis-ircd/charybdis/tree/master)
*This branch is not meant for production. Use at your own risk.*

| <sub> Continuously Integrated Host </sub>   | <sub> Compiler </sub>    | <sub> Third party </sub> | <sub> Status </sub> |
|:------------------------------------------- |:------------------------ |:------------------------ |:------------------- |
| <sub> Linux Ubuntu 16.04 Xenial </sub>      | <sub> GCC 6       </sub> | <sub> Boost 1.62 </sub>  | [![POSIX Build Status](https://travis-ci.org/charybdis-ircd/charybdis.svg?branch=master)](https://travis-ci.org/charybdis-ircd/charybdis) |
| <sub> Linux Ubuntu 14.04 Xenial </sub>      | <sub> Clang 3.8   </sub> | <sub> Boost 1.62 </sub>  | [![POSIX Build Status](https://travis-ci.org/charybdis-ircd/charybdis.svg?branch=master)](https://travis-ci.org/charybdis-ircd/charybdis) |
| <sub> Apple Darwin 15.5 </sub>              | <sub> LLVM 7.3.0  </sub> | <sub> Boost 1.62 </sub>  | [![POSIX Build Status](https://travis-ci.org/charybdis-ircd/charybdis.svg?branch=master)](https://travis-ci.org/charybdis-ircd/charybdis) |
| <sub> Windows </sub>                        | <sub> mingw 3.5   </sub> | <sub> Boost 1.62 </sub>  | [![Windows Build Status](https://ci.appveyor.com/api/projects/status/is0obsml8xyq2qk7/branch/master?svg=true)](https://ci.appveyor.com/project/kaniini/charybdis/branch/master) |

## Tips

 * Please read doc/index.txt to get an overview of the current documentation.

 * Read the NEWS file for what's new in this release.

## Developers

 * Generate doxygen using `/usr/bin/doxygen tools/doxygen.conf` the target
 directory is doc/html. Browse to doc/html/index.html
