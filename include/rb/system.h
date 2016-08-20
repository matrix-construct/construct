#ifndef _RB_SYSTEM_H
#define _RB_SYSTEM_H 1
#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_IPV6
	#define RB_IPV6 1
#endif

#ifndef __cplusplus
	#ifdef HAVE_STDBOOL_H
		#include <stdbool.h>
	#else
		#ifndef HAVE__BOOL
			#define _Bool signed char
		#endif
		#define bool _Bool
		#define false 0
		#define true 1
		#define __bool_true_false_are_defined 1
	#endif
#endif

#ifndef ulong
	typedef unsigned long ulong;
#endif

#ifndef uint
	typedef unsigned int uint;
#endif

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

/*
 * This ensures that __attribute__((deprecated)) is not used in for example
 * sun CC, since it's a GNU-specific extension. -nenolod
 */
#ifdef __GNUC__
#define IRC_DEPRECATED __attribute__((deprecated))
#else
#define IRC_DEPRECATED
#endif


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

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

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

#if defined(__INTEL_COMPILER) || defined(__GNUC__)

	#ifdef __printf
		#undef __printf
	#endif

	#ifdef __noreturn
		#undef __noreturn
	#endif

	#define __printf(x) __attribute__((__format__ (__printf__, x, x + 1)))
	#define __noreturn __attribute__((__noreturn__))

#else

	#ifndef __printf
		#define __printf
	#endif

	#ifndef __noreturn
		#define __noreturn
	#endif

#endif


#ifdef _WIN32
#define rb_get_errno() do { errno = WSAGetLastError(); WSASetLastError(errno); } while(0)
typedef SOCKET rb_platform_fd_t;
#define RB_PATH_SEPARATOR '\\'
#else
#define rb_get_errno()
typedef int rb_platform_fd_t;
#define RB_PATH_SEPARATOR '/'
#endif

#ifdef _WIN32
#include <process.h>

#ifndef PATH_MAX
#define PATH_MAX 128
#endif

#ifdef strerror
#undef strerror
#endif

#define strerror(x) rb_strerror(x)
char *rb_strerror(int error);

#undef ENOBUFS
#define ENOBUFS	    WSAENOBUFS

#undef EINPROGRESS
#define EINPROGRESS WSAEINPROGRESS

#undef EWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK

#undef EMSGSIZE
#define EMSGSIZE    WSAEMSGSIZE

#undef EALREADY
#define EALREADY    WSAEALREADY

#undef EISCONN
#define EISCONN     WSAEISCONN

#undef EADDRINUSE
#define EADDRINUSE  WSAEADDRINUSE

#undef EAFNOSUPPORT
#define EAFNOSUPPORT WSAEAFNOSUPPORT

#define pipe(x)  _pipe(x, 1024, O_BINARY)
#define ioctl(x,y,z)  ioctlsocket(x,y, (unsigned long *)z)

#define WNOHANG 1

#ifndef SIGKILL
#define SIGKILL SIGTERM
#endif

#endif /* _WIN32 */



#ifndef HOSTIPLEN
#define HOSTIPLEN	53
#endif

