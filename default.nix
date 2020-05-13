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
, useJemalloc        ? false # Use the Dynamic Memory Allocator
, withGraphicsMagick ? true  # Allow Media Thumbnails
}:

let
  pname = "matrix-construct";
  version = "development";

  source = let
    srcFilter = n: t: (lib.hasSuffix ".cc" n || lib.hasSuffix ".h" n || lib.hasSuffix ".S" n
                    || lib.hasSuffix ".md" n || t == "directory");
    repo = lib.cleanSourceWith { filter = srcFilter; src = lib.cleanSource ./.; };

    buildFileWith = root: name: type: rec {
      inherit name; file = "${root}/${name}";
      path = if type == "directory" then buildFarmFrom name file else "${file}";
    };
    buildFarm = root: lib.mapAttrsToList (buildFileWith root) (builtins.readDir root);
    buildFarmFrom = basename: root: pkgs.linkFarm (lib.strings.sanitizeDerivationName basename) (buildFarm root);
  in buildFarmFrom "construct" repo;

  buildArgs = buildInputs: nativeBuildInputs: {
    inherit buildInputs nativeBuildInputs;
    preferLocalBuild = true;
    allowSubstitutes = false;
  };

  VERSION_COMMIT_CMD = "git rev-parse --short HEAD";
  VERSION_BRANCH_CMD = "git rev-parse --abbrev-ref HEAD";
  VERSION_TAG_CMD = "git describe --tags --abbrev=0 --dirty --always --broken";
  VERSION_CMD = "git describe --tags --always --broken";
  runWithGit = id: cmd: lib.removeSuffix "\n" (builtins.readFile (pkgs.runCommandNoCCLocal "construct-${id}" {
    buildInputs = [ pkgs.git ];
  } "cd ${./.} && ${cmd} > $out"));
