dnl $Id: configure.ac 2809 2006-12-05 13:18:19Z jilles $
dnl Process this file with autoconf to produce a configure script.

dnl TODO: clean up all the OpenSSL and shared module checking stuff;
dnl the most major changes have already been made and it looks like
dnl said functions need to be just about as complex as they already are.

AC_PREREQ(2.57)

dnl Sneaky way to get an Id tag into the configure script
AC_COPYRIGHT([$Id: configure.ac 2809 2006-12-05 13:18:19Z jilles $])

AC_INIT([charybdis],[2.1.2])

AC_CONFIG_HEADER(include/setup.h)

AC_PREFIX_DEFAULT($HOME/ircd)

AC_GNU_SOURCE

OLD_CFLAGS="$CFLAGS"
dnl Checks for programs.
AC_PROG_CC
AC_LANG(C)

dnl Make sure autoconf doesn't interfere with cflags -jmallett
CFLAGS="$OLD_CFLAGS"

dnl Check for various compilers. -jmallett
dnl But if $CC turns out to be gcc, sure as hell it's, well, gcc. -joshk

if test "$ac_cv_c_compiler_gnu" != yes; then

SGS=no
AC_MSG_CHECKING($CC -version for TenDRA or MIPSpro)
case `$CC -version 2>&1` in
*TenDRA*)
	AC_MSG_RESULT([yes, TenDRA])
	IRC_CFLAGS=""
	CPPFLAGS="$CPPFLAGS -Ylonglong -Yansi -I/usr/include"
	SGS=yes
	TenDRA=yes
;;
*MIPSpro*)
	AC_MSG_RESULT([yes, MIPSpro])
	MIPSpro=yes
	SGS=yes
;;
*)
	AC_MSG_RESULT(no)
	TenDRA=no
	MIPSpro=no
;;
esac

AC_MSG_CHECKING([$CC -V for Sun Workshop, Forte, HPUX or Tru64 cc])
case `$CC -V 2>&1` in
*Sun*WorkShop* | *Forte*Developer*)
	AC_MSG_RESULT(Sun Workshop/Forte)
	IRC_CFLAGS="-fast -xinline=dlinkAdd,dlinkAddBefore,dlinkAddTail,dlinkDelete,dlink_list_length,dlink_node,dlinkMoveList,_MyMalloc,_MyRealloc,_MyFree,_DupString"
	SunWorkShop=yes
	SGS=yes
;;
*Tru64*)
	AC_MSG_RESULT(Tru64 cc)
	IRC_CFLAGS="-O2"
	CPPFLAGS="-I/usr/local/include"
	Tru=yes
;;
*HP*ANSI*)
	AC_MSG_RESULT(HPUX cc)
	HPUX=yes
	IRC_CFLAGS="+e"
;;
*)
	AC_MSG_RESULT(no)
;;
esac

fi

AC_MSG_CHECKING([uname -s for Cygwin, Solaris, AIX or HPUX])
OSNAME=`uname -s`
case "$OSNAME" in
        HP-UX*)
	dnl only do this if we haven't already detected the newer one
	dnl and we're not already using gcc
	
		if test "$HPUX" != yes -a "$ac_cv_c_compiler_gnu" = no; then
        	        AC_MSG_RESULT(assuming old HPUX with its own cc)  
			IRC_CFLAGS="$IRC_CFLAGS +e"
			HPUX=yes
		else
			AC_MSG_RESULT(already using newer HPUX)
		fi
	;;
	CYGWIN*)
		AC_MSG_RESULT(Cygwin)
		CYGWIN=yes
	;;
	SunOS*)
		AC_MSG_RESULT(SunOS or Solaris)
		AC_DEFINE(__EXTENSIONS__, 1, [This is needed to use strtok_r on Solaris.])
		SUN=yes
	;;
	AIX*)
		AC_MSG_RESULT(AIX - Sorry you poor bastard..really we are)
		IRC_CFLAGS="$IRC_CFLAGS -Wl,-brtl -Wl,-G"
	;;
	*)
		AC_MSG_RESULT(no)
	;;
esac

if test "$ac_cv_c_compiler_gnu" = yes; then
	AC_MSG_CHECKING(if $CC is Apple GCC)

	case `$CC -v 2>&1 | tail -n 1` in
	*Apple*)
		AC_MSG_RESULT(yes)
		AppleGCC=yes
	;;
	*)
		AC_MSG_RESULT(no)
		AppleGCC=no
	;;
	esac

	IRC_CFLAGS="$IRC_CFLAGS -O0 -Wall"
fi

dnl If we support -g, use it!
if test "$ac_cv_prog_cc_g" = yes; then
	dnl Tru64 needs -g3 for -O2
	if test "$Tru" = yes; then
		IRC_CFLAGS="$IRC_CFLAGS -g3"
	else
		IRC_CFLAGS="$IRC_CFLAGS -g"
	fi
fi

dnl SVR4 SGS based on what we know about the compiler -jmallett
AC_MSG_CHECKING(if $CC supports the SVR4 SGS interfaces)
if test "$SGS" = "yes"; then
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi

dnl We prefer gcc -MM because it's a lot less bloated
AC_PATH_PROG(MKDEP, mkdep)
AC_PATH_PROG(MAKEDEPEND, makedepend)

AC_MSG_CHECKING(how to generate dependency info)

STDOUT="> .depend"

if test "$ac_cv_c_compiler_gnu" = yes; then
	AC_MSG_RESULT(gcc -MM)
	MKDEP="$CC -MM"
elif test ! -z "$MKDEP"; then
	AC_MSG_RESULT(mkdep)

	dnl Tru64's mkdep is very loud
	if test -z "$Tru"; then
		STDOUT=""
	else
		STDOUT=" 2> /dev/null"
	fi
elif test "$SunWorkShop" = yes; then
	AC_MSG_RESULT($CC -xM)
	MKDEP="$CC -xM"
	STDOUT="> .depend 2> /dev/null"
elif test ! -z "$MAKEDEPEND"; then
	AC_MSG_RESULT(makedepend)
	MKDEP="$MAKEDEPEND -f-"
else
	AC_MSG_RESULT([nothing suitable.. forget it!])
	MKDEP=":"
fi

AC_SUBST(MKDEP)
AC_SUBST(STDOUT)

