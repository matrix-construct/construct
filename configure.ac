dnl TODO: clean up all the OpenSSL and shared module checking stuff;
dnl the most major changes have already been made and it looks like
dnl said functions need to be just about as complex as they already are.

AC_PREREQ(2.63)
AUTOMAKE_OPTIONS = 1.14

AC_INIT([charybdis], [5-dev])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADER(include/rb/config.h)
AC_PREFIX_DEFAULT($HOME/ircd)

AC_LANG(C++)
AC_GNU_SOURCE
AC_USE_SYSTEM_EXTENSIONS
AX_CXX_COMPILE_STDCXX_14([ext], [mandatory])
AC_ISC_POSIX
AC_C_INLINE
AC_C_CONST
AC_C_BIGENDIAN
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UID_T

AC_PROG_CXX
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
AC_PATH_PROG(AUTOMAKE, automake)
AC_PATH_PROG(ACLOCAL, aclocal)
AC_PATH_PROG(AUTOHEADER, autoheader)
AC_PATH_PROG(AS, as)
AC_PATH_PROG(RM, rm)
AC_PATH_PROG(CP, cp)
AC_PATH_PROG(MV, mv)
AC_PATH_PROG(LN, ln)
AC_PATH_PROG(TOUCH, touch)

LT_CONFIG_LTDL_DIR([libltdl])
LT_INIT([dlopen disable-static])
LT_LANG([C++])
LTDL_INIT
#LIBTOOL="$LIBTOOL --silent"

AC_CONFIG_FILES(\
	Makefile                \
	include/rb/Makefile     \
	rb/Makefile             \
	authd/Makefile          \
	charybdis/Makefile      \
	ircd/Makefile           \
	bandb/Makefile          \
	doc/Makefile            \
	extensions/Makefile     \
	help/Makefile           \
	modules/Makefile        \
	ssld/Makefile           \
	tools/Makefile          \
	tools/genssl            \
	wsockd/Makefile         \
)

AM_INIT_AUTOMAKE([subdir-objects])
AM_EXTRA_RECURSIVE_TARGETS([
	mrproper       \
])

VERSION_CMD="git describe --tags"
DATESTR_CMD="date -R"
DATECODE_CMD="date +%s"

RB_VERSION=`$VERSION_CMD`
RB_DATESTR=`$DATESTR_CMD`
RB_DATECODE=`$DATECODE_CMD`

AC_DEFUN([RB_DEFINE], [AC_DEFINE([RB_$1], [$2], [$3])])
AC_DEFUN([RB_DEFINE_UNQUOTED], [AC_DEFINE_UNQUOTED([RB_$1], [$2], [$3])])
AC_DEFUN([IRCD_DEFINE], [AC_DEFINE([IRCD_$1], [$2], [$3])])
AC_DEFUN([IRCD_DEFINE_UNQUOTED], [AC_DEFINE_UNQUOTED([IRCD_$1], [$2], [$3])])

RB_DEFINE_UNQUOTED([VERSION], ["$RB_VERSION"], [Version generated at configuration time.])
RB_DEFINE_UNQUOTED([DATESTR], ["$RB_DATESTR"], [Readable date string of configuration time.])
RB_DEFINE_UNQUOTED([DATECODE], [$RB_DATECODE], [UNIX epoch time at configuration time.])

AC_DEFINE([NICKNAMEHISTORYLENGTH], 15000, [Size of the WHOWAS array.])
AC_DEFINE([CHANNEL_HEAP_SIZE],     8192, [Size of the channel heap.])
AC_DEFINE([BAN_HEAP_SIZE],         4096, [Size of the ban heap.])
AC_DEFINE([CLIENT_HEAP_SIZE],      8192, [Size of the client heap.])
AC_DEFINE([LCLIENT_HEAP_SIZE],     1024, [Size of the local client heap.])
AC_DEFINE([PCLIENT_HEAP_SIZE],     256, [Size of the pre-client heap.])
AC_DEFINE([USER_HEAP_SIZE],        8192, [Size of the user heap.])
AC_DEFINE([DNODE_HEAP_SIZE],       8192, [Size of the dlink_node heap.])
AC_DEFINE([TOPIC_HEAP_SIZE],       4096, [Size of the topic heap.])
AC_DEFINE([LINEBUF_HEAP_SIZE],     2048, [Size of the linebuf heap.])
AC_DEFINE([MEMBER_HEAP_SIZE],      32768, [Sizeof member heap.])
AC_DEFINE([ND_HEAP_SIZE],          512, [Size of the nick delay heap.])
AC_DEFINE([CONFITEM_HEAP_SIZE],    256, [Size of the confitem heap.])
AC_DEFINE([MONITOR_HEAP_SIZE],     1024, [Size of the monitor heap.])
AC_DEFINE([FD_HEAP_SIZE],          1024, [Size of fd heap.])
AC_DEFINE([AWAY_HEAP_SIZE],        512, [Size of away heap.])


case "$host_os" in
*cygwin*)
     AC_DEFINE_UNQUOTED(CYGWIN,1,[This is a Cygwin system])
     AC_DEFINE_UNQUOTED(WINDOWS,1,[This is a Windows system])
     ;;
