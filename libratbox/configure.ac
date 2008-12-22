dnl $Id: configure.ac 26260 2008-12-10 04:08:39Z androsyn $
dnl Process this file with autoconf to produce a configure script.

dnl TODO: clean up all the OpenSSL and shared module checking stuff;
dnl the most major changes have already been made and it looks like
dnl said functions need to be just about as complex as they already are.

AC_PREREQ(2.63)
AUTOMAKE_OPTIONS = 1.10
dnl Sneaky way to get an Id tag into the configure script
AC_COPYRIGHT([$Id: configure.ac 26260 2008-12-10 04:08:39Z androsyn $])
AC_INIT([libratbox],[devel])
AM_INIT_AUTOMAKE(AC_PACKAGE_TARNAME, AC_PACKAGE_VERSION, -)

AM_CONFIG_HEADER(include/libratbox_config.h)

AC_PREFIX_DEFAULT(/usr/local/libratbox)
AC_GNU_SOURCE

dnl Checks for programs.
AC_PROG_CC_C99

if test x"$ac_cv_prog_cc_c99" = "xno"; then
	AC_ERROR([ircd-ratbox requires a C99 capable compiler])
fi
        

AC_PROG_INSTALL
AC_PROG_EGREP
AC_PROG_SED

F77=no
CXX=no
GCJ=no

AM_DISABLE_STATIC
AM_ENABLE_SHARED
AM_MAINTAINER_MODE

AC_ISC_POSIX
AC_C_INLINE
AC_C_CONST
AC_PROG_MAKE_SET
AC_PROG_INSTALL
AC_PATH_PROG(AUTOMAKE, automake)
AC_PATH_PROG(ACLOCAL, aclocal)
AC_PATH_PROG(AUTOHEADER, autoheader)
AC_PATH_PROG(AS, as)
AC_PATH_PROG(RM, rm)
AC_PATH_PROG(CP, cp)
AC_PATH_PROG(MV, mv)
AC_PATH_PROG(LN, ln)
AC_PATH_PROG(TOUCH, touch)
AC_LANG(C)

AC_PROG_LIBTOOL

LIBTOOL="$LIBTOOL --silent"

case "$host_os" in
*cygwin*)
     AC_DEFINE_UNQUOTED(CYGWIN,1,[This is a Cygwin system])
     AC_DEFINE_UNQUOTED(WINDOWS,1,[This is a Windows system])
     ;;
*mingw*)
     AC_DEFINE_UNQUOTED(MINGW,1,[This is a MinGW system])
     AC_DEFINE_UNQUOTED(WINDOWS,1,[This is a Windows system])
     AC_CHECK_HEADER(windows.h, , [AC_MSG_ERROR([** MinGW and no windows.h.  I give up.])])
     AC_CHECK_HEADER(winsock2.h, , [AC_MSG_ERROR([** MinGW and no winsock2.h.  I give up.])])
     AC_DEFINE_UNQUOTED(HAVE_WINSOCK2_H, 1, [Have WINSOCK2_H])
     AC_DEFINE_UNQUOTED(HAVE_WINSOCK_H, 1, [Have WINSOCK_H])
     LIBS="$LIBS -lws2_32 -liphlpapi"
     is_mingw="yes"
     ;;
*interix*)
     CPPFLAGS="$CFLAGS -D_ALL_SOURCE -D_XOPEN_SOURCE=500"
     ;;
*solaris*)
     CPPFLAGS="$CFLAGS -D_POSIX_PTHREAD_SEMANTICS -D_XPG4_2"
     ;;
*)
     ;;
esac

AM_CONDITIONAL([MINGW], [test "$is_mingw" = "yes"])


# backwards support for IRC_CFLAGS
CFLAGS="$IRC_CFLAGS $CFLAGS -Wall"

dnl use directory structure of cached as default (hack)
if test "$libexecdir" = '${exec_prefix}/libexec' &&
   test "$localstatedir" = '${prefix}/var'; then
	libexecdir='${bindir}'
	localstatedir='${prefix}'
fi

AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UID_T


dnl Checks for header files.
AC_HEADER_STDC

