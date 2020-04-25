{ rev    ? "c7e0e9ed5abd0043e50ee371129fcb8640264fc4"
, sha256 ? "0c28mpvjhjc8kiwj2w8zcjsr2rayw989a1wnsqda71zpcyas3mq2"
, pkgs   ? import (builtins.fetchTarball { inherit sha256;
    url = "https://github.com/NixOS/nixpkgs/archive/${rev}.tar.gz";
  }) { }

, stdenv ? if useClang
           then (if pkgs.stdenv.cc.isClang
                 then pkgs.stdenv
                 else pkgs.llvmPackages_latest.stdenv)
           else (if pkgs.stdenv.cc.isGNU
                 then pkgs.stdenv
                 else pkgs.gcc.stdenv)
, lib ? pkgs.lib

, debug              ? false # Debug Build
, useClang           ? false # Use Clang over GCC
, useJemalloc        ? true  # Use the Dynamic Memory Allocator
, withGraphicsMagick ? true  # Allow Media Thumbnails
}:

let
  pname = "matrix-construct";
  version = "development";

  source = let
    srcFilter = n: t: (lib.hasSuffix ".cc" n || lib.hasSuffix ".h" n || lib.hasSuffix ".S" n
                    || lib.hasSuffix ".md" n || t == "directory") && lib.cleanSourceFilter n t;
    repo = lib.cleanSourceWith { filter = srcFilter; src = ./.; };

    buildFileWith = root: name: type: rec {
      inherit name; file = "${root}/${name}";
      path = if type == "directory" then buildFarmFrom name file else "${file}";
    };
    buildFarm = root: lib.mapAttrsToList (buildFileWith root) (builtins.readDir root);
    buildFarmFrom = basename: root: pkgs.linkFarm (lib.strings.sanitizeDerivationName basename) (buildFarm root);
  in buildFarmFrom "construct" repo;

  rocksdb-pinned = pkgs.rocksdb.overrideAttrs (super: rec {
    version = "5.16.6";
    src = pkgs.fetchFromGitHub {
      owner = "facebook"; repo = "rocksdb"; rev = "v${version}";
      sha256 = "0yy09myzbi99qdmh2c2mxlddr12pwxzh66ym1y6raaqglrsmax66";
    };
    NIX_CFLAGS_COMPILE = "${super.NIX_CFLAGS_COMPILE} -Wno-error=redundant-move";
  });

  VERSION_COMMIT_CMD = "git rev-parse --short HEAD";
  VERSION_BRANCH_CMD = "git rev-parse --abbrev-ref HEAD";
  VERSION_TAG_CMD = "git describe --tags --abbrev=0 --dirty --always";
  VERSION_CMD = "git describe --tags --always";
  runWithGit = id: cmd: lib.removeSuffix "\n" (builtins.readFile (pkgs.runCommandNoCCLocal "construct-${id}" {
    buildInputs = [ pkgs.git ];
  } "cd ${./.} && ${cmd} > $out"));

  CXXOPTS = "-pipe -mtune=generic -O3 -fgcse-sm -fgcse-las -fsched-stalled-insns=0 -fsched-pressure -fsched-spec-load -fira-hoist-pressure -fbranch-target-load-optimize -frerun-loop-opt -fdevirtualize-at-ltrans -fipa-pta -fmodulo-sched -fmodulo-sched-allow-regmoves -ftracer -ftree-loop-im -ftree-switch-conversion -g -ggdb -frecord-gcc-switches -fstack-protector-explicit -fvtable-verify=none -fvisibility-inlines-hidden -fnothrow-opt -fno-threadsafe-statics -fverbose-asm -fsigned-char";
  WARNOPTS = "-Wall -Wextra -Wpointer-arith -Wcast-align -Wcast-qual -Wfloat-equal -Wwrite-strings -Wparentheses -Wundef -Wpacked -Wformat -Wformat-y2k -Wformat-nonliteral -Wstrict-aliasing=2 -Wstrict-overflow=5 -Wdisabled-optimization -Winvalid-pch -Winit-self -Wuninitialized -Wunreachable-code -Wno-overloaded-virtual -Wnon-virtual-dtor -Wnoexcept -Wsized-deallocation -Wctor-dtor-privacy -Wsign-promo -Wtrampolines -Wduplicated-cond -Wrestrict -Wnull-dereference -Wplacement-new=2 -Wundef -Wodr -Werror=return-type -Wno-missing-field-initializers -Wno-unused -Wno-unused-function -Wno-unused-label -Wno-unused-value -Wno-unused-variable -Wno-unused-parameter -Wno-endif-labels -Wmissing-noreturn -Wno-unknown-attributes -Wno-unknown-pragmas -Wlogical-op -Wformat-security -Wstack-usage=16384 -Wframe-larger-than=8192 -Walloca";

  includes = stdenv.mkDerivation rec {
    name = "${pname}-includes";
    src = "${source}/include/";

    configHeader = with pkgs; writeText "config.h" ''
      /* Current package */
      #define BRANDING_NAME "construct"

      /* Current version */
      #define BRANDING_VERSION "${runWithGit "version" VERSION_CMD}"

      /* Define this if you are profiling. */
      /* #undef CHARYBDIS_PROFILE */

      /* Define if custom branding is enabled. */
      /* #undef CUSTOM_BRANDING */

      /* Define to 1 if your C++ compiler doesn't accept -c and -o together. */
      /* #undef CXX_NO_MINUS_C_MINUS_O */

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
      #define IRCD_ALLOCATOR_USE_DEFAULT 1

      /* Use jemalloc as the allocator */
      /* #undef IRCD_ALLOCATOR_USE_JEMALLOC */

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
      #define RB_MODULE_DIR "@out@/lib/modules/construct"
      #define RB_MXID_MAXLEN 255
      #define RB_OPTIMIZE_LEVEL 3
      #define RB_OS "${stdenv.system}"
      #define RB_PREFIX "@out@"
      #define RB_RUN_DIR "/construct"
      #define RB_TIME_CONFIGURED 0
      /* #undef RB_UNTUNED */
      #define RB_VERSION "${runWithGit "version" VERSION_CMD}"
      #define RB_VERSION_BRANCH "${runWithGit "version-branch" VERSION_BRANCH_CMD}"
      #define RB_VERSION_COMMIT "${runWithGit "version-commit" VERSION_COMMIT_CMD}"
      #define RB_VERSION_TAG "${runWithGit "version-tag" VERSION_TAG_CMD}"
      #define RB_WEBAPP_DIR "@out@/share/construct/webapp"

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

      /* Define to 2 if the system does not provide POSIX.1 features except with
         this defined. */
      /* #undef _POSIX_1_SOURCE */

      /* Define to 1 if you need to in order for `stat' and other things to work. */
      /* #undef _POSIX_SOURCE */

      /* Define to empty if `const' does not conform to ANSI C. */
      /* #undef const */

      /* Define to `int' if <sys/types.h> doesn't define. */
      /* #undef gid_t */

      /* Define to `__inline__' or `__inline' if that's what the C compiler
         calls it, or to nothing if 'inline' is not supported under any name.  */
      #ifndef __cplusplus
      /* #undef inline */
      #endif

      /* Define to `int' if <sys/types.h> does not define. */
      /* #undef pid_t */

      /* Define to `unsigned int' if <sys/types.h> does not define. */
      /* #undef size_t */

      /* Define to `int' if <sys/types.h> does not define. */
      /* #undef ssize_t */

      /* Define to `int' if <sys/types.h> doesn't define. */
      /* #undef uid_t */
    '';
    buildInputs = with pkgs; [
      boost openssl
    ];

    buildPhase = with pkgs; ''
      mkdir -p $out
      cp -Lr --no-preserve=all ${source}/include/ircd $out/
      cd $out/ircd
      substituteAll ${configHeader} config.h
      $CXX -std=gnu++17 ${CXXOPTS} -x c++-header -pthread -fpch-deps -o ircd.h.gch -DHAVE_CONFIG_H -DPCH -DNDEBUG ${WARNOPTS} -I$out ircd.h
      $CXX -std=gnu++17 ${CXXOPTS} -x c++-header -pthread -fpch-deps -fPIC -o asio.h.gch -DHAVE_CONFIG_H -DPCH -DNDEBUG ${WARNOPTS} -I$out -I${boost.dev}/include -DPIC asio.h
      $CXX -std=gnu++17 ${CXXOPTS} -x c++-header -pthread -fpch-deps -o matrix.h.gch -DHAVE_CONFIG_H -DPCH -DNDEBUG ${WARNOPTS} -I$out matrix.h
      cp matrix.h matrix.pic.h
      $CXX -std=gnu++17 ${CXXOPTS} -x c++-header -pthread -fpch-deps -fPIC -o matrix.pic.h.gch -DHAVE_CONFIG_H -DPCH -DNDEBUG ${WARNOPTS} -I$out -DPIC matrix.pic.h
      $CXX -std=gnu++17 ${CXXOPTS} -x c++-header -pthread -fpch-deps -I${boost.dev}/include -fPIC -o spirit.h.gch -DHAVE_CONFIG_H -DPCH -DNDEBUG ${WARNOPTS} -I$out -DPIC spirit.h
      cp ircd.h ircd.pic.h
      $CXX -std=gnu++17 ${CXXOPTS} -x c++-header -pthread -fpch-deps -fPIC -o ircd.pic.h.gch -DHAVE_CONFIG_H -DPCH -DNDEBUG ${WARNOPTS} -I$out -DPIC ircd.pic.h
    '';

    installPhase = "true";
  };