*mingw* | *msys*)
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
     CPPFLAGS="$CPPFLAGS -D_ALL_SOURCE -D_XOPEN_SOURCE=500"
     ;;
*solaris*)
     CPPFLAGS="$CPPFLAGS -D_POSIX_PTHREAD_SEMANTICS -D_XPG4_2"
     ;;
*)
     ;;
esac

AM_CONDITIONAL([MINGW], [test "$is_mingw" = "yes"])


dnl Checks for system header files.
AC_DEFUN([RB_CHK_SYSHEADER], \
[
	AC_CHECK_HEADER([$1], \
	[
		AC_DEFINE([HAVE_$2], [1], [ Indication $1 is available. ])
		RB_DEFINE_UNQUOTED([INC_$2], [$1>], [ The computed-include location of $1. ])
	],[
		if test "$is_mingw" = "yes"; then
			RB_DEFINE_UNQUOTED([INC_$2], [stdint.h>], [ The dead-header in place of $1. ])
		else
			RB_DEFINE_UNQUOTED([INC_$2], [/dev/null>], [ The dead-header in place of $1. ])
		fi
	])
])

RB_CHK_SYSHEADER([errno.h],        [ERRNO_H])
RB_CHK_SYSHEADER([assert.h],       [ASSERT_H])
RB_CHK_SYSHEADER([stddef.h],       [STDDEF_H])
RB_CHK_SYSHEADER([stdarg.h],       [STDARG_H])
RB_CHK_SYSHEADER([stdint.h],       [STDINT_H])
RB_CHK_SYSHEADER([inttypes.h],     [INTTYPES_H])
RB_CHK_SYSHEADER([ctype.h],        [CTYPE_H])
RB_CHK_SYSHEADER([limits.h],       [LIMITS_H])
RB_CHK_SYSHEADER([stdlib.h],       [STDLIB_H])
RB_CHK_SYSHEADER([unistd.h],       [UNISTD_H])
RB_CHK_SYSHEADER([time.h],         [TIME_H])
RB_CHK_SYSHEADER([fcntl.h],        [FCNTL_H])
RB_CHK_SYSHEADER([signal.h],       [SIGNAL_H])
RB_CHK_SYSHEADER([dirent.h],       [DIRENT_H])
RB_CHK_SYSHEADER([sys/types.h],    [SYS_TYPES_H])
RB_CHK_SYSHEADER([sys/time.h],     [SYS_TIME_H])
RB_CHK_SYSHEADER([sys/stat.h],     [SYS_STAT_H])
RB_CHK_SYSHEADER([sys/file.h],     [SYS_FILE_H])
RB_CHK_SYSHEADER([sys/param.h],    [SYS_PARAM_H])
RB_CHK_SYSHEADER([sys/resource.h], [SYS_RESOURCE_H])
RB_CHK_SYSHEADER([sys/socket.h],   [SYS_SOCKET_H])
RB_CHK_SYSHEADER([arpa/inet.h],    [ARPA_INET_H])
RB_CHK_SYSHEADER([netinet/in.h],   [NETINET_IN_H])
RB_CHK_SYSHEADER([netinet/tcp.h],  [NETINET_TCP_H])
RB_CHK_SYSHEADER([string.h],       [STRING_H])
RB_CHK_SYSHEADER([strings.h],      [STRINGS_H])
RB_CHK_SYSHEADER([stdio.h],        [STDIO_H])
RB_CHK_SYSHEADER([crypt.h],        [CRYPT_H])
RB_CHK_SYSHEADER([sys/uio.h],      [SYS_UIO_H])
RB_CHK_SYSHEADER([spawn.h],        [SPAWN_H])
RB_CHK_SYSHEADER([sys/poll.h],     [SYS_POLL_H])
RB_CHK_SYSHEADER([sys/epoll.h],    [SYS_EPOLL_H])
RB_CHK_SYSHEADER([sys/select.h],   [SYS_SELECT_H])
RB_CHK_SYSHEADER([sys/devpoll.h],  [SYS_DEVPOLL_H])
RB_CHK_SYSHEADER([sys/event.h],    [SYS_EVENT_H])
RB_CHK_SYSHEADER([port.h],         [PORT_H])
RB_CHK_SYSHEADER([sys/signalfd.h], [SYS_SIGNALFD_H])
RB_CHK_SYSHEADER([sys/timerfd.h],  [SYS_TIMERFD_H])
RB_CHK_SYSHEADER([execinfo.h],     [EXECINFO_H])

