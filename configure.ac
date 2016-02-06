dnl TODO: clean up all the OpenSSL and shared module checking stuff;
dnl the most major changes have already been made and it looks like
dnl said functions need to be just about as complex as they already are.

AC_PREREQ(2.60)

AC_INIT([charybdis], [3.6.0-dev])

AC_LANG(C)
AC_PROG_CC_C99
AC_USE_SYSTEM_EXTENSIONS
AC_GNU_SOURCE

AC_PREFIX_DEFAULT($HOME/ircd)
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADER(include/setup.h)
AC_CONFIG_SUBDIRS([libratbox])
AC_CONFIG_AUX_DIR([libltdl/config])
AC_CONFIG_MACRO_DIR([libltdl/m4])

AM_INIT_AUTOMAKE([subdir-objects])
AM_SILENT_RULES([yes])
AM_PROG_AR

LT_INIT([dlopen disable-static])
LT_CONFIG_LTDL_DIR([libltdl])
LTDL_INIT

build_ltdl=$with_included_ltdl
AM_CONDITIONAL([BUILD_LTDL], [test x"$build_ltdl" = x"yes"])

if test "$ac_cv_c_compiler_gnu" = yes; then
	IRC_CFLAGS="$IRC_CFLAGS -O0 -Wall"
fi

dnl If we support -g, use it!
if test "$ac_cv_prog_cc_g" = yes; then
	IRC_CFLAGS="$IRC_CFLAGS -g"
fi

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
	pkglocalstatedir='${localstatedir}/${PACKAGE_TARNAME}'],
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

AC_SEARCH_LIBS(socket, socket, , [AC_MSG_ERROR([You have no socket()! Aborting.])])

dnl SunOS/Solaris required libnsl for inet_ntoa()
if test x"$SUN" = xyes; then
	AC_SEARCH_LIBS(inet_ntoa, nsl,, [AC_MSG_ERROR([libnsl not found! Aborting.])])
fi

AC_CHECK_TYPE(socklen_t, ,
[AC_DEFINE([socklen_t], [unsigned int],
[If we don't have a real socklen_t, unsigned int is good enough.])],
[#include <sys/types.h>
#include <sys/socket.h>])

AC_ARG_ENABLE(ipv6,
AC_HELP_STRING([--enable-ipv6],[Enable IPv6 support]),[ipv6=$enableval],[ipv6=no])

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

dnl **********************************************************************
dnl Check for --with-confdir [deprecated, use --sysconfdir instead]
dnl **********************************************************************

AC_ARG_WITH([confdir],
	[AC_HELP_STRING([--with-confdir=DIR],
	       [Directory to install config files [deprecated, use --sysconfdir instead].])],
	[ sysconfdir=`echo $withval | sed 's/\/$//'` ],
	[ confdir='${sysconfdir}' ])
AC_DEFINE_DIR([ETC_DIR], [sysconfdir], [Prefix where config files are installed.])

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
			[logdir='${localstatedir}/log/${PACKAGE_TARNAME}'],
			[logdir='${prefix}/logs'])
		 AC_MSG_RESULT(no)])
AC_DEFINE_DIR([LOG_DIR], [logdir], [Prefix where to write logfiles.])
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
			[helpdir='${datadir}/${PACKAGE_TARNAME}/help'],
			[helpdir='${prefix}/help'])
		 AC_MSG_RESULT(no) ])
AC_DEFINE_DIR([HELP_DIR], [helpdir], [Prefix where help files are installed.])
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
			[moduledir='${pkglibdir}/modules'],
			[moduledir='${prefix}/modules'])
		 AC_MSG_RESULT(no)
		])
AC_DEFINE_DIR(MODULE_DIR, moduledir, [Prefix where modules are installed.])
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
		[rundir='${prefix}/run'],
		[rundir='${sysconfdir}'])])
AC_SUBST([rundir])
AC_DEFINE_DIR([PKGRUNDIR], [pkgrundir], [Directory to store pidfile in.])

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
AC_DEFINE([FD_HEAP_SIZE], 1024, [Size of fd heap.])
AC_DEFINE([AWAY_HEAP_SIZE], 512, [Size of away heap.])

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

# rpath, for finding libratbox.so at run time
hold_ldflags=$LDFLAGS
AC_MSG_CHECKING(for the ld -rpath flag)
LDFLAGS="${LDFLAGS} -Wl,-rpath=${libdir}"
AC_LINK_IFELSE([AC_LANG_PROGRAM([],[int i;])], found=yes, found=no)
LDFLAGS=$hold_ldflags
AC_MSG_RESULT($found)
if test "$found" = yes; then
	LDFLAGS="${LDFLAGS} -Wl,-rpath=\${libdir}"
fi

# This must be down here, or it will mess up checks like the ones
# for -Wl,-export-dynamic
# -- jilles
CWARNS=""
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
CHARYBDIS_C_GCC_TRY_FLAGS([-Wwrite-strings], charybdis_cv_c_gcc_w_write_strings)
CHARYBDIS_C_GCC_TRY_FLAGS([-Werror-implicit-function-declaration], charybdis_cv_c_gcc_w_error_implicit_function_declaration)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations], charybdis_cv_c_gcc_prototypes)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wparenthesis], charybdis_cv_c_gcc_parenthesis)
CHARYBDIS_C_GCC_TRY_FLAGS([-W -Wno-unused], charybdis_cv_c_gcc_w)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wextra], charybdis_cv_c_gcc_w_extra)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wshadow], charybdis_cv_c_gcc_w_shadow)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wmissing-noreturn], charybdis_cv_c_gcc_w_missing_noreturn)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wundef], charybdis_cv_c_gcc_w_undef)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wpacked], charybdis_cv_c_gcc_w_packed)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wnested-externs], charybdis_cv_c_gcc_w_nested_externs)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wunused-function -Wunused-label -Wunused-value -Wunused-variable], charybdis_cv_c_gcc_w_unused)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wredundant-decls], charybdis_cv_c_gcc_w_redundant_decls)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wfloat-equal], charybdis_cv_c_gcc_w_float_equal)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wformat -Wformat-y2k -Wno-format-security], charybdis_cv_c_gcc_w_format)

IRC_CFLAGS="$CFLAGS"
],[])

IRC_CFLAGS="$IRC_CFLAGS $CWARNS"

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
	authd/Makefile			\
	bandb/Makefile			\
	ssld/Makefile			\
	extensions/Makefile		\
	ircd/Makefile			\
	modules/Makefile		\
	tools/Makefile			\
	tools/genssl			\
	doc/Makefile			\
	help/Makefile			\
)
AC_CONFIG_COMMANDS([tools/genssl_chmod], [chmod 755 tools/genssl])

AC_OUTPUT

if test "$cf_openssl_version_ok" = yes; then
	openssl="yes"
else
	openssl="no"
fi

echo "
Configuration of ${BRANDING_NAME}-${BRANDING_VERSION}:
	Install directory  : $prefix

	Ziplinks           : $zlib
	OpenSSL            : $openssl

	Nickname length    : $NICKLEN
	Topic length       : $TOPICLEN

Use (g)make to compile $BRANDING_NAME, then (g)make install to install it.
"
