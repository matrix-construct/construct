# charybdis [![POSIX Build Status](https://travis-ci.org/charybdis-ircd/charybdis.svg?branch=master)](https://travis-ci.org/charybdis-ircd/charybdis) [![Windows Build Status](https://ci.appveyor.com/api/projects/status/is0obsml8xyq2qk7/branch/master?svg=true)](https://ci.appveyor.com/project/kaniini/charybdis/branch/master)

Charybdis is an IRCv3 server designed to be highly scalable.  It implements IRCv3.1 and some parts of IRCv3.2.

It is meant to be used with an IRCv3-capable services implementation such as [Atheme][atheme] or [Anope][anope].

   [atheme]: http://www.atheme.net/
   [anope]: http://www.anope.org/

# necessary requirements

 * A supported platform
 * A working dynamic library system
 * A working lex and yacc - flex and bison should work

# platforms

Charybdis is designed with portability in mind, but does not target older systems nor those of solely academic
interest.

Do note that operating systems are only supported if they are supported by their vendor.

## Tier 1

These platforms are the best supported, and should always work. They are actively tested. If you encounter
problems, please file a bug.

* FreeBSD 10.x and above (i386 and amd64)
* Linux 2.6.x and above with glibc or musl (i386, x86_64, and ARM)
* Mac OS X 10.7 and above
* Windows Vista/Server 2008 and above (x86 or x64)

## Tier 2

These platforms are supported, and most features should work, but this is not guaranteed. If you find any
problems, file a bug, but as these are not regularly tested platforms, a timely resolution may not be
possible.

* DragonflyBSD 4.4 and above (i386)
* Linux with uClibc (i386 or x86_64)
* NetBSD 6.1.x and above (i386, amd64)
* OpenBSD 5.6 and above (i386, amd64)
* Solaris 10 and above (i386)

## Tier 3

These platforms should only be considered weakly supported, as they are either experimental or not actively
tested. These platforms have usually been tested in the past, but they may or may not be in a useful state.
Bugs for tier 3 architectures should have patches attached.

* Solaris 10 and above (sparc64)
* Old operating system versions of tier 2 and above platforms

## Tier 4

Platforms that are tier 4 are not supported at all. They include all platforms not included in tier 3 or
above. Bugs to tier 4 platforms **must** have patches attached or will be rejected, possibly without comment.

# platform specific errata

These are known issues and workarounds for supported platforms.

 * **FreeBSD**: if you are compiling with ipv6 you may experience
   problems with ipv4 due to the way the socket code is written.  To
   fix this you must: "sysctl net.inet6.ip6.v6only=0"

 * **Solaris**: you may have to set your PATH to include /usr/gnu/bin and /usr/gnu/sbin before /usr/bin
   and /usr/sbin. Solaris's default tools don't seem to play nicely with the configure script.

# building from git

We no longer supply a prebuilt configure script in git, due to use of automake and libtool causing problems.
You will need to run `autogen.sh` to build the autotools files prior to building charybdis.

# feature specific requirements

 * For SSL/TLS client and server connections, one of:

   * OpenSSL 1.0 or newer
   * LibreSSL
   * mbedTLS
   * GnuTLS

 * For certificate-based oper CHALLENGE, OpenSSL 1.0 or newer.
   (Using CHALLENGE is not recommended for new deployments, so if you want to use a different TLS library,
    feel free.)

 * For ECDHE, OpenSSL 1.0.0 or newer is required. Solaris; and RHEL/Fedora and its derivatives such as CentOS
   have removed support for ECC/ECDHE. You will need to compile your own OpenSSL on these systems.

# tips

 * To report bugs in charybdis, visit us at irc.freenode.net #charybdis

 * Please read doc/index.txt to get an overview of the current documentation.

 * Read the NEWS file for what's new in this release.

 * The files, /etc/services, /etc/protocols, and /etc/resolv.conf, SHOULD be
   readable by the user running the server in order for ircd to start with
   the correct settings.  If these files are wrong, charybdis will try to use
   127.0.0.1 for a resolver as a last-ditch effort.

