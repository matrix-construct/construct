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
     [rb__cv_timerfd_create_works=no])
  ])
case $rb__cv_timerfd_create_works in
    yes) AC_DEFINE([USE_TIMERFD_CREATE], 1, 
                   [Define to 1 if we can use timerfd_create(CLOCK_REALTIME,...)]);;
esac
])

