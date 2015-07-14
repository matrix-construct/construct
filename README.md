# charybdis

Charybdis is a reference implementation of the IRCv3.1 server component.  It is meant to be
used with an IRCv3-capable services implementation such as [Atheme][atheme] or [Anope][anope].

   [atheme]: http://www.atheme.net/
   [anope]: http://www.anope.org/

# necessary requirements

 * A supported platform
 * A working dynamic load library.
 * A working lex.  Solaris /usr/ccs/bin/lex appears to be broken, on this system flex should be used.

# feature specific requirements

 * For SSL Clients, SSL Challenge controlled OPER feature, and encrypted server links,
   a working OpenSSL library or GnuTLS library.  CHALLENGE is not supported on GnuTLS
   yet.

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

 * SOLARIS USERS: this code appears to tickle a bug in older gcc and 
   egcs ONLY on 64-bit Solaris7.  gcc-2.95 and SunPro C on 64bit should
   work fine, and any gcc or SunPro compiled on 32bit.

 * SUPPORTED PLATFORMS: this code should compile without any warnings on:

   * FreeBSD 10
   * Gentoo & Gentoo Hardened ~x86/~amd64/~fbsd
   * RHEL 6 / 7
   * Debian Jessie
   * OpenSuSE 11/12
   * OpenSolaris 2008.x?
   * Solaris 10 sparc.
  
  Please let us know if you find otherwise.  
  It probably does not compile on AIX, IRIX or libc5 Linux.

 * Please read NEWS for information about what is in this release.

 * Other files recommended for reading: BUGS, INSTALL