RB_CHK_SYSHEADER([cstddef],        [CSTDDEF])
RB_CHK_SYSHEADER([cstdint],        [CSTDINT])
RB_CHK_SYSHEADER([limits],         [LIMITS])
RB_CHK_SYSHEADER([type_traits],    [TYPE_TRAITS])
RB_CHK_SYSHEADER([utility],        [UTILITY])
RB_CHK_SYSHEADER([functional],     [FUNCTIONAL])
RB_CHK_SYSHEADER([algorithm],      [ALGORITHM])
RB_CHK_SYSHEADER([memory],         [MEMORY])
RB_CHK_SYSHEADER([exception],      [EXCEPTION])
RB_CHK_SYSHEADER([cerrno],         [CERRNO])
RB_CHK_SYSHEADER([system_error],   [SYSTEM_ERROR])
RB_CHK_SYSHEADER([map],            [MAP])
RB_CHK_SYSHEADER([set],            [SET])
RB_CHK_SYSHEADER([list],           [LIST])
RB_CHK_SYSHEADER([deque],          [DEQUE])
RB_CHK_SYSHEADER([array],          [ARRAY])
RB_CHK_SYSHEADER([vector],         [VECTOR])
RB_CHK_SYSHEADER([forward_list],   [FORWARD_LIST])
RB_CHK_SYSHEADER([unordered_map],  [UNORDERED_MAP])
RB_CHK_SYSHEADER([string],         [STRING])
RB_CHK_SYSHEADER([sstream],        [SSTREAM])
RB_CHK_SYSHEADER([fstream],        [FSTREAM])
RB_CHK_SYSHEADER([iostream],       [IOSTREAM])
RB_CHK_SYSHEADER([cstdio],         [CSTDIO])

RB_CHK_SYSHEADER([windows.h],      [WINDOWS_H])
RB_CHK_SYSHEADER([winsock2.h],     [WINSOCK2_H])
RB_CHK_SYSHEADER([ws2tcpip.h],     [WS2TCPIP_H])
RB_CHK_SYSHEADER([iphlpapi.h],     [IPHLPAPI_H])


dnl todo
AC_DEFINE([HAVE_BOOST_ASIO_HPP], [1], [ boost asio. ])
RB_DEFINE_UNQUOTED([INC_BOOST_ASIO_HPP], [boost/asio.hpp>], [ boost asio. ])

AC_DEFINE([HAVE_BOOST_CONTEXT_ALL_HPP], [1], [ boost context. ])
RB_DEFINE_UNQUOTED([INC_BOOST_CONTEXT_ALL_HPP], [boost/context/all.hpp>], [ boost context. ])

AC_DEFINE([HAVE_BOOST_ASIO_SPAWN_HPP], [1], [ boost asio spawn. ])
RB_DEFINE_UNQUOTED([INC_BOOST_ASIO_SPAWN_HPP], [boost/asio/spawn.hpp>], [ boost asio spawn. ])

AC_DEFINE([HAVE_BOOST_LEXICAL_CAST_HPP], [1], [ boost asio. ])
RB_DEFINE_UNQUOTED([INC_BOOST_LEXICAL_CAST_HPP], [boost/lexical_cast.hpp>], [ boost lexical_cast. ])

AC_DEFINE([HAVE_BOOST_TOKENIZER_HPP], [1], [ boost tokenizer. ])
RB_DEFINE_UNQUOTED([INC_BOOST_TOKENIZER_HPP], [boost/tokenizer.hpp>], [ boost tokenizer. ])


dnl use directory structure of cached as default (hack)
if test "$libexecdir" = '${exec_prefix}/libexec' &&
   test "$localstatedir" = '${prefix}/var'; then
	libexecdir='${bindir}'
	localstatedir='${prefix}'
fi


dnl Networking Functions
dnl ====================

AC_SEARCH_LIBS(socket, [socket],,)