in stdenv.mkDerivation rec {
  inherit pname version;
  src = source;

  buildPhase = with pkgs; ''
  '';

  CXXOPTS = "-pipe -mtune=generic -O3 -fgcse-sm -fgcse-las -fsched-stalled-insns=0 -fsched-pressure -fsched-spec-load -fira-hoist-pressure -fbranch-target-load-optimize -frerun-loop-opt -fdevirtualize-at-ltrans -fipa-pta -fmodulo-sched -fmodulo-sched-allow-regmoves -ftracer -ftree-loop-im -ftree-switch-conversion -g -ggdb -frecord-gcc-switches -fstack-protector-explicit -fvtable-verify=none -fvisibility-inlines-hidden -fnothrow-opt -fno-threadsafe-statics -fverbose-asm -fsigned-char";
  WARNOPTS = "-Wall -Wextra -Wpointer-arith -Wcast-align -Wcast-qual -Wfloat-equal -Wwrite-strings -Wparentheses -Wundef -Wpacked -Wformat -Wformat-y2k -Wformat-nonliteral -Wstrict-aliasing=2 -Wstrict-overflow=5 -Wdisabled-optimization -Winvalid-pch -Winit-self -Wuninitialized -Wunreachable-code -Wno-overloaded-virtual -Wnon-virtual-dtor -Wnoexcept -Wsized-deallocation -Wctor-dtor-privacy -Wsign-promo -Wtrampolines -Wduplicated-cond -Wrestrict -Wnull-dereference -Wplacement-new=2 -Wundef -Wodr -Werror=return-type -Wno-missing-field-initializers -Wno-unused -Wno-unused-function -Wno-unused-label -Wno-unused-value -Wno-unused-variable -Wno-unused-parameter -Wno-endif-labels -Wmissing-noreturn -Wno-unknown-attributes -Wno-unknown-pragmas -Wlogical-op -Wformat-security -Wstack-usage=16384 -Wframe-larger-than=8192 -Walloca";

  nativeBuildInputs = with pkgs; [
    libtool makeWrapper
  ] ++ lib.optional useClang llvmPackages_latest.llvm;
  buildInputs = with pkgs; [
    libsodium openssl file boost gmp zlib jemalloc rocksdb
  ] ++ lib.optional withGraphicsMagick graphicsmagick;

  includes = stdenv.mkDerivation rec {
    name = "${pname}-includes";
    src = "${source}/include/";

    configHeader = with pkgs; writeText "config.h" ''
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
      ${if useJemalloc then "" else "#define IRCD_ALLOCATOR_USE_DEFAULT 1"}

      /* Use jemalloc as the allocator */
      ${if useJemalloc then "#define IRCD_ALLOCATOR_USE_JEMALLOC 1" else ""}

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
    dontStrip = true;
  };

  installPhase = let
    ircdUnitCXX = ccFile: loFile: extraArgs: "${pkgs.runCommandCC (lib.strings.sanitizeDerivationName loFile) (buildArgs buildInputs nativeBuildInputs) ''
      libtool --tag=CXX --mode=compile $CXX -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT                    ${extraArgs} \
        -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} \
        -c -o $out/${loFile} ${source}/ircd/${ccFile}
    ''}/${loFile}";
    ircdUnitCC  = asFile: loFile: extraArgs: "${pkgs.runCommandCC (lib.strings.sanitizeDerivationName loFile) (buildArgs buildInputs nativeBuildInputs) ''
      libtool --tag=CC  --mode=compile $CC               -DHAVE_CONFIG_H -DIRCD_UNIT                    ${extraArgs} \
        -I${includes}                          -DPCH -DNDEBUG ${WARNOPTS}                                              \
        -c -o $out/${loFile} ${source}/ircd/${asFile}
    ''}/${loFile}";
    ircdLD = loFiles: laFile: extraArgs: "${pkgs.runCommandCC (lib.strings.sanitizeDerivationName laFile) (buildArgs buildInputs nativeBuildInputs) ''
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=initial-exec -pthread ${CXXOPTS} -version-info 3:2:0 \
        -Wl,--no-undefined-version -Wl,--weak-unresolved-symbols -Wl,--unresolved-symbols=ignore-in-shared-libs \
        -Wl,--wrap=pthread_create -Wl,--wrap=pthread_join -Wl,--wrap=pthread_timedjoin_np -Wl,--wrap=pthread_self -Wl,--wrap=pthread_setname_np \
        -Wl,-z,nodelete -Wl,-z,nodlopen -Wl,-z,lazy -L${boost.out}/lib \
        -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment \
        -o $out/${laFile} ${lib.concatStringsSep " " loFiles} ${extraArgs} \
        -lrocksdb -lboost_coroutine -lboost_context -lboost_thread -lboost_filesystem -lboost_chrono -lboost_system -lssl -lcrypto -L${pkgs.libsodium.out}/lib -lsodium -lmagic -lz -lpthread -latomic -lrocksdb -ldl
    ''}/${laFile}";
    matrixUnitCXX = ccFile: loFile: extraArgs: "${pkgs.runCommandCC (lib.strings.sanitizeDerivationName loFile) (buildArgs buildInputs nativeBuildInputs) ''
      libtool --tag=CXX --mode=compile $CXX -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT ${extraArgs} \
        -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=local-dynamic -pthread ${CXXOPTS} \
        -c -o $out/${loFile} ${source}/matrix/${ccFile}
    ''}/${loFile}";
    matrixLD = loFiles: laFile: extraArgs: "${pkgs.runCommandCC (lib.strings.sanitizeDerivationName laFile) (buildArgs buildInputs nativeBuildInputs) ''
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -pthread -ftls-model=local-dynamic ${CXXOPTS} -version-info 0:1:0 \
        -Wl,--no-undefined-version -Wl,--allow-shlib-undefined -Wl,--unresolved-symbols=ignore-in-shared-libs -Wl,-z,lazy -L${dirOf ircd}/ \
        -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment \
        -o $out/${laFile} ${lib.concatStringsSep " " loFiles} ${extraArgs} -lrocksdb -ldl ${if useJemalloc then "-ljemalloc" else ""}
    ''}/${laFile}";
    moduleUnitCXX = subdir: ccFile: loFile: extraArgs: "${pkgs.runCommandCC (lib.strings.sanitizeDerivationName loFile) (buildArgs buildInputs nativeBuildInputs) ''
      libtool --tag=CXX --mode=compile $CXX -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE ${extraArgs} \
        -I${includes} -include ${includes}/ircd/matrix.pic.h -include ${includes}/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} \
        -I ${source}/modules/${lib.concatStringsSep "/" subdir} -c -o $out/${loFile} ${source}/modules/${lib.concatStringsSep "/" subdir}/${ccFile}
    ''}/${loFile}";
    moduleLD = loFiles: laFile: extraArgs: "${pkgs.runCommandCC (lib.strings.sanitizeDerivationName laFile) (buildArgs buildInputs nativeBuildInputs) ''
      libtool --tag=CXX --mode=link    $CXX -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version \
        -Wl,--allow-shlib-undefined -Wl,-z,lazy -L${dirOf ircd}/ -L${dirOf matrix}/ \
        -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment \
        -o $out/${laFile} ${lib.concatStringsSep " " loFiles} -lrocksdb -ldl ${extraArgs}
    ''}/${laFile}";
    constructUnitCXX = ccFile: obFile: extraArgs: "${pkgs.runCommandCC (lib.strings.sanitizeDerivationName obFile) (buildArgs buildInputs nativeBuildInputs) ''
      mkdir -p $out
      $CXX -std=gnu++17 -DHAVE_CONFIG_H -I${includes} -I${pkgs.boost.dev}/include -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} \
        -c -o $out/${obFile} ${source}/construct/${ccFile} ${extraArgs}
    ''}/${obFile}";
    constructLD = obFiles: exFile: "${pkgs.runCommandCC (lib.strings.sanitizeDerivationName exFile) (buildArgs buildInputs nativeBuildInputs) ''
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=initial-exec -pthread ${CXXOPTS} -dlopen self \
        -Wl,--warn-execstack -Wl,--warn-common -Wl,--detect-odr-violations -Wl,--unresolved-symbols=report-all -Wl,--allow-shlib-undefined -Wl,--dynamic-list-data -Wl,--dynamic-list-cpp-new -Wl,--dynamic-list-cpp-typeinfo -Wl,--rosegment -Wl,-z,noexecstack \
        -L${dirOf ircd}/ ${lib.concatMapStringsSep " " (mod: "-L${dirOf mod}/") modules} -L${pkgs.boost.out}/lib \
        -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment \
        -o $out/bin/${exFile} ${lib.concatStringsSep " " obFiles} -lircd -lboost_coroutine -lboost_context -lboost_thread -lboost_filesystem -lboost_chrono -lboost_system -lssl -lcrypto -lpthread -latomic -lrocksdb -ldl
    ''}/bin/${exFile}";

    versionDefs = let
      versions = {
        BRANDING_VERSION = "${runWithGit "version" VERSION_CMD}";
        RB_VERSION = "${runWithGit "version" VERSION_CMD}";
        RB_VERSION_BRANCH = "${runWithGit "version-branch" VERSION_BRANCH_CMD}";
        RB_VERSION_COMMIT = "${runWithGit "version-commit" VERSION_COMMIT_CMD}";
        RB_VERSION_TAG = "${runWithGit "version-tag" VERSION_TAG_CMD}";
      };
    in lib.concatStringsSep " " (lib.mapAttrsToList (k: v: "-U${k} -D'${k}=\"${v}\"'") versions);

    ircd = with pkgs; ircdLD [
      "${ircdUnitCXX "assert.cc"        "assert.lo"        ""}"
      "${ircdUnitCXX "info.cc"          "info.lo"          "${versionDefs}"}"
      "${ircdUnitCXX "allocator.cc"     "allocator.lo"     ""}"
      "${ircdUnitCXX "allocator_gnu.cc" "allocator_gnu.lo" ""}"
      "${ircdUnitCXX "allocator_je.cc"  "allocator_je.lo"  ""}"
      "${ircdUnitCXX "vg.cc"            "vg.lo"            ""}"
      "${ircdUnitCXX "exception.cc"        "exception.lo"        "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "util.cc"          "util.lo"          ""}"
      "${ircdUnitCXX "demangle.cc"      "demangle.lo"      ""}"
      "${ircdUnitCXX "backtrace.cc"     "backtrace.lo"     ""}"
      "${ircdUnitCXX "locale.cc"   "locale.lo"   "-I${boost.dev}/include"}"
      "${ircdUnitCXX "timedate.cc"      "timedate.lo"      ""}"
      "${ircdUnitCXX "lex_cast.cc" "lex_cast.lo" "-I${boost.dev}/include"}"
      "${ircdUnitCXX "stringops.cc"     "stringops.lo"     ""}"
      "${ircdUnitCXX "globular.cc"      "globular.lo"      ""}"
      "${ircdUnitCXX "tokens.cc"   "tokens.lo"   "-I${boost.dev}/include"}"
      "${ircdUnitCXX "parse.cc"   "parse.lo"   "-I${boost.dev}/include -include ircd/spirit.h -fno-var-tracking -fno-var-tracking-assignments -femit-struct-debug-baseonly"}"
      "${ircdUnitCXX "rand.cc"          "rand.lo"          ""}"
      "${ircdUnitCXX "base.cc"     "base.lo"     "-I${boost.dev}/include"}"
      "${ircdUnitCXX "crh.cc"           "crh.lo"           ""}"
      "${ircdUnitCXX "fmt.cc"     "fmt.lo"     "-I${boost.dev}/include -include ircd/spirit.h -fno-var-tracking -fno-var-tracking-assignments -femit-struct-debug-baseonly"}"
      "${ircdUnitCXX "json.cc"    "json.lo"    "-I${boost.dev}/include -include ircd/spirit.h -fno-var-tracking -fno-var-tracking-assignments -femit-struct-debug-baseonly"}"
      "${ircdUnitCXX "cbor.cc"          "cbor.lo"          ""}"
      "${ircdUnitCXX "conf.cc"          "conf.lo"          ""}"
      "${ircdUnitCXX "stats.cc"         "stats.lo"         ""}"
      "${ircdUnitCXX "logger.cc"        "logger.lo"        ""}"
      "${ircdUnitCXX "run.cc"           "run.lo"           ""}"
      "${ircdUnitCXX "magic.cc"         "magic.lo"         ""}"
      "${ircdUnitCXX "sodium.cc"        "sodium.lo"        ""}"
       # pbc.cc would go here
      "${ircdUnitCXX "openssl.cc"       "openssl.lo"       ""}"
      "${ircdUnitCXX "rfc1459.cc" "rfc1459.lo" "-I${boost.dev}/include -include ircd/spirit.h -fno-var-tracking -fno-var-tracking-assignments -femit-struct-debug-baseonly"}"
      "${ircdUnitCXX "rfc3986.cc" "rfc3986.lo" "-I${boost.dev}/include -include ircd/spirit.h -fno-var-tracking -fno-var-tracking-assignments -femit-struct-debug-baseonly"}"
      "${ircdUnitCXX "rfc1035.cc"       "rfc1035.lo"       ""}"
      "${ircdUnitCXX "http.cc"    "http.lo"    "-I${boost.dev}/include -include ircd/spirit.h -fno-var-tracking -fno-var-tracking-assignments -femit-struct-debug-baseonly"}"
      "${ircdUnitCXX "http2.cc"         "http2.lo"         ""}"
      "${ircdUnitCXX "prof.cc"     "prof.lo"     "-I${boost.dev}/include"}"
      "${ircdUnitCXX "prof_linux.cc"    "prof_linux.lo"    ""}"
      "${ircdUnitCXX "fs.cc"               "fs.lo"               "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "fs_path.cc"          "fs_path.lo"          "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "ios.cc"              "ios.lo"              "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCC  "ctx_x86_64.S" "ctx_x86_64.lo " "-pipe"}"
      "${ircdUnitCXX "ctx.cc"              "ctx.lo"              "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "ctx_eh.cc"           "ctx_eh.lo"           "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "ctx_ole.cc"          "ctx_ole.lo"          "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "ctx_posix.cc"        "ctx_posix.lo"        "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "fs_aio.cc"           "fs_aio.lo"           "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "fs_iou.cc"           "fs_iou.lo"           "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "mods.cc"             "mods.lo"             "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "mods_ldso.cc"     "mods_ldso.lo"     ""}"
      "${ircdUnitCXX "db_port.cc"       "db_port.lo"       ""}"
      "${ircdUnitCXX "db_fixes.cc"        "db_fixes.lo"        "-I${rocksdb.src} -I${rocksdb.out}/include"}"
      "${ircdUnitCXX "db_env.cc"        "db_env.lo"        ""}"
      "${ircdUnitCXX "db.cc"            "db.lo"            ""}"
      "${ircdUnitCXX "net.cc"              "net.lo"              "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "net_addrs.cc"        "net_addrs.lo"        "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "net_dns.cc"          "net_dns.lo"          "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "net_dns_netdb.cc" "net_dns_netdb.lo" ""}"
      "${ircdUnitCXX "net_dns_cache.cc" "net_dns_cache.lo" ""}"
      "${ircdUnitCXX "net_dns_resolver.cc" "net_dns_resolver.lo" "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "net_listener.cc"     "net_listener.lo"     "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "net_listener_udp.cc" "net_listener_udp.lo" "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "server.cc"           "server.lo"           "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "client.cc"           "client.lo"           "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "resource.cc"      "resource.lo"      ""}"
      "${ircdUnitCXX "ircd.cc"          "ircd.lo"          "${versionDefs}"}"
    ] "libircd.la" "-rpath $out/lib";

    matrix = with pkgs; matrixLD [
      "${matrixUnitCXX "name.cc"                   "name.lo"                   ""}"
      "${matrixUnitCXX "id.cc" "id.lo" "-I${boost.dev}/include -include ircd/spirit.h"}"
      "${matrixUnitCXX "dbs.cc"                    "dbs.lo"                    ""}"
      "${matrixUnitCXX "dbs_event_idx.cc"          "dbs_event_idx.lo"          ""}"
      "${matrixUnitCXX "dbs_event_json.cc"         "dbs_event_json.lo"         ""}"
      "${matrixUnitCXX "dbs_event_column.cc"       "dbs_event_column.lo"       ""}"
      "${matrixUnitCXX "dbs_event_refs.cc"         "dbs_event_refs.lo"         ""}"
      "${matrixUnitCXX "dbs_event_horizon.cc"      "dbs_event_horizon.lo"      ""}"
      "${matrixUnitCXX "dbs_event_sender.cc"       "dbs_event_sender.lo"       ""}"
      "${matrixUnitCXX "dbs_event_type.cc"         "dbs_event_type.lo"         ""}"
      "${matrixUnitCXX "dbs_event_state.cc"        "dbs_event_state.lo"        ""}"
      "${matrixUnitCXX "dbs_room_events.cc"        "dbs_room_events.lo"        ""}"
      "${matrixUnitCXX "dbs_room_type.cc"          "dbs_room_type.lo"          ""}"
      "${matrixUnitCXX "dbs_room_state.cc"         "dbs_room_state.lo"         ""}"
      "${matrixUnitCXX "dbs_room_state_space.cc"   "dbs_room_state_space.lo"   ""}"
      "${matrixUnitCXX "dbs_room_joined.cc"        "dbs_room_joined.lo"        ""}"
      "${matrixUnitCXX "dbs_room_head.cc"          "dbs_room_head.lo"          ""}"
      "${matrixUnitCXX "dbs_desc.cc"               "dbs_desc.lo"               ""}"
      "${matrixUnitCXX "hook.cc"                   "hook.lo"                   ""}"
      "${matrixUnitCXX "event.cc"                  "event.lo"                  ""}"
      "${matrixUnitCXX "event_cached.cc"           "event_cached.lo"           ""}"
      "${matrixUnitCXX "event_conforms.cc"         "event_conforms.lo"         ""}"
      "${matrixUnitCXX "event_fetch.cc"            "event_fetch.lo"            ""}"
      "${matrixUnitCXX "event_get.cc"              "event_get.lo"              ""}"
      "${matrixUnitCXX "event_id.cc"               "event_id.lo"               ""}"
      "${matrixUnitCXX "event_index.cc"            "event_index.lo"            ""}"
      "${matrixUnitCXX "event_prefetch.cc"         "event_prefetch.lo"         ""}"
      "${matrixUnitCXX "event_prev.cc"             "event_prev.lo"             ""}"
      "${matrixUnitCXX "event_refs.cc"             "event_refs.lo"             ""}"
      "${matrixUnitCXX "room.cc"                   "room.lo"                   ""}"
      "${matrixUnitCXX "room_auth.cc"              "room_auth.lo"              ""}"
      "${matrixUnitCXX "room_aliases.cc"           "room_aliases.lo"           ""}"
      "${matrixUnitCXX "room_bootstrap.cc"         "room_bootstrap.lo"         ""}"
      "${matrixUnitCXX "room_create.cc"            "room_create.lo"            ""}"
      "${matrixUnitCXX "room_events.cc"            "room_events.lo"            ""}"
      "${matrixUnitCXX "room_head.cc"              "room_head.lo"              ""}"
      "${matrixUnitCXX "room_join.cc"              "room_join.lo"              ""}"
      "${matrixUnitCXX "room_leave.cc"             "room_leave.lo"             ""}"
      "${matrixUnitCXX "room_visible.cc"           "room_visible.lo"           ""}"
      "${matrixUnitCXX "room_members.cc"           "room_members.lo"           ""}"
      "${matrixUnitCXX "room_origins.cc"           "room_origins.lo"           ""}"
      "${matrixUnitCXX "room_type.cc"              "room_type.lo"              ""}"
      "${matrixUnitCXX "room_power.cc"             "room_power.lo"             ""}"
      "${matrixUnitCXX "room_state.cc"             "room_state.lo"             ""}"
      "${matrixUnitCXX "room_state_history.cc"     "room_state_history.lo"     ""}"
      "${matrixUnitCXX "room_state_space.cc"       "room_state_space.lo"       ""}"
      "${matrixUnitCXX "room_server_acl.cc"        "room_server_acl.lo"        ""}"
      "${matrixUnitCXX "room_stats.cc"             "room_stats.lo"             ""}"
      "${matrixUnitCXX "user.cc"                   "user.lo"                   ""}"
      "${matrixUnitCXX "user_account_data.cc"      "user_account_data.lo"      ""}"
      "${matrixUnitCXX "user_devices.cc"           "user_devices.lo"           ""}"
      "${matrixUnitCXX "user_events.cc"            "user_events.lo"            ""}"
      "${matrixUnitCXX "user_filter.cc"            "user_filter.lo"            ""}"
      "${matrixUnitCXX "user_ignores.cc"           "user_ignores.lo"           ""}"
      "${matrixUnitCXX "user_mitsein.cc"           "user_mitsein.lo"           ""}"
      "${matrixUnitCXX "user_notifications.cc"     "user_notifications.lo"     ""}"
      "${matrixUnitCXX "user_profile.cc"           "user_profile.lo"           ""}"
      "${matrixUnitCXX "user_pushers.cc"           "user_pushers.lo"           ""}"
      "${matrixUnitCXX "user_pushrules.cc"         "user_pushrules.lo"         ""}"
      "${matrixUnitCXX "user_register.cc"          "user_register.lo"          ""}"
      "${matrixUnitCXX "user_room_account_data.cc" "user_room_account_data.lo" ""}"
      "${matrixUnitCXX "user_room_tags.cc"         "user_room_tags.lo"         ""}"
      "${matrixUnitCXX "user_rooms.cc"             "user_rooms.lo"             ""}"
      "${matrixUnitCXX "user_tokens.cc"            "user_tokens.lo"            ""}"
      "${matrixUnitCXX "acquire.cc"                "acquire.lo"                ""}"
      "${matrixUnitCXX "bridge.cc"                 "bridge.lo"                 ""}"
      "${matrixUnitCXX "breadcrumb_rooms.cc"       "breadcrumb_rooms.lo"       ""}"
      "${matrixUnitCXX "burst.cc"                  "burst.lo"                  ""}"
      "${matrixUnitCXX "display_name.cc"           "display_name.lo"           ""}"
      "${matrixUnitCXX "event_append.cc"           "event_append.lo"           ""}"
      "${matrixUnitCXX "event_horizon.cc"          "event_horizon.lo"          ""}"
      "${matrixUnitCXX "events.cc"                 "events.lo"                 ""}"
      "${matrixUnitCXX "fed.cc"                    "fed.lo"                    ""}"
      "${matrixUnitCXX "feds.cc"                   "feds.lo"                   ""}"
      "${matrixUnitCXX "fetch.cc"                  "fetch.lo"                  ""}"
      "${matrixUnitCXX "gossip.cc"                 "gossip.lo"                 ""}"
      "${matrixUnitCXX "request.cc"                "request.lo"                ""}"
      "${matrixUnitCXX "keys.cc"                   "keys.lo"                   ""}"
      "${matrixUnitCXX "node.cc"                   "node.lo"                   ""}"
      "${matrixUnitCXX "presence.cc"               "presence.lo"               ""}"
      "${matrixUnitCXX "pretty.cc"                 "pretty.lo"                 ""}"
      "${matrixUnitCXX "receipt.cc"                "receipt.lo"                ""}"
      "${matrixUnitCXX "rooms.cc"                  "rooms.lo"                  ""}"
      "${matrixUnitCXX "membership.cc"             "membership.lo"             ""}"
      "${matrixUnitCXX "rooms_summary.cc"          "rooms_summary.lo"          ""}"
      "${matrixUnitCXX "sync.cc"                   "sync.lo"                   ""}"
      "${matrixUnitCXX "typing.cc"                 "typing.lo"                 ""}"
      "${matrixUnitCXX "users.cc"                  "users.lo"                  ""}"
      "${matrixUnitCXX "users_servers.cc"          "users_servers.lo"          ""}"
      "${matrixUnitCXX "error.cc"                  "error.lo"                  ""}"
      "${matrixUnitCXX "push.cc"                   "push.lo"                   ""}"
      "${matrixUnitCXX "filter.cc"                 "filter.lo"                 ""}"
      "${matrixUnitCXX "txn.cc"                    "txn.lo"                    ""}"
      "${matrixUnitCXX "vm.cc"                     "vm.lo"                     ""}"
      "${matrixUnitCXX "vm_eval.cc"                "vm_eval.lo"                ""}"
      "${matrixUnitCXX "vm_inject.cc"              "vm_inject.lo"              ""}"
      "${matrixUnitCXX "vm_execute.cc"             "vm_execute.lo"             ""}"
      "${matrixUnitCXX "init_backfill.cc"          "init_backfill.lo"          ""}"
      "${matrixUnitCXX "homeserver.cc"             "homeserver.lo"             ""}"
      "${matrixUnitCXX "resource.cc"               "resource.lo"               ""}"
      "${matrixUnitCXX "matrix.cc"                 "matrix.lo"                 ""}"
    ] "libircd_matrix.la" "-rpath $out/lib";

    modules = with pkgs; [
      (moduleLD [
        "${moduleUnitCXX [] "m_breadcrumb_rooms.cc"                     "m_breadcrumb_rooms.lo"                     ""}"
      ] "m_breadcrumb_rooms.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_bridge.cc"                               "m_bridge.lo"                               ""}"
      ] "m_bridge.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_command.cc"                              "m_command.lo"                              ""}"
      ] "m_command.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_control.cc"                              "m_control.lo"                              ""}"
      ] "m_control.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_device.cc"                               "m_device.lo"                               ""}"
      ] "m_device.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_device_list_update.cc"                   "m_device_list_update.lo"                   ""}"
      ] "m_device_list_update.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_direct.cc"                               "m_direct.lo"                               ""}"
      ] "m_direct.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_direct_to_device.cc"                     "m_direct_to_device.lo"                     ""}"
      ] "m_direct_to_device.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_ignored_user_list.cc"                    "m_ignored_user_list.lo"                    ""}"
      ] "m_ignored_user_list.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_listen.cc"                               "m_listen.lo"                               ""}"
      ] "m_listen.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_noop.cc"                                 "m_noop.lo"                                 ""}"
      ] "m_noop.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_presence.cc"                             "m_presence.lo"                             ""}"
      ] "m_presence.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_profile.cc"                              "m_profile.lo"                              ""}"
      ] "m_profile.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_push.cc"                                 "m_push.lo"                                 ""}"
      ] "m_push.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_receipt.cc"                              "m_receipt.lo"                              ""}"
      ] "m_receipt.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_relation.cc"                             "m_relation.lo"                             ""}"
      ] "m_relation.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_room_aliases.cc"                         "m_room_aliases.lo"                         ""}"
      ] "m_room_aliases.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_room_canonical_alias.cc"                 "m_room_canonical_alias.lo"                 ""}"
      ] "m_room_canonical_alias.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_room_create.cc"                          "m_room_create.lo"                          ""}"
      ] "m_room_create.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_room_history_visibility.cc"              "m_room_history_visibility.lo"              ""}"
      ] "m_room_history_visibility.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_room_join_rules.cc"                      "m_room_join_rules.lo"                      ""}"
      ] "m_room_join_rules.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_room_member.cc"                          "m_room_member.lo"                          ""}"
      ] "m_room_member.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_room_message.cc"                         "m_room_message.lo"                         ""}"
      ] "m_room_message.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_room_name.cc"                            "m_room_name.lo"                            ""}"
      ] "m_room_name.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_room_power_levels.cc"                    "m_room_power_levels.lo"                    ""}"
      ] "m_room_power_levels.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_room_redaction.cc"                       "m_room_redaction.lo"                       ""}"
      ] "m_room_redaction.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_room_server_acl.cc"                      "m_room_server_acl.lo"                      ""}"
      ] "m_room_server_acl.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_room_third_party_invite.cc"              "m_room_third_party_invite.lo"              ""}"
      ] "m_room_third_party_invite.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_vm_fetch.cc"                             "m_vm_fetch.lo"                             ""}"
      ] "m_vm_fetch.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "net_dns_cache.cc"                          "net_dns_cache.lo"                          ""}"
      ] "net_dns_cache.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "stats.cc"                                  "stats.lo"                                  ""}"
      ] "stats.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "console.cc"                                "console.lo"                                "${versionDefs}"}"
      ] "console.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "web_root.cc"                               "web_root.lo"                               ""}"
      ] "web_root.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "web_hook.cc"                               "web_hook.lo"                               ""}"
      ] "web_hook.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "well_known.cc"                             "well_known.lo"                             ""}"
      ] "well_known.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "admin" ] "users.cc"                         "users.lo"                              ""}"
      ] "admin_users.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "admin" ] "deactivate.cc"                    "deactivate.lo"                         ""}"
      ] "admin_deactivate.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "versions.cc"                        "versions.lo"                        ""}"
      ] "client_versions.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "events.cc"                          "events.lo"                          ""}"
      ] "client_events.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "login.cc"                           "login.lo"                           ""}"
      ] "client_login.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "logout.cc"                          "logout.lo"                          ""}"
      ] "client_logout.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "sync.cc"                            "sync.lo"                            ""}"
      ] "client_sync.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "presence.cc"                        "presence.lo"                        ""}"
      ] "client_presence.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "profile.cc"                         "profile.lo"                         ""}"
      ] "client_profile.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "devices.cc"                         "devices.lo"                         ""}"
      ] "client_devices.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "pushers.cc"                         "pushers.lo"                         ""}"
      ] "client_pushers.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "publicrooms.cc"                     "publicrooms.lo"                     ""}"
      ] "client_publicrooms.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "createroom.cc"                      "createroom.lo"                      ""}"
      ] "client_createroom.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "pushrules.cc"                       "pushrules.lo"                       ""}"
      ] "client_pushrules.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "join.cc"                            "join.lo"                            ""}"
      ] "client_join.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "publicised_groups.cc"               "publicised_groups.lo"               ""}"
      ] "client_publicised_groups.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "initialsync.cc"                     "initialsync.lo"                     ""}"
      ] "client_initialsync.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "search.cc"                          "search.lo"                          ""}"
      ] "client_search.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "joined_groups.cc"                   "joined_groups.lo"                   ""}"
      ] "client_joined_groups.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "register_available.cc"              "register_available.lo"              ""}"
      ] "client_register_available.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "capabilities.cc"                    "capabilities.lo"                    ""}"
      ] "client_capabilities.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "send_to_device.cc"                  "send_to_device.lo"                  ""}"
      ] "client_send_to_device.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "delete_devices.cc"                  "delete_devices.lo"                  ""}"
      ] "client_delete_devices.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "notifications.cc"                   "notifications.lo"                   ""}"
      ] "client_notifications.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "register_email.cc"                  "register_email.lo"                  ""}"
      ] "client_register_email.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "register.cc"                        "register.lo"                        ""}"
      ] "client_register.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "rooms" ] "messages.cc"                  "messages.lo"                   ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "state.cc"                     "state.lo"                      ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "members.cc"                   "members.lo"                    ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "context.cc"                   "context.lo"                    ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "event.cc"                     "event.lo"                      ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "send.cc"                      "send.lo"                       ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "typing.cc"                    "typing.lo"                     ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "redact.cc"                    "redact.lo"                     ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "receipt.cc"                   "receipt.lo"                    ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "join.cc"                      "join.lo"                       ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "invite.cc"                    "invite.lo"                     ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "leave.cc"                     "leave.lo"                      ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "forget.cc"                    "forget.lo"                     ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "kick.cc"                      "kick.lo"                       ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "ban.cc"                       "ban.lo"                        ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "unban.cc"                     "unban.lo"                      ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "read_markers.cc"              "read_markers.lo"               ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "initialsync.cc"               "initialsync.lo"                ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "report.cc"                    "report.lo"                     ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "relations.cc"                 "relations.lo"                  ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "upgrade.cc"                   "upgrade.lo"                    ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "rooms.cc"                     "rooms.lo"                      ""}"
      ] "client_rooms.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "user" ] "openid.cc"                     "openid.lo"                     ""}"
        "${moduleUnitCXX [ "client" "user" ] "filter.cc"                     "filter.lo"                     ""}"
        "${moduleUnitCXX [ "client" "user" ] "account_data.cc"               "account_data.lo"               ""}"
        "${moduleUnitCXX [ "client" "user" ] "rooms.cc"                      "rooms.lo"                      ""}"
        "${moduleUnitCXX [ "client" "user" ] "user.cc"                       "user.lo"                       ""}"
      ] "client_user.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "directory" ] "room.cc"                  "room.lo"                       ""}"
      ] "client_directory_room.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "directory" ] "user.cc"                  "user.lo"                       ""}"
      ] "client_directory_user.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "directory" "list" ] "room.cc"           "room.lo"                       ""}"
      ] "client_directory_list_room.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "directory" "list" ] "appservice.cc"     "appservice.lo"                 ""}"
      ] "client_directory_list_appservice.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "voip" ] "turnserver.cc"                 "turnserver.lo"                 ""}"
      ] "client_voip_turnserver.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "thirdparty" ] "protocols.cc"            "protocols.lo"                  ""}"
      ] "client_thirdparty_protocols.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "sync" ] "account_data.cc"               "account_data.lo"               ""}"
      ] "client_sync_account_data.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "sync" ] "presence.cc"                   "presence.lo"                   ""}"
      ] "client_sync_presence.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "sync" ] "rooms.cc"                      "rooms.lo"                      ""}"
      ] "client_sync_rooms.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "sync" ] "to_device.cc"                  "to_device.lo"                  ""}"
      ] "client_sync_to_device.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "sync" ] "device_lists.cc"               "device_lists.lo"               ""}"
      ] "client_sync_device_lists.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "sync" ] "device_one_time_keys_count.cc" "device_one_time_keys_count.lo" ""}"
      ] "client_sync_device_one_time_keys_count.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "sync" "rooms" ] "account_data.cc"         "account_data.lo"         ""}"
      ] "client_sync_rooms_account_data.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "sync" "rooms" ] "ephemeral.cc"            "ephemeral.lo"            ""}"
      ] "client_sync_rooms_ephemeral.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "sync" "rooms" ] "state.cc"                "state.lo"                ""}"
      ] "client_sync_rooms_state.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "sync" "rooms" ] "timeline.cc"             "timeline.lo"             ""}"
      ] "client_sync_rooms_timeline.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "sync" "rooms" ] "unread_notifications.cc" "unread_notifications.lo" ""}"
      ] "client_sync_rooms_unread_notifications.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "sync" "rooms" ] "summary.cc"              "summary.lo"              ""}"
      ] "client_sync_rooms_summary.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "sync" "rooms" ] "ephemeral/receipt.cc"    "ephemeral/receipt.lo"    ""}"
      ] "client_sync_rooms_ephemeral_receipt.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "sync" "rooms" ] "ephemeral/typing.cc"     "ephemeral/typing.lo"     ""}"
      ] "client_sync_rooms_ephemeral_typing.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "keys" ] "upload.cc"                     "upload.lo"                     ""}"
      ] "client_keys_upload.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "keys" ] "query.cc"                      "query.lo"                      ""}"
      ] "client_keys_query.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "keys" ] "claim.cc"                      "claim.lo"                      ""}"
      ] "client_keys_claim.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "keys" ] "changes.cc"                    "changes.lo"                    ""}"
      ] "client_keys_changes.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "keys" ] "signatures/upload.cc"          "signatures/upload.lo"          ""}"
      ] "client_keys_signatures_upload.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "keys" ] "device_signing/upload.cc"      "device_signing/upload.lo"      ""}"
      ] "client_keys_device_signing_upload.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "room_keys" ] "version.cc"               "version.lo"                    ""}"
      ] "client_room_keys_version.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "room_keys" ] "keys.cc"                  "keys.lo"                       ""}"
      ] "client_room_keys_keys.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "account" ] "3pid.cc"                    "3pid.lo"                       ""}"
        "${moduleUnitCXX [ "client" "account" ] "whoami.cc"                  "whoami.lo"                     ""}"
        "${moduleUnitCXX [ "client" "account" ] "password.cc"                "password.lo"                   ""}"
        "${moduleUnitCXX [ "client" "account" ] "deactivate.cc"              "deactivate.lo"                 ""}"
        "${moduleUnitCXX [ "client" "account" ] "account.cc"                 "account.lo"                    ""}"
      ] "client_account.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "send.cc"                        "send.lo"                        ""}"
      ] "federation_send.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "event.cc"                       "event.lo"                       ""}"
      ] "federation_event.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "get_missing_events.cc"          "get_missing_events.lo"          ""}"
      ] "federation_get_missing_events.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "get_groups_publicised.cc"       "get_groups_publicised.lo"       ""}"
      ] "federation_get_groups_publicised.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "version.cc"                     "version.lo"                     ""}"
      ] "federation_version.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "sender.cc"                      "sender.lo"                      ""}"
      ] "federation_sender.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "query.cc"                       "query.lo"                       ""}"
      ] "federation_query.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "invite.cc"                      "invite.lo"                      ""}"
      ] "federation_invite.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "invite2.cc"                     "invite2.lo"                     ""}"
      ] "federation_invite2.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "make_join.cc"                   "make_join.lo"                   ""}"
      ] "federation_make_join.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "send_join.cc"                   "send_join.lo"                   ""}"
      ] "federation_send_join.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "state_ids.cc"                   "state_ids.lo"                   ""}"
      ] "federation_state_ids.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "state.cc"                       "state.lo"                       ""}"
      ] "federation_state.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "make_leave.cc"                  "make_leave.lo"                  ""}"
      ] "federation_make_leave.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "send_leave.cc"                  "send_leave.lo"                  ""}"
      ] "federation_send_leave.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "backfill.cc"                    "backfill.lo"                    ""}"
      ] "federation_backfill.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "backfill_ids.cc"                "backfill_ids.lo"                ""}"
      ] "federation_backfill_ids.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "event_auth.cc"                  "event_auth.lo"                  ""}"
      ] "federation_event_auth.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "query_auth.cc"                  "query_auth.lo"                  ""}"
      ] "federation_query_auth.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "publicrooms.cc"                 "publicrooms.lo"                 ""}"
      ] "federation_publicrooms.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "user_devices.cc"                "user_devices.lo"                ""}"
      ] "federation_user_devices.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "user_keys_query.cc"             "user_keys_query.lo"             ""}"
      ] "federation_user_keys_query.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "user_keys_claim.cc"             "user_keys_claim.lo"             ""}"
      ] "federation_user_keys_claim.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "rooms.cc"                       "rooms.lo"                       ""}"
      ] "federation_rooms.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "identity" ] "v1.cc"                            "v1.lo"                          ""}"
      ] "identity_v1.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "identity" ] "pubkey.cc"                        "pubkey.lo"                      ""}"
      ] "identity_pubkey.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "key" ] "server.cc"                             "server.lo"                      ""}"
      ] "key_server.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "key" ] "query.cc"                              "query.lo"                       ""}"
      ] "key_query.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "media" ] "download.cc"                         "download.lo"                    ""}"
        "${moduleUnitCXX [ "media" ] "upload.cc"                           "upload.lo"                      ""}"
        "${moduleUnitCXX [ "media" ] "thumbnail.cc"                        "thumbnail.lo"                   ""}"
        "${moduleUnitCXX [ "media" ] "preview_url.cc"                      "preview_url.lo"                 ""}"
        "${moduleUnitCXX [ "media" ] "config.cc"                           "config.lo"                      ""}"
        "${moduleUnitCXX [ "media" ] "media.cc"                            "media.lo"                       ""}"
      ] "media_media.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "magick.cc" "magick_la-magick.lo" "-I${graphicsmagick.out}/include/GraphicsMagick"}"
      ] "magick.la" "-rpath $out/lib/modules -lGraphicsMagick++ -lGraphicsMagickWand -lGraphicsMagick")
    ];

    construct = with pkgs; constructLD [
      "${constructUnitCXX "construct.cc" "construct.o" "${versionDefs}"}"
      "${constructUnitCXX "lgetopt.cc"   "lgetopt.o" ""}"
      "${constructUnitCXX "console.cc"   "console.o" ""}"
      "${constructUnitCXX "signals.cc"   "signals.o" ""}"
    ] "construct";
  in with pkgs; ''
    mkdir -p $out
    ln -s ${includes} $out/include

    mkdir -p $out/share
    ln -s ${source}/share/webapp $out/share/webapp

    mkdir -p $out/lib
    libtool --mode=install ${coreutils.out}/bin/install -c ${ircd} $out/lib
    libtool --mode=install ${coreutils.out}/bin/install -c ${matrix} $out/lib

    mkdir -p $out/lib/modules
    libtool --mode=install ${coreutils.out}/bin/install -c ${lib.concatStringsSep " " modules} $out/lib/modules

    mkdir -p $out/bin
    libtool --mode=install ${coreutils.out}/bin/install -c ${construct} $out/bin

    wrapProgram $out/bin/construct \
      --set-default ircd_web_root_path ${riot-web.out} \
      --set-default ircd_fs_base_prefix $out \
      --set-default ircd_fs_base_bin $out/bin \
      --set-default ircd_fs_base_lib $out/lib \
      --set-default ircd_fs_base_modules $out/lib/modules \
      --argv0 $out/bin/construct

    makeWrapper ${gdb}/bin/gdb $out/bin/construct-gdb \
      --set-default ircd_web_root_path $out/share/webapp \
      --set-default ircd_fs_base_prefix $out \
      --set-default ircd_fs_base_bin $out/bin \
      --set-default ircd_fs_base_lib $out/lib \
      --set-default ircd_fs_base_modules $out/lib/modules \
      --add-flags "$out/bin/.construct-wrapped"
  '';

  doInstallCheck = true;
  installCheckPhase = ''
    chmod -R a-w $out
    mkdir -p /tmp/cache/construct
    export RUNTIME_DIRECTORY=/tmp/run/construct
    export STATE_DIRECTORY=/tmp/lib/construct
    export LOGS_DIRECTORY=/tmp/log/construct
    cd /tmp/cache/construct
    $out/bin/construct -smoketest localhost
  '';

  #outputs = [ "bin" "dev" "out" "doc" ];
  #separateDebugInfo = true;
  enableParallelBuilding = true;
}
