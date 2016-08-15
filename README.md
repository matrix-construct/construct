# charybdis

Charybdis is an IRCv3 server designed to be highly scalable. It implements IRCv3.1 and some parts of IRCv3.2.

## building from git

We no longer supply a prebuilt configure script in git, due to use of automake and libtool causing problems.
You will need to run `autogen.sh` to build the autotools files prior to building charybdis.

## feature specific requirements

 * For SSL/TLS client and server connections, one of:

   * OpenSSL 1.0.0 or newer (--enable-openssl)
   * LibreSSL (--enable-openssl)
   * mbedTLS (--enable-mbedtls)
   * GnuTLS (--enable-gnutls)

 * For certificate-based oper CHALLENGE, OpenSSL 1.0.0 or newer.
   (Using CHALLENGE is not recommended for new deployments, so if you want to use a different TLS library,
    feel free.)

 * For ECDHE under OpenSSL, on Solaris and RHEL/Fedora (and its derivatives such as CentOS) you will
   need to compile your own OpenSSL on these systems, as they have removed support for ECC/ECDHE.
   Alternatively, consider using another library (see above).

## platforms

Charybdis is designed with portability in mind, but does not target older systems nor those of solely academic
interest. Operating systems are only supported if they are supported by their vendor.

#### testing

[![SemVer](http://img.shields.io/SemVer/v4.0.0-rc2.png)](https://github.com/charybdis-ircd/charybdis/tree/release/4)
*This branch is testing and does not guarantee stability.*

| <sub> Continuously Integrated Host </sub>   | <sub> Compiler </sub>    | <sub> Third party </sub> | <sub> Status </sub> |
|:------------------------------------------- |:------------------------ |:------------------------ |:------------------- |
| <sub> Linux Ubuntu 12.04 Precise </sub>     | <sub> GCC 4.8 </sub>     |                          | <sub> [![POSIX Build Status](https://travis-ci.org/charybdis-ircd/charybdis.svg?branch=release/4)](https://travis-ci.org/charybdis-ircd/charybdis) </sub> |
| <sub> Linux Ubuntu 12.04 Precise </sub>     | <sub> GCC 4.9 </sub>     |                          | <sub> [![POSIX Build Status](https://travis-ci.org/charybdis-ircd/charybdis.svg?branch=release/4)](https://travis-ci.org/charybdis-ircd/charybdis) </sub> |
| <sub> Linux Ubuntu 12.04 Precise </sub>     | <sub> GCC 5 </sub>       |                          | [![POSIX Build Status](https://travis-ci.org/charybdis-ircd/charybdis.svg?branch=release/4)](https://travis-ci.org/charybdis-ircd/charybdis) |
| <sub> Linux Ubuntu 12.04 Precise </sub>     | <sub> Clang 3.4 </sub>   |                          | [![POSIX Build Status](https://travis-ci.org/charybdis-ircd/charybdis.svg?branch=release/4)](https://travis-ci.org/charybdis-ircd/charybdis) |
| <sub> Apple Darwin 15.5 </sub>              | <sub> LLVM 7.3.0 </sub>  |                          | [![POSIX Build Status](https://travis-ci.org/charybdis-ircd/charybdis.svg?branch=release/4)](https://travis-ci.org/charybdis-ircd/charybdis) |
| <sub> Windows </sub>                        | <sub> mingw 3.5 </sub>   |                          | [![Windows Build Status](https://ci.appveyor.com/api/projects/status/is0obsml8xyq2qk7/branch/release/4?svg=true)](https://ci.appveyor.com/project/kaniini/charybdis/branch/release/4) |

#### stable

[![SemVer](http://img.shields.io/SemVer/v3.5.0.png)](https://github.com/charybdis-ircd/charybdis/tree/release/3.5)

| <sub> Continuously Integrated Host </sub>   | <sub> Compiler </sub>    | <sub> Third party </sub> | <sub> Status </sub> |
|:------------------------------------------- |:------------------------ |:------------------------ |:------------------- |
| <sub> Linux Ubuntu 12.04 Precise </sub>     | <sub> GCC 4.8 </sub>     |                          | [![POSIX Build Status](https://travis-ci.org/charybdis-ircd/charybdis.svg?branch=release/3.5)](https://travis-ci.org/charybdis-ircd/charybdis) |
| <sub> Linux Ubuntu 12.04 Precise </sub>     | <sub> GCC 4.9 </sub>     |                          | [![POSIX Build Status](https://travis-ci.org/charybdis-ircd/charybdis.svg?branch=release/3.5)](https://travis-ci.org/charybdis-ircd/charybdis) |
| <sub> Linux Ubuntu 12.04 Precise </sub>     | <sub> GCC 5 </sub>       |                          | [![POSIX Build Status](https://travis-ci.org/charybdis-ircd/charybdis.svg?branch=release/3.5)](https://travis-ci.org/charybdis-ircd/charybdis) |
| <sub> Linux Ubuntu 12.04 Precise </sub>     | <sub> Clang 3.4 </sub>   |                          | [![POSIX Build Status](https://travis-ci.org/charybdis-ircd/charybdis.svg?branch=release/3.5)](https://travis-ci.org/charybdis-ircd/charybdis) |
| <sub> Apple Darwin 15.5 </sub>              | <sub> LLVM 7.3.0 </sub>  |                          | [![POSIX Build Status](https://travis-ci.org/charybdis-ircd/charybdis.svg?branch=release/3.5)](https://travis-ci.org/charybdis-ircd/charybdis) |
| <sub> Windows </sub>                        | <sub> mingw 3.5 </sub>   |                          | [![Windows Build Status](https://ci.appveyor.com/api/projects/status/is0obsml8xyq2qk7/branch/release/3.5?svg=true)](https://ci.appveyor.com/project/kaniini/charybdis/branch/release/3.5) |

* Tier 1 platforms should always work **when using a release version tag.** If you encounter problems, please file a bug.
	* <sub> FreeBSD 10+ (i386/amd64) </sub>
	* <sub> Mac OS X 10.7+ </sub>
	* <sub> Vista/Server 2008 (x86/x64) </sub>
	* <sub> Linux 2.6 (x86_64/ARM) with glibc or musl </sub>

* Tier 2 platforms should work, but this is not guaranteed. If you find any problems, file a bug, but as these are not regularly tested platforms, a timely resolution may not be possible.
	* <sub> Linux (i386/x86_64) with uClibc </sub>
	* <sub> DragonflyBSD 4.4+ (i386) </sub>
	* <sub> NetBSD 6.1.x+ (i386/amd64) </sub>
	* <sub> OpenBSD 5.6+ (i386/amd64) </sub>
	* <sub> Solaris 10+ (i386) </sub>


#### development

[![Charybdis](http://img.shields.io/SemVer/v5.0.0-dev.png)](https://github.com/charybdis-ircd/charybdis/tree/master)
*This branch is not meant for production. Use at your own risk.*

| <sub> Continuously Integrated Host </sub>   | <sub> Compiler </sub>    | <sub> Third party </sub> | <sub> Status </sub> |
|:------------------------------------------- |:------------------------ |:------------------------ |:------------------- |
| <sub> Linux Ubuntu 12.04 Precise </sub>     | <sub> GCC 4.9     </sub> | <sub> Boost 1.61 </sub>  | [![POSIX Build Status](https://travis-ci.org/charybdis-ircd/charybdis.svg?branch=master)](https://travis-ci.org/charybdis-ircd/charybdis) |
| <sub> Linux Ubuntu 12.04 Precise </sub>     | <sub> GCC 5       </sub> | <sub> Boost 1.61 </sub>  | [![POSIX Build Status](https://travis-ci.org/charybdis-ircd/charybdis.svg?branch=master)](https://travis-ci.org/charybdis-ircd/charybdis) |
| <sub> Linux Ubuntu 12.04 Precise </sub>     | <sub> Clang 3.6   </sub> | <sub> Boost 1.61 </sub>  | [![POSIX Build Status](https://travis-ci.org/charybdis-ircd/charybdis.svg?branch=master)](https://travis-ci.org/charybdis-ircd/charybdis) |
| <sub> Linux Ubuntu 14.04 Trusty </sub>      | <sub> GCC 6       </sub> | <sub> Boost 1.61 </sub>  | [![POSIX Build Status](https://travis-ci.org/charybdis-ircd/charybdis.svg?branch=master)](https://travis-ci.org/charybdis-ircd/charybdis) |
| <sub> Linux Ubuntu 14.04 Trusty </sub>      | <sub> Clang 3.8   </sub> | <sub> Boost 1.61 </sub>  | [![POSIX Build Status](https://travis-ci.org/charybdis-ircd/charybdis.svg?branch=master)](https://travis-ci.org/charybdis-ircd/charybdis) |
| <sub> Apple Darwin 15.5 </sub>              | <sub> LLVM 7.3.0  </sub> | <sub> Boost 1.61 </sub>  | [![POSIX Build Status](https://travis-ci.org/charybdis-ircd/charybdis.svg?branch=master)](https://travis-ci.org/charybdis-ircd/charybdis) |
| <sub> Windows </sub>                        | <sub> mingw 3.5   </sub> | <sub> Boost 1.61 </sub>  | [![Windows Build Status](https://ci.appveyor.com/api/projects/status/is0obsml8xyq2qk7/branch/master?svg=true)](https://ci.appveyor.com/project/kaniini/charybdis/branch/master) |


## platform specific errata

These are known issues and workarounds for supported platforms.

 * **FreeBSD**: if you are compiling with ipv6 you may experience
   problems with ipv4 due to the way the socket code is written.  To
   fix this you must: "sysctl net.inet6.ip6.v6only=0"

 * **Solaris**: you may have to set your PATH to include /usr/gnu/bin and /usr/gnu/sbin before /usr/bin
   and /usr/sbin. Solaris's default tools don't seem to play nicely with the configure script.

## tips

 * To report bugs in charybdis, visit us at irc.freenode.net #charybdis

 * Please read doc/index.txt to get an overview of the current documentation.

 * Read the NEWS file for what's new in this release.

 * The files, /etc/services, /etc/protocols, and /etc/resolv.conf, SHOULD be
   readable by the user running the server in order for ircd to start with
   the correct settings.  If these files are wrong, charybdis will try to use
   127.0.0.1 for a resolver as a last-ditch effort.

## git access

 * The Charybdis GIT repository can be checked out using the following command:
	`git clone https://github.com/charybdis-ircd/charybdis`

 * Charybdis's GIT repository depot can be browsed over the Internet at the following address:
	https://github.com/charybdis-ircd/charybdis