AC_CHECK_HEADERS([crypt.h unistd.h sys/socket.h sys/stat.h sys/time.h time.h netinet/in.h arpa/inet.h errno.h sys/uio.h spawn.h sys/poll.h sys/epoll.h sys/select.h sys/devpoll.h sys/event.h port.h signal.h sys/signalfd.h sys/timerfd.h])
AC_HEADER_TIME

dnl Networking Functions
dnl ====================

AC_SEARCH_LIBS(socket, [socket],,)


AC_CHECK_MEMBER([struct sockaddr.sa_len], [AC_DEFINE(SOCKADDR_IN_HAS_LEN, 1, [Define to 1 if sockaddr has a 'sa_len'
member.])],,[[
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
]])

AC_CHECK_TYPE([socklen_t], rb_cv_socklen_t=socklen_t,[
	AC_MSG_CHECKING([for socklen_t equivalent])
	AC_CACHE_VAL([rb_cv_socklen_t],
	[
	# Systems have either "struct sockaddr *" or
	# "void *" as the second argument to getpeername
	rb_cv_socklen_t=
	for arg2 in "struct sockaddr" void; do
		for t in int size_t unsigned long "unsigned long"; do
			AC_TRY_COMPILE([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
int getpeername (int, $arg2 *, $t *);
],[
$t len;
getpeername(0,0,&len);
],[
	rb_cv_socklen_t="$t"
	break
])
		done
	done

	if test "x$rb_cv_socklen_t" = x; then
		AC_MSG_WARN([Cannot find a type to use in place of socklen_t using int])
		rb_cv_socklen_t=int
	fi
	])
	AC_MSG_RESULT($rb_cv_socklen_t)],
	[#include <sys/types.h>
#include <sys/socket.h>])

rb_socklen_t=$rb_cv_socklen_t





AC_ARG_ENABLE(ipv6,AC_HELP_STRING([--disable-ipv6],[Disable IPv6 support]),[ipv6=$enableval],[ipv6=yes])

if test x$ipv6 != xyes; then
	have_v6="no"
else

have_v6=yes
RB_PROTO_INET6([], [
		AC_MSG_NOTICE([Disabling IPv6 support: PF_INET6 not found])
		have_v6=no
		])

	if test "X$have_v6" = "Xyes"; then
	RB_TYPE_STRUCT_SOCKADDR_IN6([], [
		AC_MSG_NOTICE([Disabling IPv6 support: struct sockaddr_in6 not found])
		have_v6=no
		])
	fi

fi			

AC_CHECK_TYPES([struct sockaddr_storage],[
	rb_have_sockaddr_storage=yes
        ],[], [
		#include <sys/types.h>
		#include <sys/socket.h>
	])
                

save_LIBS=$LIBS
AC_SEARCH_LIBS(crypt, [crypt descrypt],,)
LIBS=$save_LIBS

CRYPT_LIB=$ac_cv_search_crypt

if test "$CRYPT_LIB" = "none required"; then
	unset CRYPT_LIB
elif test "$CRYPT_LIB" = no; then
	need_crypt=yes;
	AC_DEFINE(NEED_CRYPT, 1, [Define if your system needs crypt.])
	unset CRYPT_LIB
fi

AM_CONDITIONAL([NEED_CRYPT], [test x"$need_crypt" = "xyes"])


AC_SUBST(CRYPT_LIB)

dnl Check for stdarg.h - if we cant find it, halt configure
AC_CHECK_HEADER(stdarg.h, , [AC_MSG_ERROR([** stdarg.h could not be found - libratbox will not compile without it **])])

AC_CHECK_TYPE([sa_family_t], [],
[AC_DEFINE(sa_family_t, [u_int16_t], [If system does not define sa_family_t, define it here.])],
[[#include <sys/types.h>
#include <sys/socket.h>]])


dnl check for various functions...
AC_CHECK_FUNCS([socketpair gettimeofday writev sendmsg gmtime_r strtok_r usleep posix_spawn strlcpy strlcat strnlen fstat signalfd select poll kevent port_create epoll_ctl arc4random getrusage timerfd_create])	

AC_SEARCH_LIBS(nanosleep, rt posix4, AC_DEFINE(HAVE_NANOSLEEP, 1, [Define if you have nanosleep]))
AC_SEARCH_LIBS(timer_create, rt, AC_DEFINE(HAVE_TIMER_CREATE, 1, [Define if you have timer_create]))
RB_CHECK_TIMER_CREATE
RB_CHECK_TIMERFD_CREATE

AC_FUNC_ALLOCA
AC_FUNC_VFORK
AC_FUNC_MMAP

AC_MSG_CHECKING(for /dev/poll)
if test -c "/dev/poll"; then
        AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_DEVPOLL, [1], [Define to 1 if you have devpoll])
else
	AC_MSG_RESULT(no)	
fi


if test "$is_mingw" = "yes"; then
	AC_DEFINE(HAVE_WIN32, [1], [Define to 1 if you are on windows])
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
			SSL_CFLAGS="-I${cf_openssl_basedir}/include"
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
CPPFLAGS="$CPPFLAGS $SSL_CFLAGS"
save_LIBS="$LIBS"
LIBS="$LIBS $SSL_LIBS"
if test "$cf_enable_openssl" != no; then
	dnl Check OpenSSL version (must be 0.9.7 or above!)
	AC_MSG_CHECKING(for OpenSSL 0.9.7 or above)
	AC_RUN_IFELSE(
		AC_LANG_PROGRAM(
	    	[#include <openssl/opensslv.h>
		#include <stdlib.h>],
		[[if (OPENSSL_VERSION_NUMBER >= 0x00907000)
		exit(0); else exit(1);]]),
	cf_enable_openssl=yes,
	cf_enable_openssl=no,
	cf_enable_openssl=no)

	if test "$cf_enable_openssl" != no; then
		AC_MSG_RESULT(found)
	else
	        AC_MSG_RESULT(no - OpenSSL support disabled)
	fi

fi


if test "$cf_enable_openssl" != no; then
	CPPFLAGS="$CPPFLAGS $SSL_LIBS"
	AC_CHECK_LIB(crypto, RAND_status, 
		[cf_enable_openssl=yes],
		[cf_enable_openssl=no])
fi

if test "$cf_enable_openssl" != no; then
	CPPFLAGS="$CPPFLAGS $SSL_LIBS"
	AC_CHECK_LIB(ssl, SSL_read, 
		[SSL_LIBS="$SSL_LIBS -lssl -lcrypto"],
		[cf_enable_openssl=no], [-lcrypto])
fi



dnl GnuTLS support
AC_MSG_CHECKING(for GnuTLS)
AC_ARG_ENABLE(gnutls,
[AC_HELP_STRING([--enable-gnutls],[Enable GnuTLS support.])
AC_HELP_STRING([--disable-gnutls],[Disable GnuTLS support.])],
[cf_enable_gnutls=$enableval],
[cf_enable_gnutls="auto"])
 
if test "$cf_enable_gnutls" != no; then
       PKG_CHECK_MODULES(GNUTLS, [gnutls], [
               cf_enable_gnutls="yes"   
       ], [cf_enable_gnutls="no"])      
fi

if test "$cf_enable_gnutls" = "auto" -a "$cf_enable_openssl" = "yes"; then
       cf_enable_gnutls="no"
fi

if test x"$cf_enable_openssl" != xno; then
	AC_DEFINE(HAVE_OPENSSL,1,[Has OpenSSL])
	GNUTLS_CFLAGS=""
	GNUTLS_LIBS=""
	SSL_TYPE="openssl"
elif test x"$cf_enable_gnutls" != xno; then
	AC_DEFINE(HAVE_GNUTLS, 1, [Has GnuTLS])
	SSL_LIBS=""
	SSL_CFLAGS=""	
	SSL_TYPE="gnutls"
fi




CPPFLAGS="$save_CPPFLAGS"
LIBS="$save_LIBS"

dnl End OpenSSL detection




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

AC_MSG_CHECKING(if you want to do a profile build)
AC_ARG_ENABLE(profile,
AC_HELP_STRING([--enable-profile],[Enable profiling]),
[profile=$enableval], [profile=no])

if test "$profile" = yes; then
	if test "$ac_cv_c_compiler_gnu" = yes; then
		CFLAGS="$CFLAGS -pg -static"
		AC_MSG_RESULT([yes, adding -pg -static])
		AC_DEFINE(RATBOX_PROFILE, 1, [Defined to mark profiling is enabled])
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

AC_ARG_ENABLE(warnings,
AC_HELP_STRING([--enable-warnings],[Enable all sorts of warnings for debugging.]),
[CFLAGS="$CFLAGS -Wall -Werror -Wcast-qual -Wmissing-declarations -Wmissing-prototypes -Wnested-externs -Wshadow -Wwrite-strings -W -Wno-unused -Wunused-function -Wunused-variable"],[])

AC_SUBST(LDFLAGS)
AC_SUBST(PICFLAGS)
AC_SUBST(CFLAGS)
AC_SUBST(SEDOBJ)
AC_SUBST(SSL_CFLAGS)
AC_SUBST(SSL_LIBS)
AC_SUBST(GNUTLS_CFLAGS)
AC_SUBST(GNUTLS_LIBS)

if test "$prefix" = "NONE"; then 
	AC_DEFINE_UNQUOTED(RB_PREFIX, "$ac_default_prefix", [Prefix where libratbox is installed.])

else

dnl Don't get bitten by Cygwin's stupidity if the user specified
dnl a custom prefix with a trailing slash

	prefix=`echo $prefix | sed 's/\/$//'`
	AC_DEFINE_UNQUOTED(RB_PREFIX, "$prefix", [Prefix where libratbox is installed.])
	
fi

AC_SUBST(RB_PREFIX)

AC_CONFIG_COMMANDS([include/librb-config.h],
[
	outfile=include/librb-config.h.tmp
	cat > $outfile  <<\_______EOF
/*    
 * librb-config.h: libratbox config file. Please modify configure.ac
 */
 
#ifndef __LIBRB_CONFIG_H
#define __LIBRB_CONFIG_H

_______EOF 

if test "x$rb_have_ipv6" = "xyes"; then
	echo "#define RB_IPV6 1" >> $outfile
fi

if test "x$rb_windows_h" = "xyes"; then
	echo '#define WIN32_LEAN_AND_MEAN 1' >> $outfile
	echo '#include <windows.h>' >> $outfile
	echo '#include <winsock2.h>' >> $outfile
	echo '#include <iphlpapi.h>' >> $outfile
fi

if test "x$rb_alloca_h" = "xyes"; then
	echo '#define RB_HAVE_ALLOCA_H 1' >> $outfile
fi

if test "x$rb_header_stdc" = "xyes"; then
	echo '#include <stdlib.h>' >> $outfile
	echo '#include <stddef.h>' >> $outfile
elif test "x$rb_header_stdlib" = "xyes"; then
	echo '#include <stdlib.h>' >> $outfile
fi

if test "x$rb_header_string_h" = "xyes"; then
	echo '#include <string.h>' >> $outfile
fi

if test "x$rb_stdint_h" = "xyes"; then
	echo '#include <stdint.h>' >> $outfile
fi

if test "x$rb_inttypes_h" = "xyes"; then
	echo '#include <inttypes.h>' >> $outfile
fi

if test "x$rb_sys_types_h" = "xyes"; then
	echo '#include <sys/types.h>' >> $outfile
fi
if test "x$rb_sys_time_h" = "xyes"; then
	echo '#include <sys/time.h>' >> $outfile
fi
if test "x$rb_sys_stat_h" = "xyes"; then
	echo '#include <sys/stat.h>' >> $outfile
fi
if test "x$rb_time_h" = "xyes"; then
	echo '#include <time.h>' >> $outfile
fi

if test "x$rb_sys_socket_h" = "xyes"; then
	echo '#include <sys/socket.h>' >> $outfile
fi

if test "x$rb_netinet_in_h" = "xyes"; then
	echo '#include <netinet/in.h>' >> $outfile
fi

if test "x$rb_arpa_inet_h" = "xyes"; then
	echo '#include <arpa/inet.h>' >> $outfile
fi

if test "x$rb_unistd_h" = "xyes"; then
	echo '#include <unistd.h>' >> $outfile
fi

if test "x$rb_crypt_h" = "xyes"; then
	echo '#include <crypt.h>' >> $outfile
fi

if test "x$rb_errno_h" = "xyes"; then
	echo '#include <errno.h>' >> $outfile
fi

echo "typedef $rb_socklen_t rb_socklen_t;" >> $outfile


if test "x$rb_sockaddr_sa_len" = "xyes"; then
	echo '#define RB_SOCKADDR_HAS_SA_LEN 1' >> $outfile
fi

if test "x$rb_sockaddr_storage" = "xyes"; then
	echo '#define rb_sockaddr_storage sockaddr_storage' >> $outfile
else
	echo 'struct rb_sockaddr_storage { uint8_t _padding[[128]]; };' >> $outfile
fi

cat >> $outfile <<\_______EOF
#endif /* __LIBRB_CONFIG_H */
_______EOF

if cmp -s $outfile include/librb-config.h; then
	AC_MSG_NOTICE([include/librb-config.h is unchanged])
	${rb_rm} -f $outfile
else
	${rb_mv} $outfile include/librb-config.h
fi

],[
	if test x$ac_cv_header_stdc = xyes; then
		rb_header_stdc=yes
	fi
	if test x$ac_cv_header_stdlib_h = xyes; then
		rb_header_stdlib_h=yes
	fi
	if test x$ac_cv_header_string_h = xyes; then
		rb_header_string_h=yes
	fi
	if test x$ac_cv_header_memory_h = xyes; then
		rb_header_memory_h=yes
	fi
	if test "x${ac_cv_working_alloca_h+set}" = xset ; then
		rb_alloca_h="$ac_cv_working_alloca_h"
	else
		rb_alloc_h="$ac_cv_header_alloca_h"
	fi
	if test x$ac_cv_member_struct_sockaddr_sa_len = xyes; then
		rb_sockaddr_sa_len=yes
	fi
	if test x$ac_cv_header_sys_socket_h = xyes; then
		rb_sys_socket_h=yes
	fi
	if test x$ac_cv_header_sys_types_h = xyes; then
		rb_sys_types_h=yes
	fi
	if test x$ac_cv_header_sys_stat_h = xyes; then
		rb_sys_stat_h=yes
	fi
	if test x$ac_cv_header_sys_time_h = xyes; then
		rb_sys_time_h=yes
	fi
	if test x$ac_cv_header_time = xyes; then
		rb_time_h=yes
	fi

	if test x$ac_cv_header_stdint_h = xyes; then
		rb_stdint_h=yes
	fi
	if test x$ac_cv_header_inttypes_h = xyes; then
		rb_inttypes_h=yes
	fi
	if test x$ac_cv_header_netinet_in_h = xyes; then
		rb_netinet_in_h=yes
	fi

	if test x$ac_cv_header_crypt_h = xyes; then
		rb_crypt_h=yes
	fi
	if test x$ac_cv_header_errno_h = xyes; then
		rb_errno_h=yes
	fi
	if test x$ac_cv_header_unistd_h = xyes; then
		rb_unistd_h=yes
	fi
	if test x$ac_cv_header_windows_h = xyes; then
		rb_windows_h=yes
	fi
	if test x$ac_cv_header_winsock2_h = xyes; then
		rb_winsock2_h=yes
	fi

	rb_socklen_t=$rb_socklen_t

	if test "x$rb_have_sockaddr_storage" = "xyes"; then
		rb_sockaddr_storage="yes" 
	else
		rb_sockaddr_storage="no"
	fi
	
	rb_have_ipv6="$have_v6"
		
	rb_mv="$MV"
	rb_rm="$RM"
		
]
	

)


AC_CONFIG_FILES(		\
	src/Makefile		\
	Makefile		\
	libratbox.pc
)

AC_OUTPUT

dnl Make it look sexay!

echo
echo "Compiling $PACKAGE_NAME $PACKAGE_VERSION"
echo

echo "Installing into: $prefix"

echo "IPv6 support ................... $have_v6"
echo "Assert debugging ............... $assert"
echo "Block allocator ................ $balloc"
echo "SSL Type........................ $SSL_TYPE"
echo

