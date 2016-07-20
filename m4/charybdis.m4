AC_DEFUN([AC_DEFINE_DIR], [
  test "x$prefix" = xNONE && prefix="$ac_default_prefix"
  test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'
  last_ac_define_dir=`eval echo [$]$2`
  ac_define_dir=`eval echo [$]last_ac_define_dir`
  ac_define_dir_counter=0
  while test "x[$]last_ac_define_dir" != "x[$]ac_define_dir"; do
    last_ac_define_dir="[$]ac_define_dir"
    ac_define_dir=`eval echo [$]last_ac_define_dir`
    AS_VAR_ARITH([ac_define_dir_counter], [$ac_define_dir_counter + 1])
    AS_VAR_IF([ac_define_dir_counter], [128],
	[AC_MSG_ERROR([detected recusive directory expansion when expanding $1=[$]$2: [$]ac_define_dir])
	break])
  done
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

dnl CHARYBDIS_C_GCC_TRY_FLAGS(<warnings>,<cachevar>)
AC_DEFUN([CHARYBDIS_C_GCC_TRY_FLAGS],[
 AC_MSG_CHECKING([GCC flag(s) $1])
 if test "${GCC-no}" = yes
 then
  AC_CACHE_VAL($2,[
   oldcflags="${CXXFLAGS-}"
   CXXFLAGS="${CXXFLAGS-} ${CWARNS} $1 -Werror"
   AC_TRY_COMPILE([
#include <string.h>
#include <stdio.h>
int main(void);
],[
    (void)strcmp("a","b"); fprintf(stdout,"test ok\n");
], [$2=yes], [$2=no])
   CXXFLAGS="${oldcflags}"])
  if test "x$$2" = xyes; then
   CWARNS="${CWARNS}$1 "
   AC_MSG_RESULT(ok)  
  else
   $2=''
   AC_MSG_RESULT(no)
  fi
 else
  AC_MSG_RESULT(no, not using GCC)
 fi
])


AC_DEFUN([RB_CHECK_TIMER_CREATE],[AC_CACHE_CHECK([for a working timer_create(CLOCK_REALTIME)],
[rb__cv_timer_create_works], [AC_TRY_RUN([

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



AC_DEFUN([RB_CHECK_TIMERFD_CREATE], [AC_CACHE_CHECK([for a working timerfd_create(CLOCK_REALTIME)],
[rb__cv_timerfd_create_works], [AC_TRY_RUN([

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