#ifdef __GNUC__
#define slrb_assert(expr) do                                                 \
{                                                                            \
    if(rb_unlikely(!(expr)))                                                 \
    {                                                                        \
        rb_lib_log("file: %s line: %d (%s): Assertion failed: (%s)",         \
        __FILE__, __LINE__, __PRETTY_FUNCTION__, #expr);                     \
        rb_backtrace_log_symbols();                                          \
    }                                                                        \
}                                                                            \
while(0)
#else
#define slrb_assert(expr)	do								\
			if(rb_unlikely(!(expr))) {							\
				rb_lib_log( 						\
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
					struct sockaddr *_storage;		\
					_storage = ((struct sockaddr *)(x));\
					_storage->sa_len = (y);				\
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

#ifdef RB_IPV6
#define GET_SS_PORT(x) (((struct sockaddr *)(x))->sa_family == AF_INET ? ((struct sockaddr_in *)(x))->sin_port : ((struct sockaddr_in6 *)(x))->sin6_port)
#define SET_SS_PORT(x, y)	do { \
					if(((struct sockaddr *)(x))->sa_family == AF_INET) { \
						((struct sockaddr_in *)(x))->sin_port = (y); \
					} else { \
						((struct sockaddr_in6 *)(x))->sin6_port = (y); \
					} \
				} while (0)
#else
#define GET_SS_PORT(x) (((struct sockaddr_in *)(x))->sin_port)
#define SET_SS_PORT(x, y) (((struct sockaddr_in *)(x))->sin_port = y)
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

#ifndef UINT16_MAX
#define UINT16_MAX (65535U)
#endif

#ifndef UINT32_MAX
#define UINT32_MAX (4294967295U)
#endif

/* For those unfamiliar with GNU format attributes, a is the 1 based
 * argument number of the format string, and b is the 1 based argument
 * number of the variadic ... */
#ifdef __GNUC__
#define AFP(a,b) __attribute__((format (printf, a, b)))
#else
#define AFP(a,b)
#endif


#ifdef HAVE_STRUCT_SOCKADDR_STORAGE
	#define rb_sockaddr_storage sockaddr_storage
#else
	struct rb_sockaddr_storage { uint8_t _padding[128]; };
#endif

#define rb_hash_fd(x) ((x ^ (x >> RB_FD_HASH_BITS) ^ (x >> (RB_FD_HASH_BITS * 2))) & RB_FD_HASH_MASK)

#ifdef HAVE_WRITEV
#ifndef UIO_MAXIOV
# if defined(__FreeBSD__) || defined(__APPLE__) || defined(__NetBSD__)
            /* FreeBSD 4.7 defines it in sys/uio.h only if _KERNEL is specified */
#  define RB_UIO_MAXIOV 1024
# elif defined(__sgi)
            /* IRIX 6.5 has sysconf(_SC_IOV_MAX) which might return 512 or bigger */
#  define RB_UIO_MAXIOV 512
# elif defined(__sun)
            /* Solaris (and SunOS?) defines IOV_MAX instead */
#  ifndef IOV_MAX
#   define RB_UIO_MAXIOV 16
#  else
#   define RB_UIO_MAXIOV IOV_MAX
#  endif

# elif defined(IOV_MAX)
#  define RB_UIO_MAXIOV IOV_MAX
# else
#  define RB_UIO_MAXIOV 16
# endif
#else
#define RB_UIO_MAXIOV UIO_MAXIOV
#endif
#else
#define RB_UIO_MAXIOV 16
#endif

#ifndef MAX
#define MAX(a, b)   ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a, b)   ((a) < (b) ? (a) : (b))
#endif

#ifdef RB_IPV6
#ifndef AF_INET6
#error "AF_INET6 not defined"
#endif


#else /* #ifdef RB_IPV6 */

#ifndef AF_INET6
#define AF_INET6 AF_MAX     /* Dummy AF_INET6 declaration */
#endif
#endif /* #ifdef RB_IPV6 */

#ifdef RB_IPV6
#define PATRICIA_BITS   128
#else
#define PATRICIA_BITS   32
#endif


#ifdef __cplusplus
	#define RB_EXTERN_C extern "C"
#else
	#define RB_EXTERN_C
#endif


#ifdef strdupa
	#define LOCAL_COPY(s) strdupa(s)
#else
	#if defined(__INTEL_COMPILER) || defined(__GNUC__)
		#define LOCAL_COPY(s) __extension__({ char *_s = (char *)alloca(strlen(s) + 1); strcpy(_s, s); _s; })
	#else
		#define LOCAL_COPY(s) strcpy((char *)alloca(strlen(s) + 1), s)
	#endif
#endif


#ifdef __cplusplus
} // extern "C"
#endif
#endif  // _RB_SYSTEM_H