AC_CHECK_MEMBER([struct sockaddr.sa_len], [AC_DEFINE(SOCKADDR_IN_HAS_LEN, 1, [Define to 1 if sockaddr has a 'sa_len' member.])],,[[
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
#include <ws2tcpip.h>
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



AC_ARG_ENABLE(ipv6, AC_HELP_STRING([--disable-ipv6], [Disable IPv6 support]), [ipv6=$enableval], [ipv6=yes])

dnl TODO: IPV6 CHECKS

if test x$ipv6 = "Xyes"; then
	AC_DEFINE([HAVE_IPV6], [1], [IPv6 is supported])
else
	AC_DEFINE([HAVE_IPV6], [0], [IPv6 not supported])
fi


AC_CHECK_TYPES([struct sockaddr_storage],[
	rb_have_sockaddr_storage=yes
        ],[], [
		#include <sys/types.h>
		#include <sys/socket.h>
	])


dnl Check for stdarg.h - if we cant find it, halt configure
AC_CHECK_HEADER(stdarg.h, , [AC_MSG_ERROR([** stdarg.h could not be found - librb will not compile without it **])])

AC_CHECK_TYPE([sa_family_t], [],
[AC_DEFINE(sa_family_t, [u_int16_t], [If system does not define sa_family_t, define it here.])],
[[#include <sys/types.h>
#include <sys/socket.h>]])


dnl check for various functions...
AC_CHECK_FUNCS([snprintf vsnprintf socketpair gettimeofday writev sendmsg gmtime_r strtok_r usleep posix_spawn getexecname strlcpy strlcat strnlen strcasestr strcasecmp strncasecmp fstat signalfd select poll kevent port_create epoll_ctl arc4random getrusage timerfd_create])	

AC_SEARCH_LIBS(dlinfo, dl, AC_DEFINE(HAVE_DLINFO, 1, [Define if you have dlinfo]))
AC_SEARCH_LIBS(nanosleep, rt posix4, AC_DEFINE(HAVE_NANOSLEEP, 1, [Define if you have nanosleep]))
AC_SEARCH_LIBS(timer_create, rt, AC_DEFINE(HAVE_TIMER_CREATE, 1, [Define if you have timer_create]))

AC_DEFUN([RB_CHECK_TIMER_CREATE],
  [AC_CACHE_CHECK([for a working timer_create(CLOCK_REALTIME)],
    [rb__cv_timer_create_works],
    [AC_TRY_RUN([
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
int main(int argc, char *argv[])
{
#if HAVE_TIMER_CREATE
    struct sigevent ev;
    timer_t timer;
    ev.sigev_notify = SIGEV_SIGNAL;
    ev.sigev_signo  = SIGVTALRM;
    if (timer_create(CLOCK_REALTIME, &ev, &timer) != 0) {
       return 1;
    }
#else
    return 1;
#endif
    return 0;
}
     ],
     [rb__cv_timer_create_works=yes],
     [rb__cv_timer_create_works=no],
     [rb__cv_timer_create_works=no])
  ])
case $rb__cv_timer_create_works in
    yes) AC_DEFINE([USE_TIMER_CREATE], 1,
                   [Define to 1 if we can use timer_create(CLOCK_REALTIME,...)]);;
esac
])



AC_DEFUN([RB_CHECK_TIMERFD_CREATE],
  [AC_CACHE_CHECK([for a working timerfd_create(CLOCK_REALTIME)],
    [rb__cv_timerfd_create_works],
    [AC_TRY_RUN([
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TIMERFD_H
#include <sys/timerfd.h>
#endif
int main(int argc, char *argv[])
{
#if defined(HAVE_TIMERFD_CREATE) && defined(HAVE_SYS_TIMERFD_H)
    if (timerfd_create(CLOCK_REALTIME, 0) < 0) {
       return 1;
    }
#else
    return 1;
#endif
    return 0;
}
     ],
     [rb__cv_timerfd_create_works=yes],
     [rb__cv_timerfd_create_works=no],
     [rb__cv_timerfd_create_works=no])
  ])
case $rb__cv_timerfd_create_works in
    yes) AC_DEFINE([USE_TIMERFD_CREATE], 1,
                   [Define to 1 if we can use timerfd_create(CLOCK_REALTIME,...)]);;
esac
])

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
		[AC_LANG_PROGRAM(
	    	[#include <openssl/opensslv.h>
		#include <stdlib.h>],
		[[if (OPENSSL_VERSION_NUMBER >= 0x00907000)
		exit(0); else exit(1);]])],
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


dnl mbedTLS support
AC_ARG_ENABLE(mbedtls,
[AC_HELP_STRING([--enable-mbedtls], [Enable mbedTLS support.])
AC_HELP_STRING([--disable-mbedtls], [Disable mbedTLS support.])],
[cf_enable_mbedtls=$enableval],
[cf_enable_mbedtls="auto"])

if test "$cf_enable_mbedtls" != no; then
	save_CPPFLAGS="$CPPFLAGS"
	save_LIBS="$LIBS"
	LIBS="$LIBS $MBEDTLS_LIBS"
	AC_CHECK_LIB(mbedtls, mbedtls_ssl_init, [
		MBEDTLS_LIBS="$MBEDTLS_LIBS -lmbedtls -lmbedx509 -lmbedcrypto"
		cf_enable_mbedtls=yes
	], [cf_enable_mbedtls=no], [-lmbedx509 -lmbedcrypto])
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

dnl mbed TLS is given highest preference when no specified TLS library is provided
if test "$cf_enable_openssl" = "auto" -a "$cf_enable_mbedtls" = "yes"; then
       cf_enable_openssl="no"
fi

if test "$cf_enable_gnutls" = "auto" -a "$cf_enable_openssl" = "yes" -a "$cf_enable_mbedtls" = "yes"; then
       cf_enable_gnutls="no"
fi

if test x"$cf_enable_openssl" != xno; then
	AC_DEFINE(HAVE_OPENSSL,1,[Has OpenSSL])
	GNUTLS_CFLAGS=""
	GNUTLS_LIBS=""
	MBEDTLS_LIBS=""
	MBEDTLS_CFLAGS=""
	SSL_TYPE="openssl"
elif test x"$cf_enable_mbedtls" != xno; then
	AC_DEFINE(HAVE_MBEDTLS, 1, [Has mbedTLS])
	SSL_LIBS=""
	SSL_CFLAGS=""
	GNUTLS_CFLAGS=""
	GNUTLS_LIBS=""
	SSL_TYPE="mbedtls"
elif test x"$cf_enable_gnutls" != xno; then
	AC_DEFINE(HAVE_GNUTLS, 1, [Has GnuTLS])
	SSL_LIBS=""
	SSL_CFLAGS=""
	MBEDTLS_LIBS=""
	MBEDTLS_CFLAGS=""
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
		CPPFLAGS="$CPPFLAGS -pg -static"
		AC_MSG_RESULT([yes, adding -pg -static])
		AC_DEFINE(RB_PROFILE, 1, [Defined to mark profiling is enabled])
	else
		AC_MSG_RESULT([no, profile builds only work with gcc])
	fi
else
	AC_MSG_RESULT(no)
fi

AC_SUBST(LDFLAGS)
AC_SUBST(PICFLAGS)
AC_SUBST(SEDOBJ)
AC_SUBST(SSL_CFLAGS)
AC_SUBST(SSL_LIBS)
AC_SUBST(GNUTLS_CFLAGS)
AC_SUBST(GNUTLS_LIBS)
AC_SUBST(MBEDTLS_CFLAGS)
AC_SUBST(MBEDTLS_LIBS)

if test "$prefix" = "NONE"; then
	AC_DEFINE_UNQUOTED(RB_PREFIX, "$ac_default_prefix", [Prefix where librb is installed.])
else

dnl Don't get bitten by Cygwin's stupidity if the user specified
dnl a custom prefix with a trailing slash

	prefix=`echo $prefix | sed 's/\/$//'`
	AC_DEFINE_UNQUOTED(RB_PREFIX, "$prefix", [Prefix where librb is installed.])

fi

AC_SUBST(RB_PREFIX)


PKG_PROG_PKG_CONFIG
build_ltdl=$with_included_ltdl
AM_CONDITIONAL([BUILD_LTDL], [test x"$build_ltdl" = x"yes"])

case "$host_os" in
*cygwin*)
     AC_DEFINE_UNQUOTED(CYGWIN,1,[This is a Cygwin system])
     AC_DEFINE_UNQUOTED(WINDOWS,1,[This is a Windows system])
     ;;
*mingw* | *msys*)
     AC_DEFINE_UNQUOTED(MINGW,1,[This is a MinGW system])
     AC_DEFINE_UNQUOTED(WINDOWS,1,[This is a Windows system])
     AC_CHECK_HEADER(winsock2.h, , [AC_MSG_ERROR([** MinGW and no winsock2.h.  I give up.])])
     LIBS="$LIBS -lws2_32 -liphlpapi"
     is_mingw="yes"
     ;;
*)
     ;;
esac

AM_CONDITIONAL([MINGW], [test "$is_mingw" = "yes"])

dnl check for /dev/null so we can use it to hold evil fd's
AC_MSG_CHECKING([for /dev/null])
if test -c /dev/null ; then
	AC_DEFINE(PATH_DEVNULL, "/dev/null", [Path to /dev/null])
	AC_MSG_RESULT(yes)
else
	AC_DEFINE(PATH_DEVNULL, "devnull.log", [Path to /dev/null])
	AC_MSG_RESULT(no - using devnull.log)
fi

AC_PROG_YACC

dnl AC_PROG_YACC defaults to yacc unconditionally if nothing can be found
if test "$YACC" = "yacc" -a -z "`which $YACC 2>/dev/null`"; then
	AC_MSG_ERROR([could not locate a suitable parser generator; install bison, yacc, or byacc])
fi

AC_PROG_LEX

if test "$LEX" = ":"; then
	AC_MSG_ERROR([could not locate a suitable lexical generator, install flex or lex.])
fi

AC_ARG_ENABLE([fhs-paths],
	[AS_HELP_STRING([--enable-fhs-paths], [Use more FHS-like pathnames (for packagers).])],
	[],
	[dnl detect if the user appears to want --enable-fhs-paths
	AS_IF([test "$libexecdir" = '${exec_prefix}/libexec' && \
		test "$localstatedir" = '${prefix}/var' && \
		test "$libdir" = '${exec_prefix}/lib'],
		[enable_fhs_paths=no],
		[enable_fhs_paths=yes])
	])
dnl use directory structure of cached as default (hack)
AS_IF([test "x$enable_fhs_paths" = "xyes"],
	[dnl Avoid name collisions.
	pkglibexecdir='${libexecdir}/${PACKAGE_TARNAME}'
	rundir=${rundir-'${prefix}/run'}
	pkgrundir='${rundir}/${PACKAGE_TARNAME}'
	pkglocalstatedir='${localstatedir}/${PACKAGE_TARNAME}'
	AC_DEFINE([ENABLE_FHS_PATHS], [1], [Uncomment if FHS pathnames are enabled])],
	[libexecdir='${bindir}'
	pkglibexecdir='${libexecdir}'
	rundir='${sysconfdir}'
	pkgrundir='${rundir}'
	localstatedir='${prefix}'
	pkglocalstatedir='${sysconfdir}'])
pkglibdir='${libdir}/${PACKAGE_TARNAME}'
AC_SUBST([pkglibdir])
AC_SUBST([rundir])
AC_SUBST([pkgrundir])
AC_SUBST([pkglocalstatedir])
AC_DEFINE_DIR([PKGLOCALSTATEDIR], [pkglocalstatedir], [[Directory in which to store state, such as ban database]])
AC_SUBST([pkglibexecdir])
AC_DEFINE_DIR([PKGLIBEXECDIR], [pkglibexecdir], [Directory where binaries the IRCd itself spawns live])

dnl Checks for header files.
AC_HEADER_STDC
AC_HEADER_STDBOOL

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

dnl Networking Functions
dnl ====================

AC_SEARCH_LIBS(socket, [socket ws2_32], , [AC_MSG_ERROR([You have no socket()! Aborting.])])

dnl SunOS/Solaris required libnsl for inet_ntoa()
if test x"$SUN" = xyes; then
	AC_SEARCH_LIBS(inet_ntoa, nsl,, [AC_MSG_ERROR([libnsl not found! Aborting.])])
fi

AC_SEARCH_LIBS(crypt, [crypt descrypt],,)

CRYPT_LIB=$ac_cv_search_crypt

if test "$CRYPT_LIB" = "none required"; then
	unset CRYPT_LIB
elif test "$CRYPT_LIB" = no; then
	unset CRYPT_LIB
fi

AC_SUBST(CRYPT_LIB)

AC_C_BIGENDIAN

dnl Check for stdarg.h - if we can't find it, halt configure
AC_CHECK_HEADER(stdarg.h, , [AC_MSG_ERROR([** stdarg.h could not be found - charybdis will not compile without it **])])
AC_CHECK_FUNCS([strlcat strlcpy])

AC_TYPE_INT16_T
AC_TYPE_INT32_T

AC_CHECK_TYPE([sa_family_t], [],
[AC_DEFINE(sa_family_t, [u_int16_t], [If system does not define sa_family_t, define it here.])],
[[#include <sys/types.h>
#include <sys/socket.h>]])

AC_CHECK_TYPES([uintptr_t])

dnl check for various functions...
AC_CHECK_FUNCS([snprintf vsnprintf socketpair mmap gettimeofday strdup strndup ])

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
		[AC_LANG_PROGRAM(
	    	[#include <openssl/opensslv.h>
		#include <stdlib.h>],
		[[if ( OPENSSL_VERSION_NUMBER >= 0x00906000)
		exit(0); else exit(1);]])],
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

dnl Check for shared sqlite
dnl ======================
AC_ARG_WITH(shared-sqlite,
AC_HELP_STRING([--with-shared-sqlite],[Use shared sqlite]),
[shared_sqlite=$withval],[shared_sqlite=no])

if test "$shared_sqlite" = yes; then
	PKG_CHECK_MODULES(SQLITE, [sqlite3],
	[
		shared_sqlite=yes
	], shared_sqlite=no)
fi

if test "$shared_sqlite" = no; then
	SQLITE_OBJ='sqlite3.$(OBJEXT)'
fi

AC_SUBST(SQLITE_LD, "$SQLITE_LIBS")
AC_SUBST(SQLITE_INCLUDES, "$SQLITE_CFLAGS")
AC_SUBST(SQLITE_OBJ)

dnl Check for PCRE2
dnl ===============

PKG_CHECK_MODULES(PCRE, [libpcre2-8], [have_pcre="yes"], [have_pcre="no"])
AM_CONDITIONAL([PCRE], [test "x$have_pcre" = "xyes"])

dnl **********************************************************************
dnl Check for --with-confdir [deprecated, use --sysconfdir instead]
dnl **********************************************************************

AC_ARG_WITH([confdir],
	[AC_HELP_STRING([--with-confdir=DIR],
	       [Directory to install config files [deprecated, use --sysconfdir instead].])],
	[ sysconfdir=`echo $withval | sed 's/\/$//'` ],
	[ confdir='etc' ])
RB_DEFINE_UNQUOTED([ETC_DIR], ["${prefix}/${confdir}"], [Prefix where config files are installed.])

dnl **********************************************************************
dnl Check for --with-bindir
dnl **********************************************************************

AC_MSG_CHECKING([whether to modify binary directory])
AC_ARG_WITH(bindir,
AC_HELP_STRING([--with-bindir=DIR],
	       [Directory where binary executables are placed.]),
	       [ logdir=`echo $withval | sed 's/\/$//'`
		 AC_MSG_RESULT(yes)],
	       [ AS_IF([test "x$enable_fhs_paths" = "xyes"],
			[bindir="${prefix}/bin"],
			[bindir="${prefix}/bin"])
		 AC_MSG_RESULT(no)])
RB_DEFINE_UNQUOTED([BIN_DIR], ["${bindir}"], [Directory where binary executables are to be found.])
AC_SUBST_DIR([bindir])


dnl **********************************************************************
dnl Check for --with-logdir
dnl **********************************************************************

AC_MSG_CHECKING([whether to modify logdir])
AC_ARG_WITH(logdir,
AC_HELP_STRING([--with-logdir=DIR],
	       [Directory where to write logfiles.]),
	       [ logdir=`echo $withval | sed 's/\/$//'`
		 AC_MSG_RESULT(yes)],
	       [ AS_IF([test "x$enable_fhs_paths" = "xyes"],
			[logdir="${prefix}/var/log"],
			[logdir="${prefix}/var/${PACKAGE_TARNAME}/log"])
		 AC_MSG_RESULT(no)])
RB_DEFINE_UNQUOTED([LOG_DIR], ["${logdir}"], [Prefix where to write logfiles.])
AC_SUBST_DIR([logdir])

dnl **********************************************************************
dnl Check for --with-helpdir
dnl **********************************************************************

AC_MSG_CHECKING([whether to modify helpdir])
AC_ARG_WITH(helpdir,
AC_HELP_STRING([--with-helpdir=DIR],
	       [Directory to install help files.]),
	       [ helpdir=`echo $withval | sed 's/\/$//'`
		 AC_MSG_RESULT(yes) ],
	       [ AS_IF([test "x$enable_fhs_paths" = "xyes"],
			[helpdir="${prefix}/share/help"],
			[helpdir="${prefix}/share/${PACKAGE_TARNAME}/help"])
		 AC_MSG_RESULT(no) ])
RB_DEFINE_UNQUOTED([HELP_DIR], ["${helpdir}"], [Prefix where help files are installed.])
AC_SUBST_DIR([helpdir])

dnl **********************************************************************
dnl Check for --with-moduledir
dnl **********************************************************************

AC_MSG_CHECKING([whether to modify moduledir])
AC_ARG_WITH(moduledir,
	[AC_HELP_STRING([--with-moduledir=DIR],
	       [Directory to install modules.])],
	       [ moduledir=`echo $withval | sed 's/\/$//'`
		 AC_MSG_RESULT(yes)],
	       [ AS_IF([test "x$enable_fhs_paths" = "xyes"],
			[moduledir="${prefix}/lib/modules"],
			[moduledir="${prefix}/lib/${PACKAGE_TARNAME}/modules"])
		 AC_MSG_RESULT(no)
		])
RB_DEFINE_UNQUOTED([MODULE_DIR], ["${moduledir}"], [Prefix where modules are installed.])
AC_SUBST_DIR([moduledir])

dnl Check for --with-rundir

AC_MSG_CHECKING([whether or modify rundir])
AC_ARG_WITH([rundir],
	[AC_HELP_STRING([--with-rundir=DIR],
		[Directory in which to store pidfile.])],
	[AC_MSG_RESULT([yes])
	rundir=`echo $withval | sed 's/\/$//'`],
	[AC_MSG_RESULT([no])
	AS_IF([test "x$enable_fhs_paths" = "xyes"],
		[rundir="${prefix}/var/run"],
		[rundir="${prefix}/${PACKAGE_TARNAME}/run"])])
AC_SUBST([rundir])
AC_DEFINE_DIR([PKGRUNDIR], ["${rundir}"], [Directory to store pidfile in.])

dnl Installed utility program prefixes (does not affect binaries
dnl installed into pkglibexecdir)
AC_MSG_CHECKING([for program prefix])
AC_ARG_WITH([program-prefix],
	[AS_HELP_STRING([--with-program-prefix=], [If set, programs installed into PATH will be installed with names prefixed by this prefix.])],
	[test "x$with_program_prefix" = "xno" && with_program_prefix=],
	[with_program_prefix=])
AC_MSG_RESULT(["$with_program_prefix"])
PROGRAM_PREFIX="$with_program_prefix"
AC_SUBST([PROGRAM_PREFIX])
AC_DEFINE_UNQUOTED([PROGRAM_PREFIX], ["$with_program_prefix"], [String with which all programs intended to be in PATH are prefixed.])

dnl **********************************************************************
dnl Branding
dnl **********************************************************************

AC_MSG_CHECKING([whether custom branding is requested])
AC_ARG_WITH(custom-branding,
AC_HELP_STRING([--with-custom-branding=NAME],
	       [Custom branding name.]),
	       [BRANDING_NAME=$withval
		AC_MSG_RESULT([yes])],
	       [BRANDING_NAME=$PACKAGE_NAME
		AC_MSG_RESULT([no])]
)

AC_MSG_CHECKING([whether a custom version is requested])
AC_ARG_WITH(custom-version,
AC_HELP_STRING([--with-custom-version=NAME],
	       [Custom version branding.]),
	       [BRANDING_VERSION=$withval
		AC_MSG_RESULT([yes])],
	       [BRANDING_VERSION=$PACKAGE_VERSION
		AC_MSG_RESULT([no])]
)

AC_DEFINE_UNQUOTED(BRANDING_NAME, ["$BRANDING_NAME"], [Custom branding name.])
AC_DEFINE_UNQUOTED(BRANDING_VERSION, ["$BRANDING_VERSION"], [Custom branding name.])

if test "x$BRANDING_NAME" != "x$PACKAGE_NAME"; then
	AC_DEFINE(CUSTOM_BRANDING, 1, [Define if custom branding is enabled.])
fi

dnl **********************************************************************
dnl Enable oper chghost
dnl **********************************************************************

AC_ARG_ENABLE(oper-chghost,
AC_HELP_STRING([--enable-oper-chghost],[Enable opers to use the CHGHOST command]),
[operchghost=$enableval],[operchghost=no])

if test "$operchghost" = yes; then
	AC_DEFINE(ENABLE_OPER_CHGHOST, 1, [Define this to enable opers to use the CHGHOST command.])
fi

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
		CPPFLAGS="$CPPFLAGS -pg"
		AC_MSG_RESULT([yes, adding -pg])
		AC_DEFINE(CHARYBDIS_PROFILE, 1, [Define this if you are profiling.])
	else
		AC_MSG_RESULT([no, profile builds only work with gcc])
	fi
else
	AC_MSG_RESULT(no)
fi

AC_ARG_WITH(nicklen,
AC_HELP_STRING([--with-nicklen=LENGTH],[Set the upper-bound nick length to LENGTH (default 31, max 50)]),
[
  if ! expr "$withval" + 0 >/dev/null 2>&1; then
	AC_ERROR([NICKLEN must be a numeric value])
  fi
  if test $withval -gt 50; then
	NICKLEN=50
	AC_MSG_WARN([NICKLEN has a hard limit of 50. Setting NICKLEN=50])
  elif test $withval -lt 9; then
	NICKLEN=9
	AC_MSG_WARN([NICKLEN has a lower limit of 9. Setting NICKLEN=9])
  else
	NICKLEN="$withval"
  fi
], [NICKLEN=31])

AC_ARG_WITH(topiclen,
AC_HELP_STRING([--with-topiclen=NUMBER],[Set the max topic length to NUMBER (default 390, max 390)]),
[
 if test $withval -gt 390; then
	TOPICLEN=390
	AC_MSG_WARN([TOPICLEN has a hard limit of 390. Setting TOPICLEN=390])
 else
 	TOPICLEN=$withval
 fi
], [TOPICLEN=390])

AC_DEFINE_UNQUOTED(TOPICLEN, ${TOPICLEN}, [Maximum topic length (<=390)])
AC_DEFINE_UNQUOTED(NICKLEN, (${NICKLEN}+1), [Nickname length])

# rpath, for finding librb.so at run time
hold_ldflags=$LDFLAGS
#AC_MSG_CHECKING(for the ld -rpath flag)
#LDFLAGS="${LDFLAGS} -Wl,-rpath=${prefix}/${libdir}"
#AC_LINK_IFELSE([AC_LANG_PROGRAM([],[int i;])], found=yes, found=no)
#LDFLAGS=$hold_ldflags
#AC_MSG_RESULT($found)
#if test "$found" = yes; then
#	LDFLAGS="${LDFLAGS} -Wl,-rpath=${prefix}/${libdir}"
#fi

# This must be down here, or it will mess up checks like the ones
# for -Wl,-export-dynamic
# -- jilles
CWARNS=""
AC_ARG_ENABLE(warnings,
AC_HELP_STRING([--enable-warnings],[Enable all sorts of warnings for debugging.]),
[
CHARYBDIS_C_GCC_TRY_FLAGS([-Wall], charybdis_cv_c_gcc_w_all)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wextra], charybdis_cv_c_gcc_w_extra)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wpointer-arith], charybdis_cv_c_gcc_w_pointer_arith)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wcast-align], charybdis_cv_c_gcc_w_cast_align)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wcast-qual], charybdis_cv_c_gcc_w_cast_qual)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wfloat-equal], charybdis_cv_c_gcc_w_float_equal)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wwrite-strings], charybdis_cv_c_gcc_w_write_strings)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wmissing-declarations], charybdis_cv_c_gcc_w_prototypes)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wno-missing-field-initializers], charybdis_cv_c_gcc_w_missing_field_initializers)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wparentheses], charybdis_cv_c_gcc_parentheses)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wmissing-noreturn], charybdis_cv_c_gcc_w_missing_noreturn)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wundef], charybdis_cv_c_gcc_w_undef)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wpacked], charybdis_cv_c_gcc_w_packed)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wno-unused -Wno-unused-function -Wno-unused-label -Wno-unused-value -Wno-unused-variable -Wno-unused-parameter], charybdis_cv_c_gcc_w_unused)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wformat -Wformat-y2k -Wformat-security -Wformat-nonliteral], charybdis_cv_c_gcc_w_format)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wsuggest-attribute=format], charybdis_cv_c_gcc_w_suggest_attribute_format)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wstrict-aliasing -Wstrict-overflow], charybdis_cv_c_gcc_w_strict)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wdisabled-optimization], charybdis_cv_c_gcc_w_disabled_optimization)
CHARYBDIS_C_GCC_TRY_FLAGS([-Winvalid-pch], charybdis_cv_c_gcc_w_invalid_pch)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wlogical-op], charybdis_cv_c_gcc_w_logical_op)
CHARYBDIS_C_GCC_TRY_FLAGS([-Winit-self], charybdis_cv_c_gcc_w_init_self)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wuninitialized], charybdis_cv_c_gcc_w_uninitialized)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wunreachable-code], charybdis_cv_c_gcc_w_unreachable_code)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wno-unknown-warning -Wno-unknown-warning-option], charybdis_cv_c_gcc_w_unknown_warning)
],[])