dnl check for /dev/null so we can use it to hold evil fd's
AC_MSG_CHECKING([for /dev/null])
if test -c /dev/null ; then
	AC_DEFINE(PATH_DEVNULL, "/dev/null", [Path to /dev/null])
	AC_MSG_RESULT(yes)
else
	AC_DEFINE(PATH_DEVNULL, "devnull.log", [Path to /dev/null])
	AC_MSG_RESULT(no - using devnull.log)
fi

dnl jdc -- If CFLAGS is defined, best use it everywhere...
dnl NOTE: jv says it must be added to the *END*, because things like
dnl "gcc -O9 -O2" will result in -O2 getting preference.  How stupid.
if test ! -z "$CFLAGS"; then
	IRC_CFLAGS="$IRC_CFLAGS $CFLAGS"
fi

AC_ISC_POSIX
AC_C_INLINE
AC_PROG_GCC_TRADITIONAL
AC_PROG_MAKE_SET
AC_PROG_INSTALL
AC_PATH_PROG(RM, rm)
AC_PATH_PROG(CP, cp)
AC_PATH_PROG(MV, mv)
AC_PATH_PROG(LN, ln)
AC_PATH_PROG(SED, sed)
AC_PATH_PROG(AR, ar)
AC_PATH_PROG(LD, ld)
AC_PATH_PROG(RANLIB, ranlib)
AC_PATH_PROG(TOUCH, touch)

AC_PROG_YACC

dnl AC_PROG_YACC defaults to yacc unconditionally if nothing can be found
if test "$YACC" = "yacc" -a -z "`which $YACC 2>/dev/null`"; then
	AC_MSG_ERROR([could not locate a suitable parser generator; install bison, yacc, or byacc])
fi

AC_PROG_LEX

if test "$LEX" = ":"; then
	AC_MSG_ERROR([could not locate a suitable lexical generator, install flex or lex.])
fi

dnl use directory structure of cached as default (hack)
if test "$libexecdir" = '${exec_prefix}/libexec' &&
   test "$localstatedir" = '${prefix}/var'; then
	libexecdir='${bindir}'
	localstatedir='${prefix}'
fi

dnl Checks for header files.
AC_HEADER_STDC

AC_CHECK_HEADERS([crypt.h sys/resource.h sys/param.h errno.h sys/syslog.h stddef.h sys/wait.h wait.h sys/epoll.h sys/uio.h machine/endian.h])

dnl Stuff that the memory manager (imalloc) depends on
dnl ==================================================

AC_C_CONST
if test "$ac_cv_header_machine_endian_h" = "no" ; then
    AC_C_BIGENDIAN
fi
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(long long)

dnl Memory manager
dnl ==============

