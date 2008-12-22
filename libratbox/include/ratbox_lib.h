/*
 * $Id: ratbox_lib.h 26256 2008-12-10 04:05:26Z androsyn $
 */

#ifndef RB_LIB_H
#define RB_LIB_H 1

#include <librb-config.h>
#include <stdio.h>
#include <limits.h>
#include <stdarg.h>
#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>

#ifdef __GNUC__
#undef alloca
#define alloca __builtin_alloca
#else
# ifdef _MSC_VER
#  include <malloc.h>
#  define alloca _alloca
# else
#  if RB_HAVE_ALLOCA_H
#   include <alloca.h>
#  else
#   ifdef _AIX
#pragma alloca
#   else
#    ifndef alloca		/* predefined by HP cc +Olibcalls */
char *alloca();
#    endif
#   endif
#  endif
# endif
#endif /* __GNUC__ */

#ifdef __GNUC__

#ifdef rb_likely
#undef rb_likely
#endif
#ifdef rb_unlikely
#undef rb_unlikely
#endif

#if __GNUC__ == 2 && __GNUC_MINOR__ < 96
# define __builtin_expect(x, expected_value) (x)
#endif

#define rb_likely(x)       __builtin_expect(!!(x), 1)
#define rb_unlikely(x)     __builtin_expect(!!(x), 0)

#else /* !__GNUC__ */

#define UNUSED(x) x

#ifdef rb_likely
#undef rb_likely
#endif
#ifdef rb_unlikely
#undef rb_unlikely
#endif
#define rb_likely(x)	(x)
#define rb_unlikely(x)	(x)
#endif



#ifdef _WIN32
#include <process.h>

#ifndef MAXPATHLEN
#define MAXPATHLEN 128
#endif

#ifdef strerror
#undef strerror
#endif

#define strerror(x) rb_strerror(x)
char *rb_strerror(int error);


#define ENOBUFS	    WSAENOBUFS
#define EINPROGRESS WSAEINPROGRESS
#define EWOULDBLOCK WSAEWOULDBLOCK
#define EMSGSIZE    WSAEMSGSIZE
#define EALREADY    WSAEALREADY
#define EISCONN     WSAEISCONN
#define EADDRINUSE  WSAEADDRINUSE
#define EAFNOSUPPORT WSAEAFNOSUPPORT

#define pipe(x)  _pipe(x, 1024, O_BINARY)
#define ioctl(x,y,z)  ioctlsocket(x,y, (u_long *)z)

#define WNOHANG 1

#ifndef SIGKILL
#define SIGKILL SIGTERM
#endif

#endif /* _WIN32 */



#ifndef HOSTIPLEN
#define HOSTIPLEN	53
#endif

#ifdef __GNUC__
#define slrb_assert(expr)	do								\
			if(rb_unlikely(!(expr))) {							\
				rb_lib_log( 						\
				"file: %s line: %d (%s): Assertion failed: (%s)",	\
				__FILE__, __LINE__, __PRETTY_FUNCTION__, #expr); 	\
			}								\
			while(0)
#else
#define slrb_assert(expr)	do								\
			if(rb_unlikely(!(expr))) {							\
				rb_lib_log(L_MAIN, 						\
				"file: %s line: %d: Assertion failed: (%s)",		\
				__FILE__, __LINE__, #expr); 				\
			}								\
			while(0)
#endif

#ifdef SOFT_ASSERT
#define lrb_assert(expr) 	slrb_assert(expr)
#else
#define lrb_assert(expr)	do { slrb_assert(expr); assert(expr); } while(0)
#endif

#ifdef RB_SOCKADDR_HAS_SA_LEN
#define ss_len sa_len
#endif

#define GET_SS_FAMILY(x) (((const struct sockaddr *)(x))->sa_family)
#define SET_SS_FAMILY(x, y) ((((struct sockaddr *)(x))->sa_family) = y)
#ifdef RB_SOCKADDR_HAS_SA_LEN
#define SET_SS_LEN(x, y)	do {							\
					struct sockaddr *storage;		\
					storage = ((struct sockaddr *)(x));\
					storage->sa_len = (y);				\
				} while (0)
#define GET_SS_LEN(x) (((struct sockaddr *)(x))->sa_len)
#else /* !RB_SOCKADDR_HAS_SA_LEN */
#define SET_SS_LEN(x, y) (((struct sockaddr *)(x))->sa_family = ((struct sockaddr *)(x))->sa_family)
#ifdef RB_IPV6
#define GET_SS_LEN(x) (((struct sockaddr *)(x))->sa_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6))
#else
#define GET_SS_LEN(x) (((struct sockaddr *)(x))->sa_family == AF_INET ? sizeof(struct sockaddr_in) : 0)
#endif
#endif

#ifndef INADDRSZ
#define INADDRSZ 4
#endif

#ifndef IN6ADDRSZ
#define IN6ADDRSZ 16
#endif

#ifndef INT16SZ
#define INT16SZ 2
#endif


typedef void log_cb(const char *buffer);
typedef void restart_cb(const char *buffer);
typedef void die_cb(const char *buffer);

char *rb_ctime(const time_t, char *, size_t);
char *rb_date(const time_t, char *, size_t);
void rb_lib_log(const char *, ...);
void rb_lib_restart(const char *, ...);
void rb_lib_die(const char *, ...);
void rb_set_time(void);
const char *rb_lib_version(void);

void rb_lib_init(log_cb * xilog, restart_cb * irestart, die_cb * idie, int closeall, int maxfds,
		 size_t dh_size, size_t fd_heap_size);
void rb_lib_loop(long delay);

time_t rb_current_time(void);
const struct timeval *rb_current_time_tv(void);
pid_t rb_spawn_process(const char *, const char **);

char *rb_strtok_r(char *, const char *, char **);

int rb_gettimeofday(struct timeval *, void *);

void rb_sleep(unsigned int seconds, unsigned int useconds);
char *rb_crypt(const char *, const char *);

unsigned char *rb_base64_encode(const unsigned char *str, int length);
unsigned char *rb_base64_decode(const unsigned char *str, int length, int *ret);
int rb_kill(pid_t, int);
char *rb_strerror(int);

int rb_setenv(const char *, const char *, int);

pid_t rb_waitpid(pid_t pid, int *status, int options);
pid_t rb_getpid(void);
//unsigned int rb_geteuid(void);


#include <rb_tools.h>
#include <rb_memory.h>
#include <rb_commio.h>
#include <rb_balloc.h>
#include <rb_linebuf.h>
#include <rb_snprintf.h>
#include <rb_event.h>
#include <rb_helper.h>
#include <rb_rawbuf.h>
#include <rb_patricia.h>

#endif