CPPFLAGS="$CPPFLAGS $CWARNS"

AC_SUBST(MODULES_LIBS)
AC_SUBST(MOD_TARGET)

AC_SUBST(SSL_SRCS_ENABLE)
AC_SUBST(SSL_INCLUDES)
AC_SUBST(SSL_LIBS)

AC_SUBST(CFLAGS)
AC_SUBST(CPPFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(PICFLAGS)
AC_SUBST(SEDOBJ)


if test "$prefix" = "NONE"; then
	AC_DEFINE_UNQUOTED(IRCD_PREFIX, "$ac_default_prefix", [Prefix where the ircd is installed.])

else

dnl Don't get bitten by Cygwin's stupidity if the user specified
dnl a custom prefix with a trailing slash

	prefix=`echo $prefix | sed 's/\/$//'`
	AC_DEFINE_UNQUOTED(IRCD_PREFIX, "$prefix", [Prefix where the ircd is installed.])

fi

AC_CONFIG_COMMANDS([tools/genssl_chmod], [chmod 755 tools/genssl])

AC_OUTPUT
dnl Make it look sexay!

echo
echo "Compiling $PACKAGE_NAME $PACKAGE_VERSION"
echo $RB_VERSION
echo $RB_DATESTR
echo

echo "Installing into: $prefix"

echo "IPv6 support ................... $ipv6"
echo "Assert debugging ............... $assert"
echo "SSL Type........................ $SSL_TYPE"
echo