AC_MSG_CHECKING([the system's memory page size])
pagesize="no"
AC_TRY_RUN([
#include <stdio.h>
#if HAVE_UNISTD_H
# include <unistd.h>
#endif

int main(void) {
    FILE *fp = fopen("conftest.malloc", "w");

    if (fp != NULL) {
        fprintf(fp, "%d\n", getpagesize());
        fclose(fp);
    } else
        exit(1);
    exit(0);
}],[
if test -f "conftest.malloc" ; then
    pagesize=`cat conftest.malloc`
fi
])
if test "$pagesize" != "no" ; then
    AC_MSG_RESULT($pagesize)
else
    if test "$ac_cv_sizeof_int" = "4" ; then
        pagesize=4096
    else
        pagesize=8192
    fi
    AC_MSG_RESULT([$pagesize (guessing)])
fi
AC_DEFINE_UNQUOTED(MALLOC_PAGESIZE, $pagesize,
    [the system's memory page size])

dnl Networking Functions
dnl ====================

AC_SEARCH_LIBS(socket, socket, , [AC_MSG_ERROR([You have no socket()! Aborting.])])

dnl SunOS/Solaris required libnsl for inet_ntoa()
if test x"$SUN" = xyes; then
	AC_SEARCH_LIBS(inet_ntoa, nsl,, [AC_MSG_ERROR([libnsl not found! Aborting.])])
fi

AC_CHECK_MEMBER([struct sockaddr.sa_len], [AC_DEFINE(SOCKADDR_IN_HAS_LEN, 1, [Define to 1 if sockaddr has a 'sa_len'
member.])],,[[#include <sys/types.h>
#include <sys/socket.h>
]])

AC_CHECK_TYPE(socklen_t, ,
[AC_DEFINE([socklen_t], [unsigned int],
[If we don't have a real socklen_t, unsigned int is good enough.])],
[#include <sys/types.h>
#include <sys/socket.h>])

AC_ARG_ENABLE(ipv6,
AC_HELP_STRING([--enable-ipv6],[Enable IPv6 support]),[ipv6=$enableval],[ipv6=no])

if test $ipv6 != yes; then
	have_v6="no"
else
AC_MSG_CHECKING([for core IPv6 support])

AC_COMPILE_IFELSE(
[AC_LANG_PROGRAM(
	[[#define IN_AUTOCONF
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>]],
	[[struct sockaddr_in6 s; 
	  s.sin6_family = 0;]]
	)],
[
	if test "$CYGWIN" = "yes"; then
		AC_MSG_RESULT([no, Cygwin's IPv6 is incomplete])
		have_v6=no
	else
		have_v6=yes
		AC_DEFINE(IPV6, 1, [Define if IPv6 support is present and available.])
		AC_MSG_RESULT(yes)
		AC_MSG_CHECKING([for struct in6addr_any])
		AC_COMPILE_IFELSE(
			[AC_LANG_PROGRAM(
				[[#define IN_AUTOCONF
				#include <sys/types.h>
				#include <sys/socket.h>
				#include <netinet/in.h>]],
				[[struct in6_addr a = in6addr_any;]]
			)],
			[AC_MSG_RESULT(yes)],
			[
				AC_MSG_RESULT(no)
				AC_DEFINE(NO_IN6ADDR_ANY, 1, [Define to 1 if your system has no in6addr_any.])
				inet_misc=1
			]
		)
	fi
],
[AC_MSG_RESULT(no)
have_v6="no"])
fi

AC_SEARCH_LIBS(crypt, [crypt descrypt],,)

CRYPT_LIB=$ac_cv_search_crypt

if test "$CRYPT_LIB" = "none required"; then
	unset CRYPT_LIB
elif test "$CRYPT_LIB" = no; then
	unset CRYPT_LIB
fi

AC_SUBST(CRYPT_LIB)

if test "$ac_cv_header_sys_wait_h" = yes -o "$ac_cv_header_wait_h" = yes; then
	VICONF=viconf
	dnl We need one of the above to build viconf. Just a sanity check,
	dnl we don't want to stop people from building the rest of ircd
	dnl just because they can't build viconf.
else
	VICONF=""
fi

AC_SUBST(VICONF)

dnl See whether we can include both string.h and strings.h.
AC_CACHE_CHECK([whether string.h and strings.h may both be included],
gcc_cv_header_string,
[
	AC_COMPILE_IFELSE(
	[#include <string.h>
	#include <strings.h>], 
	[gcc_cv_header_string=yes],
	[gcc_cv_header_string=no])
])

if test "$gcc_cv_header_string" = "yes"; then
	AC_DEFINE(STRING_WITH_STRINGS, 1, [Define to 1 if string.h may be included along with strings.h])
fi

AC_C_BIGENDIAN

dnl Check for stdarg.h - if we can't find it, halt configure
AC_CHECK_HEADER(stdarg.h, , [AC_MSG_ERROR([** stdarg.h could not be found - ircd-ratbox will not compile without it **])])

dnl Checks for the existence of strlcat, strlcpy, basename...
dnl This more reliable test only works with gcc though.

if test "$ac_cv_c_compiler_gnu" = yes; then

AC_MSG_CHECKING(for strlcpy)
save_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS -Wimplicit -Werror"

AC_LINK_IFELSE(
	[AC_LANG_PROGRAM(
		[[#include <string.h>
		#include <stdlib.h>]],
		[[char *a = malloc(6);
		strlcpy(a, "hello", 6);]]
	)],
	[AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_STRLCPY, 1, [Define if strlcpy is available (most BSDs.)])],
	[AC_MSG_RESULT(no)]
)

AC_MSG_CHECKING(for strlcat)
AC_LINK_IFELSE(
	[AC_LANG_PROGRAM(
		[[#include <string.h>
		#include <stdlib.h>]],
		[[char *a = malloc(6);
		a[0] = '\0';
		strlcat(a, "hello", 6);]]
	)],
	[AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_STRLCAT, 1, [Define if strlcat is available (most BSDs.)])],
	[AC_MSG_RESULT(no)]
)

CFLAGS=$save_CFLAGS

else

dnl Better than nothing. The more complicated test above probably fixes powerpc,
dnl so who cares.

AC_CHECK_FUNCS([strlcat strlcpy])

fi

AC_CHECK_TYPE([u_int32_t], [],
[
	AC_CHECK_TYPE([uint32_t],
	[
		AC_DEFINE(u_int32_t, [uint32_t], [If system does not define u_int32_t, define a reasonable substitute.])
	],
	[
		AC_MSG_WARN([system has no u_int32_t or uint32_t, default to unsigned long int])
		AC_DEFINE(u_int32_t, [unsigned long int], [If system does not define u_int32_t, define to unsigned long int here.])
	])
])

AC_CHECK_TYPE([u_int16_t], [],
[
	AC_CHECK_TYPE([uint16_t],
	[
		AC_DEFINE(u_int16_t, [uint16_t], [If system does not define u_int16_t, define a usable substitute])
	],
	[
		AC_MSG_WARN([system has no u_int16_t or uint16_t, default to unsigned short int])
		AC_DEFINE(u_int16_t, [unsigned short int], [If system does not define u_int16_t, define a usable substitute.])
	])
])

AC_CHECK_TYPE([in_port_t], [],
[AC_DEFINE(in_port_t, [u_int16_t], [If system does not define in_port_t, define it to what it should be.])],
[[#include <sys/types.h>
#include <netinet/in.h>]])

AC_CHECK_TYPE([sa_family_t], [],
[AC_DEFINE(sa_family_t, [u_int16_t], [If system does not define sa_family_t, define it here.])],
[[#include <sys/types.h>
#include <sys/socket.h>]])

AC_CHECK_TYPES([uintptr_t])

dnl check for various functions...
AC_CHECK_FUNCS([socketpair vsnprintf mmap gettimeofday strdup strndup ])

AC_FUNC_ALLOCA

dnl Specialized functions checks
dnl ============================

dnl check for nanosleep          
AC_CHECK_FUNC(nanosleep,,[AC_CHECK_LIB(rt,nanosleep,
                 LIBS="${LIBS} -lrt",
                 [AC_CHECK_LIB(posix4,nanosleep, LIBS="${LIBS} -lposix4"
                 )])])
if test x$ac_cv_func_nanosleep = xno && test x$ac_cv_lib_posix4_nanosleep = xno  && test x$ac_cv_lib_rt_nanosleep = xno
then     
         AC_MSG_RESULT("nanosleep not found..using select for delay")
else     
	 AC_DEFINE([HAVE_NANOSLEEP], 1, [Define if nanosleep exists])
fi

dnl OpenSSL support
AC_MSG_CHECKING(for OpenSSL)
AC_ARG_ENABLE(openssl,
[AC_HELP_STRING([--enable-openssl[=DIR]],[Enable OpenSSL support (DIR optional).])
AC_HELP_STRING([--disable-openssl],[Disable OpenSSL support.])],
[cf_enable_openssl=$enableval],
[cf_enable_openssl="auto"])

if test "$cf_enable_openssl" != "no" ; then
	cf_openssl_basedir=""
	if test "$cf_enable_openssl" != "auto" &&
	test "$cf_enable_openssl" != "yes" ; then
		dnl Support for --enable-openssl=/some/place
		cf_openssl_basedir="`echo ${cf_enable_openssl} | sed 's/\/$//'`"
	else
	dnl Do the auto-probe here.  Check some common directory paths.
		for dirs in /usr/local/ssl /usr/pkg /usr/local \
		/usr/local/openssl ; do
			if test -f "${dirs}/include/openssl/opensslv.h" ; then
				cf_openssl_basedir="${dirs}"
			break
			fi
		done
		unset dirs
	fi
	dnl Now check cf_openssl_found to see if we found anything.
	if test ! -z "$cf_openssl_basedir"; then
		if test -f "${cf_openssl_basedir}/include/openssl/opensslv.h" ; then
			SSL_INCLUDES="-I${cf_openssl_basedir}/include"
			SSL_LIBS="-L${cf_openssl_basedir}/lib"
		else
		dnl OpenSSL wasn't found in the directory specified.  Naughty
		dnl administrator...
		cf_openssl_basedir=""
		fi
	else
	dnl Check for stock FreeBSD 4.x and 5.x systems, since their files
	dnl are in /usr/include and /usr/lib.  In this case, we don't want to
	dnl change INCLUDES or LIBS, but still want to enable OpenSSL.
	dnl We can't do this check above, because some people want two versions
	dnl of OpenSSL installed (stock FreeBSD 4.x/5.x and /usr/local/ssl)
	dnl and they want /usr/local/ssl to have preference.
		if test -f "/usr/include/openssl/opensslv.h" ; then
			cf_openssl_basedir="/usr"
		fi
	fi

	dnl If we have a basedir defined, then everything is okay.  Otherwise,
	dnl we have a problem.
	if test ! -z "$cf_openssl_basedir"; then
		AC_MSG_RESULT($cf_openssl_basedir)
		cf_enable_openssl="yes"
	else
		AC_MSG_RESULT([not found. Specify a correct path?])
		cf_enable_openssl="no"
	fi
	unset cf_openssl_basedir
else
	dnl If --disable-openssl was specified
	AC_MSG_RESULT(disabled)
fi

save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $SSL_INCLUDES"
save_LIBS="$LIBS"
LIBS="$LIBS $SSL_LIBS"
if test "$cf_enable_openssl" != no; then
	dnl Check OpenSSL version (must be 0.9.6 or above!)
	AC_MSG_CHECKING(for OpenSSL 0.9.6 or above)
	AC_RUN_IFELSE(
		AC_LANG_PROGRAM(
	    	[#include <openssl/opensslv.h>
		#include <stdlib.h>],
		[[if ( OPENSSL_VERSION_NUMBER >= 0x00906000)
		exit(0); else exit(1);]]),
	cf_openssl_version_ok=yes,
	cf_openssl_version_ok=no,
	cf_openssl_version_ok=no)

	if test "$cf_openssl_version_ok" = yes; then
		AC_MSG_RESULT(found)

		dnl Work around pmake/gmake conditional incompatibilities
		AC_SUBST(ENCSPEED, encspeed)
		
		dnl Do all the HAVE_LIBCRYPTO magic -- and check for ciphers
		CPPFLAGS="$CPPFLAGS $SSL_LIBS"
		AC_CHECK_LIB(crypto, RSA_free)
		SSL_LIBS="$SSL_LIBS -lcrypto"
		SSL_SRCS_ENABLE='$(SSL_SRCS)'
	else
	        AC_MSG_RESULT(no - OpenSSL support disabled)
	fi
fi

CPPFLAGS="$save_CPPFLAGS"
LIBS="$save_LIBS"

dnl End OpenSSL detection


dnl Specialized functions and libraries
dnl ===================================

AC_ARG_WITH(zlib-path,
AC_HELP_STRING([--with-zlib-path=DIR],[Path to libz.so for ziplinks support.]),
[LIBS="$LIBS -L$withval"],)

AC_ARG_ENABLE(zlib,
AC_HELP_STRING([--disable-zlib],[Disable ziplinks support]),
[zlib=$enableval],[zlib=yes])

if test "$zlib" = yes; then

AC_CHECK_HEADER(zlib.h, [
	AC_CHECK_LIB(z, zlibVersion,
	[
		AC_SUBST(ZLIB_LD, -lz)
		AC_DEFINE(HAVE_LIBZ, 1, [Define to 1 if zlib (-lz) is available.])
	], zlib=no)
], zlib=no)

fi

dnl IO Loop Selection
dnl =================

AC_ARG_ENABLE(poll, AC_HELP_STRING([--enable-poll],[Force poll() usage.]),
[ if test $enableval = yes; then 
	SELECT_TYPE_EXPLICIT="poll"
  else
  	use_poll=no
  fi
],)

AC_ARG_ENABLE(select, AC_HELP_STRING([--enable-select],[Force select() usage.]),
[ if test $enableval = yes; then 
	SELECT_TYPE_EXPLICIT="select" 
  else
  	use_select=no
  fi
],)

AC_ARG_ENABLE(kqueue, AC_HELP_STRING([--enable-kqueue],[Force kqueue() usage.]),
[ if test $enableval = yes; then 
	SELECT_TYPE_EXPLICIT="kqueue"
  else
  	use_kqueue=no
  fi
],)

AC_ARG_ENABLE(devpoll,AC_HELP_STRING([--enable-devpoll],[Force usage of /dev/poll.]),
[ if test $enableval = yes; then 
	SELECT_TYPE_EXPLICIT="devpoll" 
	dnl These need to be defined or not defined
	AC_CHECK_HEADERS([sys/devpoll.h devpoll.h])
  else
  	use_devpoll=no;
  fi
],)

AC_ARG_ENABLE(epoll, AC_HELP_STRING([--enable-epoll],[Force sys_epoll usage (Linux only).]),
[ if test $enableval = yes; then 
	SELECT_TYPE_EXPLICIT="epoll"
  else
  	use_epoll=no
  fi
],)

dnl **********************************************************************
dnl Check for --with-confdir
dnl **********************************************************************

AC_MSG_CHECKING([whether to modify confdir])
AC_ARG_WITH(confdir, 
AC_HELP_STRING([--with-confdir=DIR],
	       [Directory to install config files.]),
	       [ confdir=`echo $withval | sed 's/\/$//'`
		 AC_MSG_RESULT(yes)
		 AC_DEFINE_DIR(ETC_DIR, confdir, [Prefix where config files are installed.])
		 AC_SUBST_DIR([confdir]) ],
	       [ confdir='${prefix}/etc'
		 AC_MSG_RESULT(no)
	         AC_DEFINE_DIR(ETC_DIR, confdir, [Prefix where config files are installed.])
	  	 AC_SUBST_DIR([confdir])]
)

dnl **********************************************************************
dnl Check for --with-logdir
dnl **********************************************************************

AC_MSG_CHECKING([whether to modify logdir])
AC_ARG_WITH(logdir, 
AC_HELP_STRING([--with-logdir=DIR],
	       [Directory where to write logfiles.]),
	       [ logdir=`echo $withval | sed 's/\/$//'`
		 AC_MSG_RESULT(yes)
		 AC_DEFINE_DIR(LOG_DIR, logdir, [Prefix where to write logfiles.])
		 AC_SUBST_DIR([logdir]) ],
	       [ logdir='${prefix}/logs'
		 AC_MSG_RESULT(no)
	         AC_DEFINE_DIR(LOG_DIR, logdir, [Prefix where to write logfiles.])
	  	 AC_SUBST_DIR([logdir])]
)

dnl **********************************************************************
dnl Check for --with-helpdir
dnl **********************************************************************

AC_MSG_CHECKING([whether to modify helpdir])
AC_ARG_WITH(helpdir, 
AC_HELP_STRING([--with-helpdir=DIR],
	       [Directory to install help files.]),
	       [ helpdir=`echo $withval | sed 's/\/$//'`
		 AC_MSG_RESULT(yes)
		 AC_DEFINE_DIR(HELP_DIR, helpdir, [Prefix where help files are installed.])
		 AC_SUBST_DIR([helpdir]) ],
	       [ helpdir='${prefix}/help'
		 AC_MSG_RESULT(no)
	         AC_DEFINE_DIR(HELP_DIR, helpdir, [Prefix where help file are installed.])
	  	 AC_SUBST_DIR([helpdir])]
)

dnl **********************************************************************
dnl Check for --with-moduledir
dnl **********************************************************************

AC_MSG_CHECKING([whether to modify moduledir])
AC_ARG_WITH(moduledir, 
AC_HELP_STRING([--with-moduledir=DIR],
	       [Directory to install modules.]),
	       [ moduledir=`echo $withval | sed 's/\/$//'`
		 AC_MSG_RESULT(yes)
		 AC_DEFINE_DIR(MODULE_DIR, moduledir, [Prefix where modules are installed.])
		 AC_SUBST_DIR([moduledir]) ],
	       [ moduledir='${prefix}/modules'
		 AC_MSG_RESULT(no)
	         AC_DEFINE_DIR(MODULE_DIR, moduledir, [Prefix where modules are installed.])
	  	 AC_SUBST_DIR([moduledir])]
)

if test ! -z "$SELECT_TYPE_EXPLICIT"; then
	SELECT_TYPE="$SELECT_TYPE_EXPLICIT";
	echo "Forcing $SELECT_TYPE to be enabled"
else

if test ! "x$use_select" = "xno"; then
	AC_CHECK_FUNCS(select, [haveselect=yes], [haveselect=no])
	if test "x$haveselect" = "xyes" ; then
		SELECT_TYPE="select"
	fi
fi

if test ! "x$use_poll" = "xno"; then
	AC_CHECK_FUNCS(poll, [havepoll=yes], [havepoll=no])
	if test "x$havepoll" = "xyes" ; then
		SELECT_TYPE="poll"
	fi
fi

if test ! "x$use_devpoll" = "xno"; then
	AC_MSG_CHECKING(for /dev/poll)
	if test -c "/dev/poll"; then
	        AC_MSG_RESULT(yes)
		AC_CHECK_HEADERS([devpoll.h sys/devpoll.h])
	        SELECT_TYPE="devpoll"
	else
		AC_MSG_RESULT(no)	
	fi
fi

if test ! "x$use_kqueue" = "xno"; then
	AC_CHECK_FUNCS(kevent, [havekqueue=yes], [havekqueue=no])
	if test "x$havekqueue" = "xyes" ; then
		SELECT_TYPE="kqueue"
	fi
fi

if test ! "x$use_epoll" = "xno"; then
	AC_CHECK_FUNCS(epoll_ctl, [haveepoll=yes], [haveepoll=no])
	if test "x$ac_cv_header_sys_epoll_h" = "xyes"; then
		if test "x$haveepoll" = "xyes" ; then
                	AC_MSG_CHECKING(for epoll support in kernel)
	                AC_TRY_RUN(
#include <stdint.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/syscall.h>
#include <unistd.h>     

int
main(int argc, char **argv)
{
        int epfd;

        epfd = epoll_create(256);
        exit (epfd == -1 ? 1 : 0);
}, [AC_MSG_RESULT(yes)
	    AC_DEFINE(HAVE_EPOLL, 1,
	        [Define if your system supports the epoll system calls])
	    SELECT_TYPE="epoll"], 
	    AC_MSG_RESULT(no), AC_MSG_RESULT(no))
		fi
	fi

haveepollsyscall=no

if test "x$ac_cv_header_sys_epoll_h" = "xyes"; then
	if test "x$haveepoll" = "xno" ; then
                AC_MSG_CHECKING(for epoll system call)
                AC_TRY_RUN(
#include <stdint.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/syscall.h>
#include <unistd.h>     

int
epoll_create(int size)
{
        return (syscall(__NR_epoll_create, size));
}
 
int
main(int argc, char **argv)
{
        int epfd;

        epfd = epoll_create(256);
        exit (epfd == -1 ? 1 : 0);
}, [AC_MSG_RESULT(yes)
    AC_DEFINE(HAVE_EPOLL, 1,
        [Define if your system supports the epoll system calls])
    SELECT_TYPE="epoll"], 
    AC_MSG_RESULT(no), AC_MSG_RESULT(no))
	fi
fi

fi

fi

if test -z "$SELECT_TYPE"; then
	AC_MSG_ERROR([Unable to find a usable IO interface],)
fi

echo "Using $SELECT_TYPE for select loop."

AC_DEFINE_UNQUOTED(SELECT_TYPE, "$SELECT_TYPE", [This is the type of IO loop we are using])
AC_SUBST(SELECT_TYPE)


dnl Debug-related options
dnl =====================

AC_ARG_ENABLE(assert,
AC_HELP_STRING([--enable-assert],[Enable assert(). Choose between soft(warnings) and hard(aborts the daemon)]),
[assert=$enableval], [assert=no])

if test "$assert" = no; then
	AC_DEFINE(NDEBUG, 1, [Define this to disable debugging support.])
elif test "$assert" = soft; then
	AC_DEFINE(SOFT_ASSERT, 1, [Define this to enable soft asserts.])
	AC_DEFINE(NDEBUG, 1, [Define this to disable debugging support.])
elif test "$assert" = yes; then
	assert = "hard";
fi

AC_MSG_CHECKING(if you want IO Debugging hooks)
AC_ARG_ENABLE(iodebug,
AC_HELP_STRING([--enable-iodebug],[Enable IO Debugging hooks]),
[iodebug=$enableval], [iodebug=no])

if test "$iodebug" = yes; then
	AC_DEFINE(USE_IODEBUG_HOOKS, 1, [Define this to enable IO Debug hooks.])
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi


AC_MSG_CHECKING(if you want to do a profile build)
AC_ARG_ENABLE(profile,
AC_HELP_STRING([--enable-profile],[Enable profiling]),
[profile=$enableval], [profile=no])

if test "$profile" = yes; then
	if test "$ac_cv_c_compiler_gnu" = yes; then
		IRC_CFLAGS="$IRC_CFLAGS -pg"
		AC_MSG_RESULT([yes, adding -pg])
		AC_DEFINE(CHARYBDIS_PROFILE, 1, [Define this if you are profiling.])
	else
		AC_MSG_RESULT([no, profile builds only work with gcc])
	fi
else
	AC_MSG_RESULT(no)
fi

AC_ARG_ENABLE(balloc,
AC_HELP_STRING([--disable-balloc],[Disable the block allocator.]),
[balloc=$enableval], [balloc=yes])

if test "$balloc" = no; then
	AC_DEFINE([NOBALLOC], 1, [Define to 1 if you wish to disable the block allocator.])
fi

AC_ARG_ENABLE(ricer-hashing,
AC_HELP_STRING([--enable-ricer-hashing],[Enable assembly-based hashing routines.]),
[ricer_hashing=$enableval], [ricer_hashing=no])

FNVHASH_S=""

if test "$ricer_hashing" = "yes"; then
	AC_DEFINE([RICER_HASHING], 1, [Define to 1 if you are using the assembly-based hashing routines.])
	FNVHASH_S="fnvhash.s"
fi

AC_SUBST(FNVHASH_S)

AC_ARG_ENABLE(small-net,
AC_HELP_STRING([--enable-small-net],[Enable small network support.]),
[small_net=$enableval], [small_net=no])

if test "$small_net" = yes; then
dnl	AC_DEFINE([HASHSIZE], 4096, [Max number of buckets in hash tables.])
	AC_DEFINE([NICKNAMEHISTORYLENGTH], 1500, [Size of the WHOWAS array.])
	AC_DEFINE([CHANNEL_HEAP_SIZE], 256, [Size of the channel heap.])
	AC_DEFINE([BAN_HEAP_SIZE], 128, [Size of the ban heap.])
	AC_DEFINE([CLIENT_HEAP_SIZE], 256, [Size of the client heap.])
	AC_DEFINE([LCLIENT_HEAP_SIZE], 128, [Size of the local client heap.])
	AC_DEFINE([PCLIENT_HEAP_SIZE], 32, [Size of the pre-client heap.])
	AC_DEFINE([USER_HEAP_SIZE], 128, [Size of the user heap.])
	AC_DEFINE([DNODE_HEAP_SIZE], 256, [Size of the dlink_node heap.])
	AC_DEFINE([TOPIC_HEAP_SIZE], 256, [Size of the topic heap.])
	AC_DEFINE([LINEBUF_HEAP_SIZE], 128, [Size of the linebuf heap.])
	AC_DEFINE([MEMBER_HEAP_SIZE], 256, [Sizeof member heap.])
	AC_DEFINE([ND_HEAP_SIZE], 128, [Size of the nick delay heap.])
	AC_DEFINE([CONFITEM_HEAP_SIZE], 128, [Size of the confitem heap.])
	AC_DEFINE([MONITOR_HEAP_SIZE], 128, [Size of the monitor heap.])
else
dnl These settings are for a large network like efnet..they will use lots of memory
dnl so enable small net unless you really need this much support
        AC_DEFINE([NICKNAMEHISTORYLENGTH], 15000, [Size of the WHOWAS array.])
        AC_DEFINE([CHANNEL_HEAP_SIZE], 8192, [Size of the channel heap.])
        AC_DEFINE([BAN_HEAP_SIZE], 4096, [Size of the ban heap.])
        AC_DEFINE([CLIENT_HEAP_SIZE], 8192, [Size of the client heap.])
        AC_DEFINE([LCLIENT_HEAP_SIZE], 1024, [Size of the local client heap.])
	AC_DEFINE([PCLIENT_HEAP_SIZE], 256, [Size of the pre-client heap.])
        AC_DEFINE([USER_HEAP_SIZE], 8192, [Size of the user heap.])
        AC_DEFINE([DNODE_HEAP_SIZE], 8192, [Size of the dlink_node heap.])
        AC_DEFINE([TOPIC_HEAP_SIZE], 4096, [Size of the topic heap.])
        AC_DEFINE([LINEBUF_HEAP_SIZE], 2048, [Size of the linebuf heap.])
        AC_DEFINE([MEMBER_HEAP_SIZE], 32768, [Sizeof member heap.])
        AC_DEFINE([ND_HEAP_SIZE], 512, [Size of the nick delay heap.])
        AC_DEFINE([CONFITEM_HEAP_SIZE], 256, [Size of the confitem heap.])
	AC_DEFINE([MONITOR_HEAP_SIZE], 1024, [Size of the monitor heap.])
fi

AC_ARG_WITH(nicklen,
AC_HELP_STRING([--with-nicklen=LENGTH],[Set the nick length to LENGTH (default 15, max 50)]),
[
  if test $withval -ge 50; then
	NICKLEN=50
	AC_MSG_WARN([NICKLEN has a hard limit of 50. Setting NICKLEN=50])
  else
	NICKLEN="$withval"
  fi
], [NICKLEN=15])

AC_ARG_WITH(topiclen,           
AC_HELP_STRING([--with-topiclen=NUMBER],[Set the max topic length to NUMBER (default 390, max 390)]),
[
 if test $withval -ge 390; then
	TOPICLEN=390
	AC_MSG_WARN([TOPICLEN has a hard limit of 390. Setting TOPICLEN=390])
 else
 	TOPICLEN=$withval
 fi
], [TOPICLEN=390])

AC_ARG_WITH(maxclients,
AC_HELP_STRING([--with-maxclients=NUMBER],[Maximum number of connections the ircd can handle]),
        MAX_CLIENTS="$withval", MAX_CLIENTS=3000)


if test "$MAX_CLIENTS" = yes; then
	MAX_CLIENTS=3000
fi

if test $MAX_CLIENTS -gt 65536; then
	MAX_CLIENTS=65536
	AC_MSG_WARN([Max connections cannot be larger than 65536!])
fi

AC_DEFINE_UNQUOTED(TOPICLEN, ${TOPICLEN}, [Maximum topic length (<=390)])
AC_DEFINE_UNQUOTED(NICKLEN, (${NICKLEN}+1), [Nickname length])
AC_DEFINE_UNQUOTED(MAX_CLIENTS, ${MAX_CLIENTS}, [Maximum number of network connections])

AC_ARG_ENABLE(shared-modules,
AC_HELP_STRING([--disable-shared-modules],[ Disable shared modules.]),
[shared_modules=$enableval], [shared_modules="yes"])

dnl Some first-stage sanity checks.
if test "$shared_modules" = yes; then
  	
	if test "$CYGWIN" = yes; then
		AC_MSG_WARN([disabling shared modules; Cygwin is at present unable to build them.])
		shared_modules="no"
	fi

	dnl TenDRA's cc is called tcc too.
	if test "$CC" = tcc -a "$TenDRA" = "no"; then
		AC_MSG_WARN([disabling shared modules: Tiny C Compiler can't create PIC])
		shared_modules="no"
	fi
fi

dnl Second stage: check for functions and headers.
if test "$shared_modules" = yes; then
	DYNLINK_C=dynlink.c
	AC_CHECK_HEADERS(dlfcn.h)
	AC_SEARCH_LIBS(shl_load, dld, 
	[
	  AC_DEFINE(HAVE_SHL_LOAD, 1, [Define if the shl_load function is available.])
	  SUFFIX=".sl"
	  MOD_TARGET=hpux_shared
	  SEDOBJ="s/\.o/.sl/g"  
	],
	dnl !shl_load:
	[
          dnl standard dlopen
          AC_SEARCH_LIBS(dlopen, [dl c_r],
	  [
	    AC_DEFINE(HAVE_DLOPEN, 1, [Define if the dlopen function is available.])
	    SUFFIX=".so"
	    MOD_TARGET=shared_modules
	    SEDOBJ="s/\.o/.so/g"
	    if test "$AppleGCC" = yes; then
              AC_CHECK_HEADERS([mach-o/dyld.h])
            fi
	    AC_CHECK_FUNC(dlsym, ,
	    [
	      AC_MSG_WARN([dlsym is not available, shared modules disabled])
	      shared_modules=no
	    ])
	    AC_CHECK_FUNCS(dlfunc)
	  ],
	  [
	    shared_modules=no
	  ])
	])
fi

AC_DEFINE_UNQUOTED(SHARED_SUFFIX, "$SUFFIX", [Suffix for shared libraries on this platform.])

dnl Third stage - wrangling the linker.
if test "$shared_modules" = yes; then
	# The GNU linker requires the -export-dynamic option to make
	# all symbols visible in the dynamic symbol table.
	hold_ldflags=$LDFLAGS
	AC_MSG_CHECKING(for the ld -export-dynamic flag)
	LDFLAGS="${LDFLAGS} -Wl,-export-dynamic -Werror"
	AC_LINK_IFELSE(AC_LANG_PROGRAM([],[int i;]), found=yes, found=no)
	LDFLAGS=$hold_ldflags

	if expr "`uname -s`" : ^IRIX >/dev/null 2>&1; then
		found="no, IRIX ld uses -B,dynamic"
		LDFLAGS="${LDFLAGS} -Wl,-B,dynamic"
	fi
	
	if expr "`uname -s`" : ^AIX >/dev/null 2>&1; then
		found="no, AIX ld uses -G -brtl"
		LDFLAGS="${LDFLAGS} -Wl,-G,-brtl"
	fi

	AC_MSG_RESULT($found)

	if test "$found" = yes; then
		LDFLAGS="${LDFLAGS} -Wl,-export-dynamic"
	fi

	AC_MSG_CHECKING(for compiler option to produce PIC)
	dnl The order should be here to check for groups of compilers,
	dnl then for odd compilers, then if no PICFLAGS were set up,
	dnl check for GCC and set defaults, or else error. -jmallett
	if test "$SGS" = "yes"; then
		AC_MSG_RESULT([SVR4 SGS interfaces: -KPIC -DPIC -G])
		PICFLAGS="-KPIC -DPIC -G"
	fi
	
	if test "$AppleGCC" = "yes"; then
		AC_MSG_RESULT([Darwin Mach-O bundles: -fno-common -bundle -flat_namespace -undefined suppress])
		PICFLAGS="-fno-common -bundle -flat_namespace -undefined suppress"
	fi
	dnl Please note, that on HPUX two different stages of module compilation occurs, since
	dnl while compiling modules, the compiler does not allow you to give arguments
	dnl to the linker. (I did not design this) 
	dnl So we need -c in the first stage of module compilation.
	dnl In the second stage, we link the modules via ld -b.
	dnl Additionally, HPUX does not like -export-dynamic, it likes -E instead.
	dnl -TimeMr14C
	if test "$HPUX" = "yes" -a "$CC" != gcc; then
		AC_MSG_RESULT(HP-UX cc: +z -r -q -n)
		PICFLAGS="+z -r -q -n -c"
		AC_MSG_CHECKING([if +ESfic is required on this platform])
		
		if expr "`$CC +ESfic 2>&1`" : ".*neither supported.*" >/dev/null; then 
			AC_MSG_RESULT(no)
		else
			AC_MSG_RESULT(yes)
			PICFLAGS="$PICFLAGS +ESfic"
		fi

		LDFLAGS="${LDFLAGS} -Wl,-E"
	fi
	if test "$Tru" = yes -a "$CC" != gcc; then
		AC_MSG_RESULT([Tru64: -shared -expect_unresolved '*'])
		PICFLAGS="-shared -expect_unresolved '*' "
		LDFLAGS="-call_shared"
	fi
	if test -z "$PICFLAGS"; then
		if test "$ac_cv_c_compiler_gnu" = "yes"; then
			AC_MSG_RESULT(gcc: -fPIC -DPIC -shared)
			PICFLAGS="-fPIC -DPIC -shared"
		else
			AC_MSG_RESULT(no)
			shared_modules=no
		fi
	fi
fi

# This must be down here, or it will mess up checks like the ones
# for -Wl,-export-dynamic
# -- jilles
AC_ARG_ENABLE(warnings,
AC_HELP_STRING([--enable-warnings],[Enable all sorts of warnings for debugging.]),
[
IRC_CFLAGS="$IRC_CFLAGS -O0"
CFLAGS="$IRC_CFLAGS"

CHARYBDIS_C_GCC_TRY_FLAGS([-Wall], charybdis_cv_c_gcc_w_all)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wpointer-arith], charybdis_cv_c_gcc_w_pointer_arith)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wimplicit -Wnested-externs], charybdis_cv_c_gcc_w_implicit)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wcast-align], charybdis_cv_c_gcc_w_cast_align)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wcast-qual], charybdis_cv_c_gcc_w_cast_qual)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations], charybdis_cv_c_gcc_prototypes)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wparenthesis], charybdis_cv_c_gcc_parenthesis)
CHARYBDIS_C_GCC_TRY_FLAGS([-W -Wno-unused], charybdis_cv_c_gcc_w)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wextra], charybdis_cv_c_gcc_w_extra)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wshadow], charybdis_cv_c_gcc_w_shadow)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wmissing-noreturn], charybdis_cv_c_gcc_w_missing_noreturn)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wundef], charybdis_cv_c_gcc_w_undef)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wpacked], charybdis_cv_c_gcc_w_packed)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wnested-externs], charybdis_cv_c_gcc_w_nested_externs)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wbad-function-cast], charybdis_cv_c_gcc_w_bad_function_cast)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wunused-function -Wunused-label -Wunused-value -Wunused-variable], charybdis_cv_c_gcc_w_unused)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wredundant-decls], charybdis_cv_c_gcc_w_redundant_decls)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wfloat-equal], charybdis_cv_c_gcc_w_float_equal)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wformat=2], charybdis_cv_c_gcc_w_format)
CHARYBDIS_C_GCC_TRY_FLAGS([-pedantic], charybdis_cv_c_gcc_pedantic)