in stdenv.mkDerivation rec {
  inherit pname version;
  src = source;

  buildPhase = with pkgs; ''
    WORK=$(pwd)
    set +x

    cd $WORK/ircd
    . ${pkgs.writeScript "build-ircd" ''
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o assert.lo assert.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o allocator_gnu.lo allocator_gnu.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o allocator_je.lo allocator_je.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o vg.lo vg.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${boost.dev}/include -include ircd/asio.h -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o exception.lo exception.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o util.lo util.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o demangle.lo demangle.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o backtrace.lo backtrace.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${boost.dev}/include -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o locale.lo locale.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o timedate.lo timedate.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${boost.dev}/include -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o lex_cast.lo lex_cast.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o stringops.lo stringops.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o globular.lo globular.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${boost.dev}/include -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o tokens.lo tokens.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${boost.dev}/include -include ircd/spirit.h -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -fno-var-tracking -fno-var-tracking-assignments -femit-struct-debug-baseonly -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o parse.lo parse.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o rand.lo rand.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${boost.dev}/include -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o base.lo base.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o crh.lo crh.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${boost.dev}/include -include ircd/spirit.h -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -fno-var-tracking -fno-var-tracking-assignments -femit-struct-debug-baseonly -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o fmt.lo fmt.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${boost.dev}/include -include ircd/spirit.h -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -fno-var-tracking -fno-var-tracking-assignments -femit-struct-debug-baseonly -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o json.lo json.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o cbor.lo cbor.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o allocator.lo allocator.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o conf.lo conf.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o logger.lo logger.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o stats.lo stats.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o info.lo info.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o run.lo run.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o magic.lo magic.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o sodium.lo sodium.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${boost.dev}/include -include ircd/spirit.h -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -fno-var-tracking -fno-var-tracking-assignments -femit-struct-debug-baseonly -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o rfc3986.lo rfc3986.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o rfc1035.lo rfc1035.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${boost.dev}/include -include ircd/spirit.h -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -fno-var-tracking -fno-var-tracking-assignments -femit-struct-debug-baseonly -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o http.lo http.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o http2.lo http2.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${boost.dev}/include -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o prof.lo prof.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o prof_linux.lo prof_linux.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${boost.dev}/include -include ircd/asio.h -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o fs.lo fs.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${boost.dev}/include -include ircd/asio.h -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o fs_path.lo fs_path.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${boost.dev}/include -include ircd/asio.h -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o ios.lo ios.cc
      libtool --tag=CC --mode=compile gcc -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -DPCH -DNDEBUG ${WARNOPTS} -pipe -c -o ctx_x86_64.lo ctx_x86_64.S
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${boost.dev}/include -include ircd/asio.h -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o ctx.lo ctx.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${boost.dev}/include -include ircd/asio.h -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o ctx_eh.lo ctx_eh.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${boost.dev}/include -include ircd/asio.h -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o ctx_ole.lo ctx_ole.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${boost.dev}/include -include ircd/asio.h -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o fs_aio.lo fs_aio.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${boost.dev}/include -include ircd/asio.h -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o fs_iou.lo fs_iou.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${boost.dev}/include -include ircd/asio.h -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o mods.lo mods.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o mods_ldso.lo mods_ldso.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -I${rocksdb-pinned.out}/include -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o db_write_thread.lo db_write_thread.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -I${rocksdb-pinned.out}/include -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o db_crc32.lo db_crc32.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o db_port.lo db_port.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o db_env.lo db_env.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o db.lo db.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${boost.dev}/include -include ircd/asio.h -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o net.lo net.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${boost.dev}/include -include ircd/asio.h -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o net_addrs.lo net_addrs.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${boost.dev}/include -include ircd/asio.h -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o net_dns.lo net_dns.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o net_dns_netdb.lo net_dns_netdb.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o net_dns_cache.lo net_dns_cache.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${boost.dev}/include -include ircd/asio.h -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o net_dns_resolver.lo net_dns_resolver.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${boost.dev}/include -include ircd/asio.h -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o net_listener.lo net_listener.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${boost.dev}/include -include ircd/spirit.h -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -fno-var-tracking -fno-var-tracking-assignments -femit-struct-debug-baseonly -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o rfc1459.lo rfc1459.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o openssl.lo openssl.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${boost.dev}/include -include ircd/asio.h -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o client.lo client.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${boost.dev}/include -include ircd/asio.h -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o server.lo server.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o resource.lo resource.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${boost.dev}/include -include ircd/asio.h -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o net_listener_udp.lo net_listener_udp.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o ircd.lo ircd.cc
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=initial-exec -pthread ${CXXOPTS} -version-info 3:2:0 -Wl,--no-undefined-version -Wl,--weak-unresolved-symbols -Wl,--unresolved-symbols=ignore-in-shared-libs -Wl,-z,nodelete -Wl,-z,nodlopen -Wl,-z,lazy -L${boost.out}/lib -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o libircd.la -rpath $out/lib assert.lo info.lo allocator.lo allocator_gnu.lo allocator_je.lo vg.lo exception.lo util.lo demangle.lo backtrace.lo locale.lo timedate.lo lex_cast.lo stringops.lo globular.lo tokens.lo parse.lo rand.lo base.lo crh.lo fmt.lo json.lo cbor.lo conf.lo stats.lo logger.lo run.lo magic.lo sodium.lo openssl.lo rfc1459.lo rfc3986.lo rfc1035.lo http.lo http2.lo prof.lo prof_linux.lo fs.lo fs_path.lo ios.lo ctx_x86_64.lo ctx.lo ctx_eh.lo ctx_ole.lo fs_aio.lo fs_iou.lo mods.lo mods_ldso.lo db_write_thread.lo db_crc32.lo db_port.lo db_env.lo db.lo net.lo net_addrs.lo net_dns.lo net_dns_netdb.lo net_dns_cache.lo net_dns_resolver.lo net_listener.lo net_listener_udp.lo server.lo client.lo resource.lo ircd.lo -lrocksdb -lboost_coroutine -lboost_context -lboost_thread -lboost_filesystem -lboost_chrono -lboost_system -lssl -lcrypto -L${libsodium.out}/lib -lsodium -lmagic -lpthread -latomic -lrocksdb -ldl 
    ''}

    cd $WORK/matrix
    . ${pkgs.writeScript "build-matrix" ''
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o name.lo name.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o dbs_event_idx.lo dbs_event_idx.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o dbs_event_json.lo dbs_event_json.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o dbs_event_column.lo dbs_event_column.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o dbs.lo dbs.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o dbs_event_refs.lo dbs_event_refs.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o dbs_event_horizon.lo dbs_event_horizon.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o dbs_event_type.lo dbs_event_type.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o dbs_event_state.lo dbs_event_state.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o dbs_event_sender.lo dbs_event_sender.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o dbs_room_events.lo dbs_room_events.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${boost.dev}/include -include ircd/spirit.h -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -fno-var-tracking -fno-var-tracking-assignments -femit-struct-debug-baseonly -pthread -ftls-model=local-dynamic -c -o id.lo id.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o dbs_room_type.lo dbs_room_type.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o dbs_room_state.lo dbs_room_state.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o dbs_room_state_space.lo dbs_room_state_space.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o dbs_room_joined.lo dbs_room_joined.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o dbs_room_head.lo dbs_room_head.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o dbs_desc.lo dbs_desc.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o hook.lo hook.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o event_conforms.lo event_conforms.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o event_fetch.lo event_fetch.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o event_cached.lo event_cached.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o event_get.lo event_get.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o event_id.lo event_id.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o event_index.lo event_index.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o event_prefetch.lo event_prefetch.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o event_prev.lo event_prev.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o event_refs.lo event_refs.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o room.lo room.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o room_aliases.lo room_aliases.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o room_bootstrap.lo room_bootstrap.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o room_create.lo room_create.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o room_events.lo room_events.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o room_head.lo room_head.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o room_join.lo room_join.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o room_leave.lo room_leave.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o room_visible.lo room_visible.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o room_members.lo room_members.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o room_origins.lo room_origins.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o room_type.lo room_type.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o room_power.lo room_power.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o room_state.lo room_state.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o room_state_history.lo room_state_history.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o room_state_space.lo room_state_space.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o room_server_acl.lo room_server_acl.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o room_stats.lo room_stats.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o user.lo user.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o user_account_data.lo user_account_data.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o user_devices.lo user_devices.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o user_events.lo user_events.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o user_filter.lo user_filter.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o user_ignores.lo user_ignores.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o user_mitsein.lo user_mitsein.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o user_notifications.lo user_notifications.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o user_profile.lo user_profile.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o user_pushers.lo user_pushers.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o user_pushrules.lo user_pushrules.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o user_register.lo user_register.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o user_room_account_data.lo user_room_account_data.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o user_room_tags.lo user_room_tags.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o user_rooms.lo user_rooms.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o user_tokens.lo user_tokens.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o bridge.lo bridge.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o breadcrumb_rooms.lo breadcrumb_rooms.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o burst.lo burst.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o display_name.lo display_name.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o event_append.lo event_append.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o event_horizon.lo event_horizon.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o events.lo events.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o fed.lo fed.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o feds.lo feds.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o fetch.lo fetch.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o gossip.lo gossip.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o request.lo request.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o keys.lo keys.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o node.lo node.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o presence.lo presence.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o pretty.lo pretty.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o receipt.lo receipt.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o rooms.lo rooms.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o membership.lo membership.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o rooms_summary.lo rooms_summary.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o sync.lo sync.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o typing.lo typing.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o users.lo users.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o users_servers.lo users_servers.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o error.lo error.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o push.lo push.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o filter.lo filter.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o room_auth.lo room_auth.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o txn.lo txn.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o event.lo event.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o vm.lo vm.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o vm_eval.lo vm_eval.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o vm_inject.lo vm_inject.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o homeserver.lo homeserver.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o resource.lo resource.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o matrix.lo matrix.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o init_backfill.lo init_backfill.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -pthread -ftls-model=local-dynamic ${CXXOPTS} -c -o vm_execute.lo vm_execute.cc
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -pthread -ftls-model=local-dynamic ${CXXOPTS} -version-info 0:1:0 -Wl,--no-undefined-version -Wl,--allow-shlib-undefined -Wl,--unresolved-symbols=ignore-in-shared-libs -Wl,-z,lazy -L$WORK/ircd -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o libircd_matrix.la -rpath $out/lib name.lo id.lo dbs.lo dbs_event_idx.lo dbs_event_json.lo dbs_event_column.lo dbs_event_refs.lo dbs_event_horizon.lo dbs_event_sender.lo dbs_event_type.lo dbs_event_state.lo dbs_room_events.lo dbs_room_type.lo dbs_room_state.lo dbs_room_state_space.lo dbs_room_joined.lo dbs_room_head.lo dbs_desc.lo hook.lo event.lo event_cached.lo event_conforms.lo event_fetch.lo event_get.lo event_id.lo event_index.lo event_prefetch.lo event_prev.lo event_refs.lo room.lo room_auth.lo room_aliases.lo room_bootstrap.lo room_create.lo room_events.lo room_head.lo room_join.lo room_leave.lo room_visible.lo room_members.lo room_origins.lo room_type.lo room_power.lo room_state.lo room_state_history.lo room_state_space.lo room_server_acl.lo room_stats.lo user.lo user_account_data.lo user_devices.lo user_events.lo user_filter.lo user_ignores.lo user_mitsein.lo user_notifications.lo user_profile.lo user_pushers.lo user_pushrules.lo user_register.lo user_room_account_data.lo user_room_tags.lo user_rooms.lo user_tokens.lo bridge.lo breadcrumb_rooms.lo burst.lo display_name.lo event_append.lo event_horizon.lo events.lo fed.lo feds.lo fetch.lo gossip.lo request.lo keys.lo node.lo presence.lo pretty.lo receipt.lo rooms.lo membership.lo rooms_summary.lo sync.lo typing.lo users.lo users_servers.lo error.lo push.lo filter.lo txn.lo vm.lo vm_eval.lo vm_inject.lo vm_execute.lo init_backfill.lo homeserver.lo resource.lo matrix.lo -lrocksdb -ldl 
    ''}

    cd $WORK/modules
    . ${pkgs.writeScript "build-modules" ''
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o m_breadcrumb_rooms.lo m_breadcrumb_rooms.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o m_bridge.lo m_bridge.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o m_control.lo m_control.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o m_device.lo m_device.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o m_device_list_update.lo m_device_list_update.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o m_direct.lo m_direct.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o m_direct_to_device.lo m_direct_to_device.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o m_ignored_user_list.lo m_ignored_user_list.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o m_listen.lo m_listen.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o m_noop.lo m_noop.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o m_presence.lo m_presence.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o m_profile.lo m_profile.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o m_push.lo m_push.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o m_receipt.lo m_receipt.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o m_relation.lo m_relation.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o m_room_aliases.lo m_room_aliases.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o m_room_canonical_alias.lo m_room_canonical_alias.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o m_room_create.lo m_room_create.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o m_room_history_visibility.lo m_room_history_visibility.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o m_room_join_rules.lo m_room_join_rules.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o m_room_member.lo m_room_member.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o m_room_message.lo m_room_message.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o m_room_name.lo m_room_name.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o m_room_power_levels.lo m_room_power_levels.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o m_room_redaction.lo m_room_redaction.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o m_room_server_acl.lo m_room_server_acl.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o m_room_third_party_invite.lo m_room_third_party_invite.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o m_vm_fetch.lo m_vm_fetch.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o net_dns_cache.lo net_dns_cache.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o stats.lo stats.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o console.lo console.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o web_root.lo web_root.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o m_command.lo m_command.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o web_hook.lo web_hook.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o well_known.lo well_known.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -I${graphicsmagick.out}/include/GraphicsMagick -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o magick_la-magick.lo `test -f 'magick.cc' || echo './'`magick.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/versions.lo client/versions.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/events.lo client/events.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/logout.lo client/logout.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/sync.lo client/sync.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/presence.lo client/presence.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/profile.lo client/profile.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/devices.lo client/devices.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/pushers.lo client/pushers.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/publicrooms.lo client/publicrooms.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/createroom.lo client/createroom.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/pushrules.lo client/pushrules.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/join.lo client/join.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/publicised_groups.lo client/publicised_groups.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/initialsync.lo client/initialsync.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/search.lo client/search.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/joined_groups.lo client/joined_groups.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/register_available.lo client/register_available.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/capabilities.lo client/capabilities.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/send_to_device.lo client/send_to_device.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/delete_devices.lo client/delete_devices.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/notifications.lo client/notifications.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/register_email.lo client/register_email.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/rooms/messages.lo client/rooms/messages.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/rooms/state.lo client/rooms/state.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/rooms/members.lo client/rooms/members.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/rooms/context.lo client/rooms/context.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/rooms/event.lo client/rooms/event.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/rooms/send.lo client/rooms/send.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/rooms/typing.lo client/rooms/typing.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/rooms/redact.lo client/rooms/redact.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/rooms/receipt.lo client/rooms/receipt.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/rooms/join.lo client/rooms/join.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/rooms/invite.lo client/rooms/invite.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/rooms/leave.lo client/rooms/leave.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/rooms/forget.lo client/rooms/forget.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/rooms/kick.lo client/rooms/kick.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/rooms/ban.lo client/rooms/ban.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/rooms/unban.lo client/rooms/unban.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/rooms/read_markers.lo client/rooms/read_markers.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/rooms/initialsync.lo client/rooms/initialsync.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/rooms/report.lo client/rooms/report.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/rooms/relations.lo client/rooms/relations.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/rooms/upgrade.lo client/rooms/upgrade.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/rooms/rooms.lo client/rooms/rooms.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/user/openid.lo client/user/openid.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/user/filter.lo client/user/filter.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/user/account_data.lo client/user/account_data.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/user/rooms.lo client/user/rooms.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/user/user.lo client/user/user.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/login.lo client/login.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/account/whoami.lo client/account/whoami.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/account/password.lo client/account/password.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/account/deactivate.lo client/account/deactivate.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/account/account.lo client/account/account.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/directory/room.lo client/directory/room.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/directory/user.lo client/directory/user.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/directory/list/room.lo client/directory/list/room.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/directory/list/appservice.lo client/directory/list/appservice.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/voip/turnserver.lo client/voip/turnserver.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/thirdparty/protocols.lo client/thirdparty/protocols.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/sync/account_data.lo client/sync/account_data.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/sync/presence.lo client/sync/presence.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/sync/rooms.lo client/sync/rooms.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/sync/to_device.lo client/sync/to_device.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/sync/device_lists.lo client/sync/device_lists.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/sync/device_one_time_keys_count.lo client/sync/device_one_time_keys_count.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/sync/rooms/account_data.lo client/sync/rooms/account_data.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/sync/rooms/ephemeral.lo client/sync/rooms/ephemeral.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/sync/rooms/state.lo client/sync/rooms/state.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/sync/rooms/timeline.lo client/sync/rooms/timeline.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/sync/rooms/unread_notifications.lo client/sync/rooms/unread_notifications.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/sync/rooms/summary.lo client/sync/rooms/summary.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/sync/rooms/ephemeral/receipt.lo client/sync/rooms/ephemeral/receipt.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/sync/rooms/ephemeral/typing.lo client/sync/rooms/ephemeral/typing.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/keys/upload.lo client/keys/upload.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/keys/query.lo client/keys/query.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/keys/claim.lo client/keys/claim.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/keys/changes.lo client/keys/changes.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/keys/signatures/upload.lo client/keys/signatures/upload.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/keys/device_signing/upload.lo client/keys/device_signing/upload.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/room_keys/version.lo client/room_keys/version.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/room_keys/keys.lo client/room_keys/keys.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o federation/send.lo federation/send.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o federation/event.lo federation/event.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o federation/get_missing_events.lo federation/get_missing_events.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o federation/get_groups_publicised.lo federation/get_groups_publicised.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o federation/version.lo federation/version.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o federation/sender.lo federation/sender.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o federation/query.lo federation/query.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o federation/invite.lo federation/invite.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o federation/invite2.lo federation/invite2.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o federation/make_join.lo federation/make_join.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o federation/send_join.lo federation/send_join.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o federation/state_ids.lo federation/state_ids.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o federation/state.lo federation/state.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o federation/make_leave.lo federation/make_leave.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o federation/send_leave.lo federation/send_leave.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o federation/backfill.lo federation/backfill.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o federation/backfill_ids.lo federation/backfill_ids.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o federation/event_auth.lo federation/event_auth.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o federation/query_auth.lo federation/query_auth.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o federation/publicrooms.lo federation/publicrooms.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o federation/user_devices.lo federation/user_devices.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o federation/user_keys_query.lo federation/user_keys_query.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o federation/user_keys_claim.lo federation/user_keys_claim.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o federation/rooms.lo federation/rooms.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o identity/v1.lo identity/v1.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o identity/pubkey.lo identity/pubkey.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o key/server.lo key/server.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o key/query.lo key/query.cc
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o m_breadcrumb_rooms.la -rpath $out/lib/modules/construct m_breadcrumb_rooms.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o m_bridge.la -rpath $out/lib/modules/construct m_bridge.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o m_command.la -rpath $out/lib/modules/construct m_command.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o m_control.la -rpath $out/lib/modules/construct m_control.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o m_device.la -rpath $out/lib/modules/construct m_device.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o m_device_list_update.la -rpath $out/lib/modules/construct m_device_list_update.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o m_direct.la -rpath $out/lib/modules/construct m_direct.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o m_direct_to_device.la -rpath $out/lib/modules/construct m_direct_to_device.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o m_ignored_user_list.la -rpath $out/lib/modules/construct m_ignored_user_list.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o m_listen.la -rpath $out/lib/modules/construct m_listen.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o m_noop.la -rpath $out/lib/modules/construct m_noop.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o m_presence.la -rpath $out/lib/modules/construct m_presence.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o m_profile.la -rpath $out/lib/modules/construct m_profile.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o m_push.la -rpath $out/lib/modules/construct m_push.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o m_receipt.la -rpath $out/lib/modules/construct m_receipt.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o m_relation.la -rpath $out/lib/modules/construct m_relation.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o m_room_aliases.la -rpath $out/lib/modules/construct m_room_aliases.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o m_room_canonical_alias.la -rpath $out/lib/modules/construct m_room_canonical_alias.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o m_room_create.la -rpath $out/lib/modules/construct m_room_create.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o m_room_history_visibility.la -rpath $out/lib/modules/construct m_room_history_visibility.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o m_room_join_rules.la -rpath $out/lib/modules/construct m_room_join_rules.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o m_room_member.la -rpath $out/lib/modules/construct m_room_member.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o m_room_message.la -rpath $out/lib/modules/construct m_room_message.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o m_room_name.la -rpath $out/lib/modules/construct m_room_name.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o m_room_power_levels.la -rpath $out/lib/modules/construct m_room_power_levels.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o m_room_redaction.la -rpath $out/lib/modules/construct m_room_redaction.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o m_room_server_acl.la -rpath $out/lib/modules/construct m_room_server_acl.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o m_room_third_party_invite.la -rpath $out/lib/modules/construct m_room_third_party_invite.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o m_vm_fetch.la -rpath $out/lib/modules/construct m_vm_fetch.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o media/download.lo media/download.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o media/upload.lo media/upload.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o media/thumbnail.lo media/thumbnail.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o media/preview_url.lo media/preview_url.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o media/config.lo media/config.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o media/media.lo media/media.cc
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o net_dns_cache.la -rpath $out/lib/modules/construct net_dns_cache.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o stats.la -rpath $out/lib/modules/construct stats.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o console.la -rpath $out/lib/modules/construct console.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o web_root.la -rpath $out/lib/modules/construct web_root.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o web_hook.la -rpath $out/lib/modules/construct web_hook.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o well_known.la -rpath $out/lib/modules/construct well_known.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o magick.la -rpath $out/lib/modules/construct magick_la-magick.lo -lGraphicsMagick++ -lGraphicsMagickWand -lGraphicsMagick -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_versions.la -rpath $out/lib/modules/construct client/versions.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_events.la -rpath $out/lib/modules/construct client/events.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_login.la -rpath $out/lib/modules/construct client/login.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_logout.la -rpath $out/lib/modules/construct client/logout.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_sync.la -rpath $out/lib/modules/construct client/sync.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_presence.la -rpath $out/lib/modules/construct client/presence.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_profile.la -rpath $out/lib/modules/construct client/profile.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_devices.la -rpath $out/lib/modules/construct client/devices.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_pushers.la -rpath $out/lib/modules/construct client/pushers.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_publicrooms.la -rpath $out/lib/modules/construct client/publicrooms.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_createroom.la -rpath $out/lib/modules/construct client/createroom.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_pushrules.la -rpath $out/lib/modules/construct client/pushrules.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_join.la -rpath $out/lib/modules/construct client/join.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_publicised_groups.la -rpath $out/lib/modules/construct client/publicised_groups.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_initialsync.la -rpath $out/lib/modules/construct client/initialsync.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_search.la -rpath $out/lib/modules/construct client/search.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_joined_groups.la -rpath $out/lib/modules/construct client/joined_groups.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_register_available.la -rpath $out/lib/modules/construct client/register_available.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_capabilities.la -rpath $out/lib/modules/construct client/capabilities.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_send_to_device.la -rpath $out/lib/modules/construct client/send_to_device.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_delete_devices.la -rpath $out/lib/modules/construct client/delete_devices.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_notifications.la -rpath $out/lib/modules/construct client/notifications.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_register_email.la -rpath $out/lib/modules/construct client/register_email.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_rooms.la -rpath $out/lib/modules/construct client/rooms/messages.lo client/rooms/state.lo client/rooms/members.lo client/rooms/context.lo client/rooms/event.lo client/rooms/send.lo client/rooms/typing.lo client/rooms/redact.lo client/rooms/receipt.lo client/rooms/join.lo client/rooms/invite.lo client/rooms/leave.lo client/rooms/forget.lo client/rooms/kick.lo client/rooms/ban.lo client/rooms/unban.lo client/rooms/read_markers.lo client/rooms/initialsync.lo client/rooms/report.lo client/rooms/relations.lo client/rooms/upgrade.lo client/rooms/rooms.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_user.la -rpath $out/lib/modules/construct client/user/openid.lo client/user/filter.lo client/user/account_data.lo client/user/rooms.lo client/user/user.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_directory_room.la -rpath $out/lib/modules/construct client/directory/room.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_directory_user.la -rpath $out/lib/modules/construct client/directory/user.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_directory_list_room.la -rpath $out/lib/modules/construct client/directory/list/room.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_directory_list_appservice.la -rpath $out/lib/modules/construct client/directory/list/appservice.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_voip_turnserver.la -rpath $out/lib/modules/construct client/voip/turnserver.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_thirdparty_protocols.la -rpath $out/lib/modules/construct client/thirdparty/protocols.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_sync_account_data.la -rpath $out/lib/modules/construct client/sync/account_data.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_sync_presence.la -rpath $out/lib/modules/construct client/sync/presence.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_sync_rooms.la -rpath $out/lib/modules/construct client/sync/rooms.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_sync_to_device.la -rpath $out/lib/modules/construct client/sync/to_device.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_sync_device_lists.la -rpath $out/lib/modules/construct client/sync/device_lists.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_sync_device_one_time_keys_count.la -rpath $out/lib/modules/construct client/sync/device_one_time_keys_count.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_sync_rooms_account_data.la -rpath $out/lib/modules/construct client/sync/rooms/account_data.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_sync_rooms_ephemeral.la -rpath $out/lib/modules/construct client/sync/rooms/ephemeral.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_sync_rooms_state.la -rpath $out/lib/modules/construct client/sync/rooms/state.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_sync_rooms_timeline.la -rpath $out/lib/modules/construct client/sync/rooms/timeline.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_sync_rooms_unread_notifications.la -rpath $out/lib/modules/construct client/sync/rooms/unread_notifications.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_sync_rooms_summary.la -rpath $out/lib/modules/construct client/sync/rooms/summary.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_sync_rooms_ephemeral_receipt.la -rpath $out/lib/modules/construct client/sync/rooms/ephemeral/receipt.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_sync_rooms_ephemeral_typing.la -rpath $out/lib/modules/construct client/sync/rooms/ephemeral/typing.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_keys_upload.la -rpath $out/lib/modules/construct client/keys/upload.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_keys_query.la -rpath $out/lib/modules/construct client/keys/query.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_keys_claim.la -rpath $out/lib/modules/construct client/keys/claim.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_keys_changes.la -rpath $out/lib/modules/construct client/keys/changes.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_keys_signatures_upload.la -rpath $out/lib/modules/construct client/keys/signatures/upload.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_keys_device_signing_upload.la -rpath $out/lib/modules/construct client/keys/device_signing/upload.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_room_keys_version.la -rpath $out/lib/modules/construct client/room_keys/version.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_room_keys_keys.la -rpath $out/lib/modules/construct client/room_keys/keys.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o federation/federation_send.la -rpath $out/lib/modules/construct federation/send.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o federation/federation_event.la -rpath $out/lib/modules/construct federation/event.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o federation/federation_get_missing_events.la -rpath $out/lib/modules/construct federation/get_missing_events.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o federation/federation_get_groups_publicised.la -rpath $out/lib/modules/construct federation/get_groups_publicised.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o federation/federation_version.la -rpath $out/lib/modules/construct federation/version.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o federation/federation_sender.la -rpath $out/lib/modules/construct federation/sender.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o federation/federation_query.la -rpath $out/lib/modules/construct federation/query.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o federation/federation_invite.la -rpath $out/lib/modules/construct federation/invite.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o federation/federation_invite2.la -rpath $out/lib/modules/construct federation/invite2.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o federation/federation_make_join.la -rpath $out/lib/modules/construct federation/make_join.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o federation/federation_send_join.la -rpath $out/lib/modules/construct federation/send_join.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o federation/federation_state_ids.la -rpath $out/lib/modules/construct federation/state_ids.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o federation/federation_state.la -rpath $out/lib/modules/construct federation/state.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o federation/federation_make_leave.la -rpath $out/lib/modules/construct federation/make_leave.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o federation/federation_send_leave.la -rpath $out/lib/modules/construct federation/send_leave.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o federation/federation_backfill.la -rpath $out/lib/modules/construct federation/backfill.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o federation/federation_backfill_ids.la -rpath $out/lib/modules/construct federation/backfill_ids.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o federation/federation_event_auth.la -rpath $out/lib/modules/construct federation/event_auth.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o federation/federation_query_auth.la -rpath $out/lib/modules/construct federation/query_auth.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o federation/federation_publicrooms.la -rpath $out/lib/modules/construct federation/publicrooms.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o federation/federation_user_devices.la -rpath $out/lib/modules/construct federation/user_devices.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o federation/federation_user_keys_query.la -rpath $out/lib/modules/construct federation/user_keys_query.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o federation/federation_user_keys_claim.la -rpath $out/lib/modules/construct federation/user_keys_claim.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o federation/federation_rooms.la -rpath $out/lib/modules/construct federation/rooms.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o identity/identity_v1.la -rpath $out/lib/modules/construct identity/v1.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o identity/identity_pubkey.la -rpath $out/lib/modules/construct identity/pubkey.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o key/key_server.la -rpath $out/lib/modules/construct key/server.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o key/key_query.la -rpath $out/lib/modules/construct key/query.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o media/media_media.la -rpath $out/lib/modules/construct media/download.lo media/upload.lo media/thumbnail.lo media/preview_url.lo media/config.lo media/media.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/account/3pid.lo client/account/3pid.cc
      libtool --tag=CXX --mode=compile g++ -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE -I${includes} -include ../include/ircd/matrix.pic.h -include ../include/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} -c -o client/register.lo client/register.cc
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_account.la -rpath $out/lib/modules/construct client/account/3pid.lo client/account/whoami.lo client/account/password.lo client/account/deactivate.lo client/account/account.lo -lrocksdb -ldl 
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version -Wl,--allow-shlib-undefined -Wl,-z,lazy -L$WORK/ircd -L$WORK/matrix -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o client/client_register.la -rpath $out/lib/modules/construct client/register.lo -lrocksdb -ldl 
    ''}

    cd $WORK/construct
    . ${pkgs.writeScript "build-construct" ''
      $CXX -std=gnu++17 -DHAVE_CONFIG_H -I${includes} -I${boost.dev}/include -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o construct.o construct.cc
      $CXX -std=gnu++17 -DHAVE_CONFIG_H -I${includes} -I${boost.dev}/include -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o lgetopt.o lgetopt.cc
      $CXX -std=gnu++17 -DHAVE_CONFIG_H -I${includes} -I${boost.dev}/include -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o console.o console.cc
      $CXX -std=gnu++17 -DHAVE_CONFIG_H -I${includes} -I${boost.dev}/include -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} -c -o signals.o signals.cc
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=initial-exec -pthread ${CXXOPTS} -dlopen self -Wl,--warn-execstack -Wl,--warn-common -Wl,--detect-odr-violations -Wl,--unresolved-symbols=report-all -Wl,--allow-shlib-undefined -Wl,--dynamic-list-data -Wl,--dynamic-list-cpp-new -Wl,--dynamic-list-cpp-typeinfo -Wl,--rosegment -Wl,-z,noexecstack -L$WORK/ircd -L../modules -L${boost.out}/lib -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment -o construct construct.o signals.o console.o lgetopt.o -lircd -lboost_coroutine -lboost_context -lboost_thread -lboost_filesystem -lboost_chrono -lboost_system -lssl -lcrypto -lpthread -latomic -lrocksdb -ldl 
    ''}

    cd $WORK
  '';

  installPhase = with pkgs; ''
    WORK=$(pwd)
    set +x

    cd ${includes}/ircd
    mkdir -p $out/include/ircd
    cp -r ./README.md $out/include/ircd/
    cp -r ./allocator.h $out/include/ircd/
    cp -r ./asio.h $out/include/ircd/
    cp -r ./assert.h $out/include/ircd/
    cp -r ./backtrace.h $out/include/ircd/
    cp -r ./base.h $out/include/ircd/
    cp -r ./buffer $out/include/ircd/
    cp -r ./byte_view.h $out/include/ircd/
    cp -r ./cbor $out/include/ircd/
    cp -r ./client.h $out/include/ircd/
    cp -r ./cmp.h $out/include/ircd/
    cp -r ./color.h $out/include/ircd/
    cp -r ./conf.h $out/include/ircd/
    cp -r ./config.h $out/include/ircd/
    cp -r ./crh.h $out/include/ircd/
    cp -r ./ctx $out/include/ircd/
    cp -r ./db $out/include/ircd/
    cp -r ./demangle.h $out/include/ircd/
    cp -r ./ed25519.h $out/include/ircd/
    cp -r ./exception.h $out/include/ircd/
    cp -r ./fmt.h $out/include/ircd/
    cp -r ./fs $out/include/ircd/
    cp -r ./globular.h $out/include/ircd/
    cp -r ./grammar.h $out/include/ircd/
    cp -r ./http.h $out/include/ircd/
    cp -r ./http2 $out/include/ircd/
    cp -r ./info.h $out/include/ircd/
    cp -r ./ios.h $out/include/ircd/
    cp -r ./iov.h $out/include/ircd/
    cp -r ./ircd.h $out/include/ircd/
    cp -r ./ircd.pic.h $out/include/ircd/
    cp -r ./js $out/include/ircd/
    cp -r ./js.h $out/include/ircd/
    cp -r ./json $out/include/ircd/
    cp -r ./leb128.h $out/include/ircd/
    cp -r ./lex_cast.h $out/include/ircd/
    cp -r ./locale.h $out/include/ircd/
    cp -r ./logger.h $out/include/ircd/
    cp -r ./m $out/include/ircd/
    cp -r ./magic.h $out/include/ircd/
    cp -r ./magick.h $out/include/ircd/
    cp -r ./matrix.h $out/include/ircd/
    cp -r ./matrix.pic.h $out/include/ircd/
    cp -r ./mods $out/include/ircd/
    cp -r ./nacl.h $out/include/ircd/
    cp -r ./net $out/include/ircd/
    cp -r ./openssl.h $out/include/ircd/
    cp -r ./panic.h $out/include/ircd/
    cp -r ./parse.h $out/include/ircd/
    cp -r ./pbc.h $out/include/ircd/
    cp -r ./portable.h $out/include/ircd/
    cp -r ./prof $out/include/ircd/
    cp -r ./rand.h $out/include/ircd/
    cp -r ./resource $out/include/ircd/
    cp -r ./rfc1035.h $out/include/ircd/
    cp -r ./rfc1459.h $out/include/ircd/
    cp -r ./rfc3986.h $out/include/ircd/
    cp -r ./run.h $out/include/ircd/
    cp -r ./server $out/include/ircd/
    cp -r ./simd.h $out/include/ircd/
    cp -r ./spirit.h $out/include/ircd/
    cp -r ./stamp-h1 $out/include/ircd/
    cp -r ./stats.h $out/include/ircd/
    cp -r ./stdinc.h $out/include/ircd/
    cp -r ./string_view.h $out/include/ircd/
    cp -r ./stringops.h $out/include/ircd/
    cp -r ./strl.h $out/include/ircd/
    cp -r ./strn.h $out/include/ircd/
    cp -r ./time.h $out/include/ircd/
    cp -r ./tokens.h $out/include/ircd/
    cp -r ./util $out/include/ircd/
    cp -r ./vector_view.h $out/include/ircd/

    cd $WORK/ircd
    mkdir -p $out/lib
    libtool   --mode=install ${coreutils.out}/bin/install -c   libircd.la $out/lib

    cd $WORK/matrix
    mkdir -p $out/lib
    libtool   --mode=install ${coreutils.out}/bin/install -c   libircd_matrix.la $out/lib

    cd $WORK/modules
    mkdir -p $out/lib/modules/construct
    libtool   --mode=install ${coreutils.out}/bin/install -c   client/client_versions.la client/client_events.la client/client_register.la client/client_login.la client/client_logout.la client/client_sync.la client/client_presence.la client/client_profile.la client/client_devices.la client/client_pushers.la client/client_publicrooms.la client/client_createroom.la client/client_pushrules.la client/client_join.la client/client_publicised_groups.la client/client_initialsync.la client/client_search.la client/client_joined_groups.la client/client_register_available.la client/client_capabilities.la client/client_send_to_device.la client/client_delete_devices.la client/client_notifications.la client/client_register_email.la client/client_rooms.la client/client_user.la client/client_account.la client/client_directory_room.la client/client_directory_user.la client/client_directory_list_room.la client/client_directory_list_appservice.la client/client_voip_turnserver.la client/client_thirdparty_protocols.la client/client_sync_account_data.la client/client_sync_presence.la client/client_sync_rooms.la client/client_sync_to_device.la client/client_sync_device_lists.la client/client_sync_device_one_time_keys_count.la client/client_sync_rooms_account_data.la client/client_sync_rooms_ephemeral.la client/client_sync_rooms_state.la client/client_sync_rooms_timeline.la client/client_sync_rooms_unread_notifications.la client/client_sync_rooms_summary.la client/client_sync_rooms_ephemeral_receipt.la client/client_sync_rooms_ephemeral_typing.la client/client_keys_upload.la client/client_keys_query.la client/client_keys_claim.la client/client_keys_changes.la client/client_keys_signatures_upload.la client/client_keys_device_signing_upload.la client/client_room_keys_version.la client/client_room_keys_keys.la $out/lib/modules/construct
    mkdir -p $out/lib/modules/construct
    libtool   --mode=install ${coreutils.out}/bin/install -c   federation/federation_send.la federation/federation_event.la federation/federation_get_missing_events.la federation/federation_get_groups_publicised.la federation/federation_version.la federation/federation_sender.la federation/federation_query.la federation/federation_invite.la federation/federation_invite2.la federation/federation_make_join.la federation/federation_send_join.la federation/federation_state_ids.la federation/federation_state.la federation/federation_make_leave.la federation/federation_send_leave.la federation/federation_backfill.la federation/federation_backfill_ids.la federation/federation_event_auth.la federation/federation_query_auth.la federation/federation_publicrooms.la federation/federation_user_devices.la federation/federation_user_keys_query.la federation/federation_user_keys_claim.la federation/federation_rooms.la $out/lib/modules/construct
    mkdir -p $out/lib/modules/construct
    libtool   --mode=install ${coreutils.out}/bin/install -c   identity/identity_v1.la identity/identity_pubkey.la $out/lib/modules/construct
    mkdir -p $out/lib/modules/construct
    libtool   --mode=install ${coreutils.out}/bin/install -c   key/key_server.la key/key_query.la $out/lib/modules/construct
    mkdir -p $out/lib/modules/construct
    libtool   --mode=install ${coreutils.out}/bin/install -c   m_breadcrumb_rooms.la m_bridge.la m_command.la m_control.la m_device.la m_device_list_update.la m_direct.la m_direct_to_device.la m_ignored_user_list.la m_listen.la m_noop.la m_presence.la m_profile.la m_push.la m_receipt.la m_relation.la m_room_aliases.la m_room_canonical_alias.la m_room_create.la m_room_history_visibility.la m_room_join_rules.la m_room_member.la m_room_message.la m_room_name.la m_room_power_levels.la m_room_redaction.la m_room_server_acl.la m_room_third_party_invite.la m_vm_fetch.la $out/lib/modules/construct
    mkdir -p $out/lib/modules/construct
    libtool   --mode=install ${coreutils.out}/bin/install -c   media/media_media.la $out/lib/modules/construct
    mkdir -p $out/lib/modules/construct
    libtool   --mode=install ${coreutils.out}/bin/install -c   net_dns_cache.la stats.la console.la web_root.la web_hook.la well_known.la magick.la $out/lib/modules/construct

    cd $WORK/share
    mkdir -p $out/share/construct/construct
    cp -r ./webapp $out/share/construct/construct/

    cd $WORK/construct
    mkdir -p $out/bin
    libtool   --mode=install ${coreutils.out}/bin/install -c construct $out/bin

    cd $WORK
  '';

  enableParallelBuilding = true;

  nativeBuildInputs = with pkgs; [
    libtool pkg-config
  ] ++ lib.optional useClang llvmPackages_latest.llvm
    ++ lib.optional useJemalloc jemalloc;
  buildInputs = with pkgs; [
    libsodium openssl file boost gmp rocksdb-pinned
  ] ++ lib.optional withGraphicsMagick graphicsmagick;
}
