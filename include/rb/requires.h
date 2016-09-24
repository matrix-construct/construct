#ifndef _RB_REQUIRES_H
#define _RB_REQUIRES_H 1

#ifdef __cplusplus
extern "C" {
#endif

// This is exactly how they recommend doing it in the docs by Alan Smithee,
// but the gcc docs on the other hand, with tongue in cheek, tell the tale
// by metaphor: https://gcc.gnu.org/onlinedocs/cpp/Computed-Includes.html

#include <RB_INC_ASSERT_H
#include <RB_INC_STDDEF_H
#include <RB_INC_STDARG_H
#include <RB_INC_STDINT_H
#include <RB_INC_INTTYPES_H
#include <RB_INC_CTYPE_H
#include <RB_INC_LIMITS_H
#include <RB_INC_STDLIB_H
#include <RB_INC_UNISTD_H
#include <RB_INC_TIME_H
#include <RB_INC_FCNTL_H
#include <RB_INC_SIGNAL_H
#include <RB_INC_DIRENT_H
#include <RB_INC_SYS_TYPES_H
#include <RB_INC_SYS_TIME_H
#include <RB_INC_SYS_STAT_H
#include <RB_INC_SYS_FILE_H
#include <RB_INC_SYS_PARAM_H
#include <RB_INC_SYS_RESOURCE_H
#include <RB_INC_SYS_SOCKET_H
#include <RB_INC_ARPA_INET_H
#include <RB_INC_NETINET_IN_H
#include <RB_INC_NETINET_TCP_H
#include <RB_INC_STRING_H
#include <RB_INC_STRINGS_H
#include <RB_INC_STDIO_H
#include <RB_INC_CRYPT_H

// keep this before <cerrno>
// #include <RB_INC_ERRNO_H
#include <errno.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <RB_INC_WINDOWS_H
#include <RB_INC_WINSOCK2_H
#include <RB_INC_WS2TCPIP_H
#include <RB_INC_IPHLPAPI_H
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
#include <RB_INC_CSTDDEF
#include <RB_INC_CSTDINT
#include <RB_INC_LIMITS
#include <RB_INC_TYPE_TRAITS
#include <RB_INC_TYPEINDEX
#include <RB_INC_CERRNO
#include <RB_INC_UTILITY
#include <RB_INC_FUNCTIONAL
#include <RB_INC_ALGORITHM
#include <RB_INC_NUMERIC
#include <RB_INC_MEMORY
#include <RB_INC_EXCEPTION
#include <RB_INC_SYSTEM_ERROR
#include <RB_INC_ARRAY
#include <RB_INC_VECTOR
#include <RB_INC_STRING
#include <RB_INC_MAP
#include <RB_INC_SET
#include <RB_INC_LIST
#include <RB_INC_FORWARD_LIST
#include <RB_INC_UNORDERED_MAP
#include <RB_INC_DEQUE
#include <RB_INC_SSTREAM
#include <RB_INC_FSTREAM
#include <RB_INC_IOSTREAM
#include <RB_INC_IOMANIP
#include <RB_INC_CSTDIO
#include <RB_INC_CHRONO
#include <RB_INC_CTIME
#include <RB_INC_ATOMIC
#include <RB_INC_THREAD
#include <RB_INC_MUTEX
#include <RB_INC_CONDITION_VARIABLE

//#include <RB_INC_BOOST_LEXICAL_CAST_HPP
#endif

#endif /* _RB_REQUIRES_H */