IRC_CFLAGS="$CFLAGS"
],[])

if test "$shared_modules" = no; then
	DYNLINK_C=""
	MOD_TARGET="libmodules.a"
	MODULES_LIBS="../modules/libmodules.a"
	SEDOBJ=""
	AC_DEFINE(STATIC_MODULES, 1, [Define to 1 if dynamic modules can't be used.])
	AC_MSG_WARN([shared module support has been disabled!])
fi

dnl Stage 5 - underscores in front of symbol names.
if test "$shared_modules" = yes; then

	AC_CHECK_FUNC(nlist,,
		AC_CHECK_LIB(dl, nlist, nlist_lib="-ldl",
			AC_CHECK_LIB(elf, nlist, nlist_lib="-lelf",)
		)
	)

        dnl We need to find out whether underscores are appended to symbol
        dnl names in executable files.  First, though, we need to see
        dnl where nlist.h is hiding.
	AC_CHECK_HEADER(libelf/nlist.h, [ nlist_h="libelf/nlist.h" ], )
	AC_CHECK_HEADER(elf/nlist.h, [ nlist_h="elf/nlist.h" ], )
	AC_CHECK_HEADER(nlist.h, [ nlist_h="nlist.h" ], )
	if test x"$nlist_h" = "x"; then
		AC_DEFINE_UNQUOTED(SYMBOL_PREFIX, "", [String containing extra underscores prepended to symbols loaded from modules.])
	else
		AC_MSG_CHECKING(for extra underscores prepended to symbol names)
		AC_CACHE_VAL(symbol_underscores,
		[
cat << EOF > conftest.c
#include <$nlist_h>
#include <stdio.h>
#include <stdlib.h>
void _modinit(void);
int main(int argc, char *argv[[]]) {
	int i;
	struct nlist nl[[5]];

	/* fill the names in this way, so it'll work almost everywhere */
	nl[[0]].n_name = "_modinit";
	nl[[1]].n_name = "__modinit";
	nl[[2]].n_name = "___modinit";
	nl[[3]].n_name = "____modinit";
	nl[[0]].n_value = nl[[1]].n_value = nl[[2]].n_value = nl[[3]].n_value = nl[[4]].n_name = NULL;

	if(argc < 2 || (nlist(argv[[1]], nl)) == -1) exit(-1);
	for(i = 0; i < 4; i++) {
		if(nl[[i]].n_value != NULL)
		{
			int j;
			for(j = 0; j < i; j++)
				printf("_");
			exit(i);
		}
	}
	exit(-1);
}
void _modinit(void) { return; }
EOF
		$CC $CPPFLAGS $IRC_CFLAGS -o conftest conftest.c $nlist_lib >/dev/null 2>&1
		symbol_underscores=`./conftest conftest`
		AC_MSG_RESULT($symbol_underscores)
		$RM -f conftest conftest.c
		])
		AC_DEFINE_UNQUOTED(SYMBOL_PREFIX, "${symbol_underscores}", [String containing extra underscores prepended to symbols loaded from modules.])
	fi
fi

AC_SUBST(MODULES_LIBS)
AC_SUBST(MOD_TARGET)

AC_SUBST(SSL_SRCS_ENABLE)
AC_SUBST(SSL_INCLUDES)
AC_SUBST(SSL_LIBS)

AC_SUBST(LDFLAGS)
AC_SUBST(PICFLAGS)
AC_SUBST(IRC_CFLAGS)
AC_SUBST(SEDOBJ)


if test "$prefix" = "NONE"; then 
	AC_DEFINE_UNQUOTED(IRCD_PREFIX, "$ac_default_prefix", [Prefix where the ircd is installed.])

else

dnl Don't get bitten by Cygwin's stupidity if the user specified
dnl a custom prefix with a trailing slash

	prefix=`echo $prefix | sed 's/\/$//'`
	AC_DEFINE_UNQUOTED(IRCD_PREFIX, "$prefix", [Prefix where the ircd is installed.])
	
fi

AC_CONFIG_FILES(			\
	Makefile			\
	libcharybdis/Makefile		\
	servlink/Makefile		\
	extensions/Makefile		\
	unsupported/Makefile		\
	src/Makefile			\
	modules/Makefile		\
	tools/Makefile			\
	doc/Makefile			\
	help/Makefile			\
)

AC_OUTPUT

if test "$cf_openssl_version_ok" = yes; then
	openssl="yes"
else
	openssl="no"
fi

if test "$shared_modules" = yes; then
	modules=shared
else
	modules=static
fi

echo "
Configuration:
	Install directory  : $prefix

	Ziplinks           : $zlib
	OpenSSL            : $openssl
	Modules            : $modules
	IPv6 support       : $have_v6
	Socket Engine      : $SELECT_TYPE
	Small network      : $small_net
	Block allocator    : $balloc
	ASM hashing code   : $ricer_hashing

	Nickname length    : $NICKLEN
	Topic length       : $TOPICLEN

Use make to compile Charybdis, then make install to install it.
"
