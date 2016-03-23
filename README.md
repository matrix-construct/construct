# charybdis [![POSIX Build Status](https://travis-ci.org/charybdis-ircd/charybdis.svg?branch=master)](https://travis-ci.org/charybdis-ircd/charybdis) [![Windows Build Status](https://ci.appveyor.com/api/projects/status/is0obsml8xyq2qk7/branch/master?svg=true)](https://ci.appveyor.com/project/kaniini/charybdis/branch/master)

Charybdis is an IRCv3 server designed to be highly scalable.  It implements IRCv3.1 and some parts of IRCv3.2.

It is meant to be used with an IRCv3-capable services implementation such as [Atheme][atheme] or [Anope][anope].

   [atheme]: http://www.atheme.net/
   [anope]: http://www.anope.org/

# necessary requirements

 * A supported platform
 * A working dynamic load library.
 * A working lex.  Solaris /usr/ccs/bin/lex appears to be broken, on this system flex should be used.

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

 * For ECDHE, OpenSSL 1.0.0 or newer is required. RHEL/Fedora and derivatives like CentOS
   will need to compile OpenSSL from source, as ECC/ECDHE-functionality is removed from
   the OpenSSL package in these distributions.

# tips

 * To report bugs in charybdis, visit us at irc.freenode.net #charybdis

 * Please read doc/index.txt to get an overview of the current documentation.

 * The files, /etc/services, /etc/protocols, and /etc/resolv.conf, SHOULD be
   readable by the user running the server in order for ircd to start with
   the correct settings.  If these files are wrong, charybdis will try to use
   127.0.0.1 for a resolver as a last-ditch effort.

 * FREEBSD USERS: if you are compiling with ipv6 you may experience
   problems with ipv4 due to the way the socket code is written.  To
   fix this you must: "sysctl net.inet6.ip6.v6only=0"

 * SOLARIS USERS: you may have to set your PATH to include /usr/gnu/bin before /usr/bin. Solaris versions
   older than 10 are not supported.

 * SUPPORTED PLATFORMS: this code should compile without any warnings on:

   * FreeBSD 10
   * Gentoo & Gentoo Hardened ~x86/~amd64/~fbsd
   * RHEL 6 / 7
   * Debian Jessie
   * OpenSuSE 11/12
   * OpenSolaris 2008.x?
   * Solaris 10 sparc.
   * Solaris 11 x86

  Please let us know if you find otherwise. It may work on other platforms, but this is not guaranteed.

 * Please read NEWS for information about what is in this release.
