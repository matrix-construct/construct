{ writeText, stdenv, file
, IRCD_ALLOCATOR_USE_DEFAULT
, IRCD_ALLOCATOR_USE_JEMALLOC
}:

writeText "config.h" ''
  #define BRANDING_NAME "construct"
  /* #undef CUSTOM_BRANDING */

  #define HAVE_ALGORITHM 1
  #define HAVE_ARRAY 1
  #define HAVE_ASSERT_H 1
  #define HAVE_ATOMIC 1
  #define HAVE_BACKTRACE 1
  #define HAVE_BITSET 1
  #define HAVE_BOOST /**/
  #define HAVE_BOOST_ASIO /**/
  #define HAVE_BOOST_CHRONO /**/
  /* #undef HAVE_BOOST_CONTEXT */
  #define HAVE_BOOST_COROUTINE /**/
  #define HAVE_BOOST_FILESYSTEM /**/
  #define HAVE_BOOST_SYSTEM /**/
  #define HAVE_CERRNO 1
  #define HAVE_CFENV 1
  #define HAVE_CHRONO 1
  #define HAVE_CMATH 1
  #define HAVE_CODECVT 1
  #define HAVE_CONDITION_VARIABLE 1
  #define HAVE_CPUID_H 1
  #define HAVE_CRYPTO 1
  #define HAVE_CSTDDEF 1
  #define HAVE_CSTDINT 1
  #define HAVE_CSTDIO 1
  #define HAVE_CSTDLIB 1
  #define HAVE_CSTRING 1
  #define HAVE_CTIME 1
  #define HAVE_CXX17 1
  #define HAVE_CXXABI_H 1
  #define HAVE_DEQUE 1
  #define HAVE_DLFCN_H 1
  #define HAVE_DLINFO 1
  #define HAVE_ELF_H 1
  #define HAVE_ENDPROTOENT 1
  #define HAVE_EXCEPTION 1
  #define HAVE_EXECINFO_H 1
  #define HAVE_EXPERIMENTAL_MEMORY_RESOURCE 1
  #define HAVE_EXPERIMENTAL_OPTIONAL 1
  #define HAVE_EXPERIMENTAL_STRING_VIEW 1
  #define HAVE_FCNTL_H 1
  #define HAVE_FILESYSTEM 1
  #define HAVE_FORWARD_LIST 1
  #define HAVE_FSTREAM 1
  #define HAVE_FUNCTIONAL 1
  #define HAVE_GETPROTOBYNAME 1
  #define HAVE_GETPROTOBYNAME_R 1
  #define HAVE_GETTIMEOFDAY 1
  #define HAVE_GNU_LIBC_VERSION_H 1
  #define HAVE_GNU_LIB_NAMES_H 1
  #define HAVE_IFADDRS_H 1
  /* #undef HAVE_INT128_T */
  #define HAVE_INTPTR_T 1
  #define HAVE_INTTYPES_H 1
  #define HAVE_IOMANIP 1
  #define HAVE_IOSFWD 1
  #define HAVE_IOSTREAM 1
  /* #undef HAVE_IPHLPAPI_H */
  #define HAVE_IPV6 1
  #define HAVE_JEMALLOC_H 1
  #define HAVE_LIBNSS_DB 1
  #define HAVE_LIBROCKSDB 1
  #define HAVE_LIMITS 1
  #define HAVE_LINK_H 1
  #define HAVE_LINUX_AIO_ABI_H 1
  #define HAVE_LINUX_HW_BREAKPOINT_H 1
  #define HAVE_LINUX_ICMP_H 1
  #define HAVE_LINUX_IO_URING_H 1
  #define HAVE_LINUX_MAGIC_H 1
  #define HAVE_LINUX_PERF_EVENT_H 1
  #define HAVE_LIST 1
  #define HAVE_LOCALE 1
  #define HAVE_LONG_DOUBLE 1
  /* #undef HAVE_LZ4_H */
  #define HAVE_MAGICKPP_H 1
  #define HAVE_MAGICK_API_H 1
  #define HAVE_MAGICK_WAND_API_H 1
  #define HAVE_MAGIC_H 1
  #define HAVE_MALLOC_H 1
  #define HAVE_MAP 1
  #define HAVE_MEMORY 1
  #define HAVE_MEMORY_H 1
  #define HAVE_MEMORY_RESOURCE 1
  #define HAVE_MUTEX 1
  #define HAVE_NANOSLEEP 1
  #define HAVE_NETDB_H 1
  #define HAVE_NEW 1
  #define HAVE_NUMERIC 1
  #define HAVE_OPENSSL_ASN1_H 1
  #define HAVE_OPENSSL_DH_H 1
  #define HAVE_OPENSSL_EC_H 1
  #define HAVE_OPENSSL_ERR_H 1
  #define HAVE_OPENSSL_EVP_H 1
  #define HAVE_OPENSSL_HMAC_H 1
  #define HAVE_OPENSSL_RIPEMD_H 1
  #define HAVE_OPENSSL_RSA_H 1
  #define HAVE_OPENSSL_SHA_H 1
  #define HAVE_OPENSSL_SSL_H 1
  #define HAVE_OPENSSL_TLS1_H 1
  #define HAVE_OPENSSL_X509_H 1
  #define HAVE_OPTIONAL 1
  /* #undef HAVE_PBC */
  #define HAVE_POSIX_FADVISE 1
  #define HAVE_PREADV2 1
  #define HAVE_PWRITEV2 1
  #define HAVE_QUEUE 1
  #define HAVE_RANDOM 1
  #define HAVE_REGEX 1
  #define HAVE_SET 1
  #define HAVE_SETPROTOENT 1
  #define HAVE_SHARED_MUTEX 1
  #define HAVE_SIGNAL_H 1
  /* #undef HAVE_SNAPPY_H */
  #define HAVE_SNPRINTF 1
  #define HAVE_SODIUM 1
  /* #undef HAVE_SODIUM_H */
  #define HAVE_SSTREAM 1
  #define HAVE_STACK 1
  #define HAVE_STDARG_H 1
  /* #undef HAVE_STDBOOL_H */
  #define HAVE_STDINT_H 1
  #define HAVE_STDLIB_H 1
  #define HAVE_STRCPY 1
  #define HAVE_STRING 1
  #define HAVE_STRINGS_H 1
  #define HAVE_STRING_H 1
  #define HAVE_STRING_VIEW 1
  /* #undef HAVE_STRLCAT */
  /* #undef HAVE_STRLCPY */
  #define HAVE_STRNCPY 1
  #define HAVE_STRNLEN 1
  #define HAVE_SYSTEM_ERROR 1
  #define HAVE_SYS_AUXV_H 1
  #define HAVE_SYS_EVENTFD_H 1
  #define HAVE_SYS_INOTIFY_H 1
  #define HAVE_SYS_IOCTL_H 1
  #define HAVE_SYS_MMAN_H 1
  #define HAVE_SYS_RESOURCE_H 1
  #define HAVE_SYS_STATFS_H 1
  #define HAVE_SYS_STATVFS_H 1
  #define HAVE_SYS_STAT_H 1
  #define HAVE_SYS_SYSCALL_H 1
  #define HAVE_SYS_SYSINFO_H 1
  #define HAVE_SYS_SYSMACROS_H 1
  #define HAVE_SYS_TIME_H 1
  #define HAVE_SYS_TYPES_H 1
  #define HAVE_SYS_UTSNAME_H 1
  #define HAVE_THREAD 1
  #define HAVE_TYPEINDEX 1
  #define HAVE_TYPE_TRAITS 1
  /* #undef HAVE_UINT128_T */
  #define HAVE_UINTPTR_T 1
  #define HAVE_UNISTD_H 1
  #define HAVE_UNORDERED_MAP 1
  #define HAVE_UTILITY 1
  /* #undef HAVE_VALGRIND_CALLGRIND_H */
  /* #undef HAVE_VALGRIND_MEMCHECK_H */
  /* #undef HAVE_VALGRIND_VALGRIND_H */
  #define HAVE_VARIANT 1
  #define HAVE_VECTOR 1
  #define HAVE_VSNPRINTF 1
  /* #undef HAVE_WIN32 */
  /* #undef HAVE_WINDOWS_H */
  /* #undef HAVE_WINSOCK2_H */
  /* #undef HAVE_WS2TCPIP_H */
  #define HAVE_X86INTRIN_H 1
  #define HAVE_ZLIB_H 1
  /* #undef HAVE__BOOL */
  #define HAVE___INT128 1
  #define HAVE___INT128_T 1
  #define HAVE___UINT128_T 1

  /* Use the default allocator */
  ${if IRCD_ALLOCATOR_USE_DEFAULT then "#define IRCD_ALLOCATOR_USE_DEFAULT 1" else ""}

  /* Use jemalloc as the allocator */
  ${if IRCD_ALLOCATOR_USE_JEMALLOC then "#define IRCD_ALLOCATOR_USE_JEMALLOC 1" else ""}

  /* Linux AIO is supported and will be used */
  #define IRCD_USE_AIO 1

  /* Linux io_uring is supported and may be used */
  #define IRCD_USE_IOU 1

  /* Define to the sub-directory where libtool stores uninstalled libraries. */
  #define LT_OBJDIR ".libs/"

  /* Name of package */
  #define PACKAGE "construct"

  /* Define to the address where bug reports for this package should be sent. */
  #define PACKAGE_BUGREPORT ""

  /* Define to the full name of this package. */
  #define PACKAGE_NAME "construct"

  /* Define to the full name and version of this package. */
  #define PACKAGE_STRING "construct 1.0-dev"

  /* Define to the one symbol short name of this package. */
  #define PACKAGE_TARNAME "construct"

  /* Define to the home page for this package. */
  #define PACKAGE_URL ""

  /* Define to the version of this package. */
  #define PACKAGE_VERSION "1.0-dev"

  /* #undef RB_ASSERT */
  /* #undef RB_ASSERT_INTRINSIC */
  #define RB_BIN_DIR "@out@/bin"
  /* #undef RB_COMPACT */
  #define RB_CONF_DIR "@out@/etc"
  #define RB_CXX "${stdenv.cc}/bin/g++ -std=gnu++17"
  #define RB_CXX_EPOCH 9.3.0
  #define RB_CXX_VERSION "9.3.0"
  #define RB_DATAROOT_DIR "@out@/share"
  #define RB_DATA_DIR "@out@/share/construct"
  #define RB_DATESTR "Thu  1 Jan 01:00:00 BST 1970"
  #define RB_DATE_CONFIGURED "Thu  1 Jan 01:00:00 BST 1970"
  #define RB_DB_DIR "@out@/var/db/construct"
  /* #undef RB_DEBUG */
  #define RB_DEBUG_LEVEL 0
  /* #undef RB_ENABLE_JS */
  #define RB_GENERIC 1
  /* #undef RB_INCLUDED_BOOST */
  #define RB_INCLUDE_DIR "@out@/include"
  #define RB_INC_ALGORITHM algorithm>
  #define RB_INC_ARRAY array>
  #define RB_INC_ASSERT_H assert.h>
  #define RB_INC_ATOMIC atomic>
  #define RB_INC_BITSET bitset>
  #define RB_INC_CERRNO cerrno>
  #define RB_INC_CFENV cfenv>
  #define RB_INC_CHRONO chrono>
  #define RB_INC_CMATH cmath>
  #define RB_INC_CODECVT codecvt>
  #define RB_INC_CONDITION_VARIABLE condition_variable>
  #define RB_INC_CPUID_H cpuid.h>
  #define RB_INC_CSTDDEF cstddef>
  #define RB_INC_CSTDINT cstdint>
  #define RB_INC_CSTDIO cstdio>
  #define RB_INC_CSTDLIB cstdlib>
  #define RB_INC_CSTRING cstring>
  #define RB_INC_CTIME ctime>
  #define RB_INC_CXXABI_H cxxabi.h>
  #define RB_INC_DEQUE deque>
  #define RB_INC_DLFCN_H dlfcn.h>
  #define RB_INC_ELF_H elf.h>
  #define RB_INC_EXCEPTION exception>
  #define RB_INC_EXECINFO_H execinfo.h>
  #define RB_INC_EXPERIMENTAL_MEMORY_RESOURCE experimental/memory_resource>
  #define RB_INC_EXPERIMENTAL_OPTIONAL experimental/optional>
  #define RB_INC_EXPERIMENTAL_STRING_VIEW experimental/string_view>
  #define RB_INC_FCNTL_H fcntl.h>
  #define RB_INC_FILESYSTEM filesystem>
  #define RB_INC_FORWARD_LIST forward_list>
  #define RB_INC_FSTREAM fstream>
  #define RB_INC_FUNCTIONAL functional>
  #define RB_INC_GNU_LIBC_VERSION_H gnu/libc-version.h>
  #define RB_INC_GNU_LIB_NAMES_H gnu/lib-names.h>
  #define RB_INC_IFADDRS_H ifaddrs.h>
  #define RB_INC_IOMANIP iomanip>
  #define RB_INC_IOSFWD iosfwd>
  #define RB_INC_IOSTREAM iostream>
  #define RB_INC_IPHLPAPI_H stddef.h>
  #define RB_INC_JEMALLOC_H jemalloc/jemalloc.h>
  /* #undef RB_INC_JSAPI_H */
  /* #undef RB_INC_JSFRIENDAPI_H */
  /* #undef RB_INC_JS_CONVERSIONS_H */
  #define RB_INC_LIMITS limits>
  #define RB_INC_LINK_H link.h>
  #define RB_INC_LINUX_AIO_ABI_H linux/aio_abi.h>
  #define RB_INC_LINUX_HW_BREAKPOINT_H linux/hw_breakpoint.h>
  #define RB_INC_LINUX_ICMP_H linux/icmp.h>
  #define RB_INC_LINUX_IO_URING_H linux/io_uring.h>
  #define RB_INC_LINUX_MAGIC_H linux/magic.h>
  #define RB_INC_LINUX_PERF_EVENT_H linux/perf_event.h>
  #define RB_INC_LIST list>
  #define RB_INC_LOCALE locale>
  #define RB_INC_LZ4_H stddef.h>
  #define RB_INC_MAGICKPP_H Magick++.h>
  #define RB_INC_MAGICK_API_H magick/api.h>
  #define RB_INC_MAGICK_WAND_API_H wand/wand_api.h>
  #define RB_INC_MAGIC_H magic.h>
  #define RB_INC_MALLOC_H malloc.h>
  #define RB_INC_MAP map>
  #define RB_INC_MEMORY memory>
  #define RB_INC_MEMORY_RESOURCE memory_resource>
  #define RB_INC_MUTEX mutex>
  #define RB_INC_NETDB_H netdb.h>
  #define RB_INC_NEW new>
  #define RB_INC_NUMERIC numeric>
  #define RB_INC_OPENSSL_ASN1_H openssl/asn1.h>
  #define RB_INC_OPENSSL_DH_H openssl/dh.h>
  #define RB_INC_OPENSSL_EC_H openssl/ec.h>
  #define RB_INC_OPENSSL_ERR_H openssl/err.h>
  #define RB_INC_OPENSSL_EVP_H openssl/evp.h>
  #define RB_INC_OPENSSL_HMAC_H openssl/hmac.h>
  #define RB_INC_OPENSSL_RIPEMD_H openssl/ripemd.h>
  #define RB_INC_OPENSSL_RSA_H openssl/rsa.h>
  #define RB_INC_OPENSSL_SHA_H openssl/sha.h>
  #define RB_INC_OPENSSL_SSL_H openssl/ssl.h>
  #define RB_INC_OPENSSL_TLS1_H openssl/tls1.h>
  #define RB_INC_OPENSSL_X509_H openssl/x509.h>
  #define RB_INC_OPTIONAL optional>
  #define RB_INC_QUEUE queue>
  #define RB_INC_RANDOM random>
  #define RB_INC_REGEX regex>
  #define RB_INC_SET set>
  #define RB_INC_SHARED_MUTEX shared_mutex>
  #define RB_INC_SIGNAL_H signal.h>
  #define RB_INC_SNAPPY_H stddef.h>
  /* #undef RB_INC_SODIUM_H */
  #define RB_INC_SSTREAM sstream>
  #define RB_INC_STACK stack>
  #define RB_INC_STDARG_H stdarg.h>
  #define RB_INC_STRING string>
  #define RB_INC_STRING_VIEW string_view>
  #define RB_INC_SYSTEM_ERROR system_error>
  #define RB_INC_SYS_AUXV_H sys/auxv.h>
  #define RB_INC_SYS_EVENTFD_H sys/eventfd.h>
  #define RB_INC_SYS_INOTIFY_H sys/inotify.h>
  #define RB_INC_SYS_IOCTL_H sys/ioctl.h>
  #define RB_INC_SYS_MMAN_H sys/mman.h>
  #define RB_INC_SYS_RESOURCE_H sys/resource.h>
  #define RB_INC_SYS_STATFS_H sys/statfs.h>
  #define RB_INC_SYS_STATVFS_H sys/statvfs.h>
  #define RB_INC_SYS_STAT_H sys/stat.h>
  #define RB_INC_SYS_SYSCALL_H sys/syscall.h>
  #define RB_INC_SYS_SYSINFO_H sys/sysinfo.h>
  #define RB_INC_SYS_SYSMACROS_H sys/sysmacros.h>
  #define RB_INC_SYS_TIME_H sys/time.h>
  #define RB_INC_SYS_TYPES_H sys/types.h>
  #define RB_INC_SYS_UTSNAME_H sys/utsname.h>
  #define RB_INC_THREAD thread>
  #define RB_INC_TYPEINDEX typeindex>
  #define RB_INC_TYPE_TRAITS type_traits>
  #define RB_INC_UNISTD_H unistd.h>
  #define RB_INC_UNORDERED_MAP unordered_map>
  #define RB_INC_UTILITY utility>
  #define RB_INC_VALGRIND_CALLGRIND_H stddef.h>
  #define RB_INC_VALGRIND_MEMCHECK_H stddef.h>
  #define RB_INC_VALGRIND_VALGRIND_H stddef.h>
  #define RB_INC_VARIANT variant>
  #define RB_INC_VECTOR vector>
  #define RB_INC_WINDOWS_H stddef.h>
  #define RB_INC_WINSOCK2_H stddef.h>
  #define RB_INC_WS2TCPIP_H stddef.h>
  #define RB_INC_X86INTRIN_H x86intrin.h>
  #define RB_INC_ZLIB_H zlib.h>
  #define RB_LIB_DIR "@out@/lib"
  #define RB_LOCALSTATE_DIR "@out@/var"
  #define RB_LOG_DIR "@out@/var/log/construct"
  #define RB_LOG_LEVEL 4
  #define RB_MAGIC_FILE "${file.out}/share/misc/magic.mgc"
  #define RB_MODULE_DIR "@out@/lib/modules"
  #define RB_MXID_MAXLEN 255
  #define RB_OPTIMIZE_LEVEL 3
  #define RB_OS "${stdenv.system}"
  #define RB_PREFIX "@out@"
  #define RB_RUN_DIR "/construct"
  #define RB_TIME_CONFIGURED 0
  /* #undef RB_UNTUNED */
  #define RB_WEBAPP_DIR "@out@/share/webapp"

  #define SIZEOF_CHAR 1
  #define SIZEOF_DOUBLE 8
  #define SIZEOF_FLOAT 4
  #define SIZEOF_INT 4
  #define SIZEOF_INT128_T 0
  #define SIZEOF_LONG 8
  #define SIZEOF_LONG_DOUBLE 16
  #define SIZEOF_LONG_LONG 8
  #define SIZEOF_SHORT 2
  #define SIZEOF_UINT128_T 0
  #define SIZEOF___INT128 16
  #define SIZEOF___INT128_T 16
  #define SIZEOF___UINT128_T 16
  #define STDC_HEADERS 1

  /* Enable extensions on AIX 3, Interix.  */
  #ifndef _ALL_SOURCE
  # define _ALL_SOURCE 1
  #endif
  /* Enable GNU extensions on systems that have them.  */
  #ifndef _GNU_SOURCE
  # define _GNU_SOURCE 1
  #endif
  /* Enable threading extensions on Solaris.  */
  #ifndef _POSIX_PTHREAD_SEMANTICS
  # define _POSIX_PTHREAD_SEMANTICS 1
  #endif
  /* Enable extensions on HP NonStop.  */
  #ifndef _TANDEM_SOURCE
  # define _TANDEM_SOURCE 1
  #endif
  /* Enable general extensions on Solaris.  */
  #ifndef __EXTENSIONS__
  # define __EXTENSIONS__ 1
  #endif

  /* Version number of package */
  #define VERSION "1.0-dev"

  /* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
     significant byte first (like Motorola and SPARC, unlike Intel). */
  #if defined AC_APPLE_UNIVERSAL_BUILD
  # if defined __BIG_ENDIAN__
  #  define WORDS_BIGENDIAN 1
  # endif
  #else
  # ifndef WORDS_BIGENDIAN
  /* #  undef WORDS_BIGENDIAN */
  # endif
  #endif
''
