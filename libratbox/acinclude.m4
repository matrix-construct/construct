# $Id: acinclude.m4 23020 2006-09-01 18:20:19Z androsyn $ - aclocal.m4 - Autoconf fun...
AC_DEFUN([AC_DEFINE_DIR], [
  test "x$prefix" = xNONE && prefix="$ac_default_prefix"
  test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'
  ac_define_dir=`eval echo [$]$2`
  ac_define_dir=`eval echo [$]ac_define_dir`
  $1="$ac_define_dir"
  AC_SUBST($1)
  ifelse($3, ,
    AC_DEFINE_UNQUOTED($1, "$ac_define_dir"),
    AC_DEFINE_UNQUOTED($1, "$ac_define_dir", $3))
])

AC_DEFUN([AC_SUBST_DIR], [
        ifelse($2,,,$1="[$]$2")
        $1=`(
            test "x$prefix" = xNONE && prefix="$ac_default_prefix"
            test "x$exec_prefix" = xNONE && exec_prefix="${prefix}"
            eval echo \""[$]$1"\"
        )`
        AC_SUBST($1)
])


# RB_TYPE_INTMAX_T
# -----------------
AC_DEFUN([RB_TYPE_INTMAX_T],
[
  AC_REQUIRE([AC_TYPE_LONG_LONG_INT])
  AC_CHECK_TYPE([intmax_t],
    [AC_DEFINE([HAVE_INTMAX_T], 1,
       [Define to 1 if the system has the type `intmax_t'.]) ac_cv_c_intmax_t=yes],
    [test $ac_cv_type_long_long_int = yes \
       && ac_type='long long int' \
       || ac_type='long int'
     AC_DEFINE_UNQUOTED([intmax_t], [$ac_type],
       [Define to the widest signed integer type
	if <stdint.h> and <inttypes.h> do not define.]) ac_cv_c_intmax_t="$ac_type"])
])


# RB_TYPE_UINTMAX_T
# -----------------
AC_DEFUN([RB_TYPE_UINTMAX_T],
[
  AC_REQUIRE([AC_TYPE_UNSIGNED_LONG_LONG_INT])
  AC_CHECK_TYPE([uintmax_t],
    [AC_DEFINE([HAVE_UINTMAX_T], 1,
       [Define to 1 if the system has the type `uintmax_t'.]) ac_cv_c_uintmax_t=yes],
    [test $ac_cv_type_unsigned_long_long_int = yes \
       && ac_type='unsigned long long int' \
       || ac_type='unsigned long int'
     AC_DEFINE_UNQUOTED([uintmax_t], [$ac_type],
       [Define to the widest unsigned integer type
	if <stdint.h> and <inttypes.h> do not define.]) ac_cv_c_uintmax_t="$ac_type"])
])


# RB_TYPE_INTPTR_T
# -----------------
AC_DEFUN([RB_TYPE_INTPTR_T],
[
  AC_CHECK_TYPE([intptr_t],
    [AC_DEFINE([HAVE_INTPTR_T], 1,
       [Define to 1 if the system has the type `intptr_t'.]) ac_cv_c_intptr_t=yes],
    [for ac_type in 'int' 'long int' 'long long int'; do
       AC_COMPILE_IFELSE(
	 [AC_LANG_BOOL_COMPILE_TRY(
	    [AC_INCLUDES_DEFAULT],
	    [[sizeof (void *) <= sizeof ($ac_type)]])],
	 [AC_DEFINE_UNQUOTED([intptr_t], [$ac_type],
	    [Define to the type of a signed integer type wide enough to
	     hold a pointer, if such a type exists, and if the system
	     does not define it.]) ac_cv_c_intptr_t="$ac_type"
	  ac_type=])
       test -z "$ac_type" && break
     done])
])


# RB_TYPE_UINTPTR_T
# -----------------
AC_DEFUN([RB_TYPE_UINTPTR_T],
[
  AC_CHECK_TYPE([uintptr_t],
    [AC_DEFINE([HAVE_UINTPTR_T], 1,
       [Define to 1 if the system has the type `uintptr_t'.]) ac_cv_c_uintptr_t=yes],
    [for ac_type in 'unsigned int' 'unsigned long int' \
	'unsigned long long int'; do
       AC_COMPILE_IFELSE(
	 [AC_LANG_BOOL_COMPILE_TRY(
	    [AC_INCLUDES_DEFAULT],
	    [[sizeof (void *) <= sizeof ($ac_type)]])],
	 [AC_DEFINE_UNQUOTED([uintptr_t], [$ac_type],
	    [Define to the type of an unsigned integer type wide enough to
	     hold a pointer, if such a type exists, and if the system
	     does not define it.]) ac_cv_c_uintptr_t="$ac_type"
	  ac_type=])
       test -z "$ac_type" && break
     done])
])

dnl IPv6 support macros..pretty much swiped from wget

dnl RB_PROTO_INET6
 
AC_DEFUN([RB_PROTO_INET6],[
  AC_CACHE_CHECK([for INET6 protocol support], [rb_cv_proto_inet6],[
    AC_TRY_CPP([
#include <sys/types.h>
#include <sys/socket.h>

#ifndef PF_INET6
#error Missing PF_INET6
#endif
#ifndef AF_INET6
#error Mlssing AF_INET6
#endif
    ],[
      rb_cv_proto_inet6=yes
    ],[
      rb_cv_proto_inet6=no
    ])
  ])  

  if test "X$rb_cv_proto_inet6" = "Xyes"; then :
    $1
  else :
    $2  
  fi    
])      


AC_DEFUN([RB_TYPE_STRUCT_SOCKADDR_IN6],[
  rb_have_sockaddr_in6=
  AC_CHECK_TYPES([struct sockaddr_in6],[
    rb_have_sockaddr_in6=yes
  ],[
    rb_have_sockaddr_in6=no
  ],[
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
  ])

  if test "X$rb_have_sockaddr_in6" = "Xyes"; then :
    $1
  else :
    $2
  fi
])


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
     [rb__cv_timer_create_works=no])
  ])
case $rb__cv_timer_create_works in
    yes) AC_DEFINE([USE_TIMER_CREATE], 1, 
                   [Define to 1 if we can use timer_create(CLOCK_REALTIME,...)]);;
esac
])

