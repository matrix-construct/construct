AC_PREREQ([2.63])
AUTOMAKE_OPTIONS = 1.11
AC_INIT([construct], [1])

AC_GNU_SOURCE
AC_USE_SYSTEM_EXTENSIONS

AC_CANONICAL_HOST
AC_CANONICAL_BUILD
AC_CANONICAL_TARGET

AC_PREFIX_DEFAULT([/])
AC_CONFIG_MACRO_DIR([tools/m4])
AC_CONFIG_HEADER(include/ircd/config.h)

AC_PATH_PROG(AUTOMAKE, automake)
AC_PATH_PROG(ACLOCAL, aclocal)
AC_PATH_PROG(AUTOHEADER, autoheader)
AC_PATH_PROG(AS, as)
AC_PATH_PROG(RM, rm)
AC_PATH_PROG(CP, cp)
AC_PATH_PROG(MV, mv)
AC_PATH_PROG(LN, ln)
AC_PATH_PROG(LD, ld.gold)
AC_PATH_PROG(TOUCH, touch)

AC_SUBST(CFLAGS, "-pipe $CFLAGS")
AC_SUBST(CXXFLAGS, "-pipe $CXXFLAGS")

LT_INIT([dlopen shared disable-static])
LT_LANG([C++])

AM_PROG_AS
AC_PROG_CXX
AC_PROG_CXX_C_O
AC_PROG_LIBTOOL
AC_PROG_MAKE_SET

AC_LANG(C++)
AX_CXX_COMPILE_STDCXX([17], [gnu], [mandatory], 201703)


dnl ***************************************************************************
dnl
dnl Makefile tree
dnl
dnl These (and Makefile.in) are generated from a Makefile.am in the same dir.
dnl

AC_CONFIG_FILES(\
	Makefile                \
	include/ircd/Makefile   \
	construct/Makefile      \
	ircd/Makefile           \
	matrix/Makefile         \
	modules/Makefile        \
	share/Makefile          \
)

AM_INIT_AUTOMAKE([subdir-objects])

dnl If this is not (un)set, paths are added such as -I../include/ircd to every
dnl build command and that isn't nice because it makes our headers conflict
dnl with standard library/path headers which we don't want or need.
AC_SUBST(DEFAULT_INCLUDES, [])

dnl
dnl Recursive local targets (clean-local is implied)
dnl

#AM_EXTRA_RECURSIVE_TARGETS([
#	mrproper
#])


dnl ***************************************************************************
dnl
dnl Platform
dnl

dnl
dnl Compiler brand
dnl

AM_CONDITIONAL([GCC], [[[[ $CXX = g\+\+* ]]]])
AM_CONDITIONAL([CLANG], [[[[ $CXX = clang* ]]]])
RB_DEFINE_UNQUOTED([CXX], ["$CXX"], [CXX Compiler])

dnl
dnl Compiler version
dnl

AX_CXX_VERSION
RB_DEFINE_UNQUOTED([CXX_VERSION], ["$CXX_VERSION"], [CXX Compiler Version])

AX_CXX_EPOCH
RB_DEFINE_UNQUOTED([CXX_EPOCH], [$CXX_EPOCH], [CXX Compiler Version (major nr)])

AM_CONDITIONAL([GCC8],     [[[[ $CXX = g\+\+* ]] && [[ $CXX_EPOCH -ge 8 ]]]])
AM_CONDITIONAL([GCC9],     [[[[ $CXX = g\+\+* ]] && [[ $CXX_EPOCH -ge 9 ]]]])
AM_CONDITIONAL([GCC10],    [[[[ $CXX = g\+\+* ]] && [[ $CXX_EPOCH -ge 10 ]]]])
AM_CONDITIONAL([GCC11],    [[[[ $CXX = g\+\+* ]] && [[ $CXX_EPOCH -ge 11 ]]]])
AM_CONDITIONAL([GCC12],    [[[[ $CXX = g\+\+* ]] && [[ $CXX_EPOCH -ge 12 ]]]])
AM_CONDITIONAL([CLANG8],   [[[[ $CXX = clang* ]] && [[ $CXX_EPOCH -ge 8 ]]]])
AM_CONDITIONAL([CLANG9],   [[[[ $CXX = clang* ]] && [[ $CXX_EPOCH -ge 9 ]]]])
AM_CONDITIONAL([CLANG10],  [[[[ $CXX = clang* ]] && [[ $CXX_EPOCH -ge 10 ]]]])
AM_CONDITIONAL([CLANG11],  [[[[ $CXX = clang* ]] && [[ $CXX_EPOCH -ge 11 ]]]])
AM_CONDITIONAL([CLANG12],  [[[[ $CXX = clang* ]] && [[ $CXX_EPOCH -ge 12 ]]]])

dnl
dnl Coarse operating system test macros
dnl

AM_CONDITIONAL([LINUX],      [[[[ $host_os = *linux* ]]]])
AM_CONDITIONAL([NETBSD],     [[[[ $host_os = *netbsd* ]]]])
AM_CONDITIONAL([FREEBSD],    [[[[ $host_os = *freebsd* ]]]])
AM_CONDITIONAL([MINGW],      [[[[ $host_os = *mingw* ]] || [[ $host_os = *msys* ]]]])
AM_CONDITIONAL([CYGWIN],     [[[[ $host_os = *cygwin* ]]]])
AM_CONDITIONAL([INTERIX],    [[[[ $host_os = *interix* ]]]])
AM_CONDITIONAL([SOLARIS],    [[[[ $host_os = *solaris* ]]]])

RB_DEFINE_UNQUOTED([OS], ["$host_os"], [Host operating system])

dnl
dnl Standard libraries
dnl

AC_SEARCH_LIBS([__dynamic_cast], [c++], [],
[
	AC_MSG_ERROR([Please install libstdc++ on this system.])
])


dnl ***************************************************************************
dnl
dnl Developer options
dnl

dnl
dnl Debugging mode
dnl

AC_MSG_CHECKING(whether to enable debugging)
AC_ARG_ENABLE(debug, RB_HELP_STRING([--enable-debug], [Enable debugging suite for development]),
[
	debug="yes"
	AC_MSG_RESULT([yes])
	AC_SUBST(DEBUG, 1)
	RB_DEFINE_UNQUOTED([DEBUG], [1], [Not configured for release when lit.])
	RB_DEFINE_UNQUOTED([DEBUG_LEVEL], [1], [Defined to 0 for release; or > 0 otherwise])
], [
	debug="no"
	AC_MSG_RESULT([no])
	RB_DEFINE_UNQUOTED([DEBUG_LEVEL], [0], [Defined to 0 for release])
])

AM_CONDITIONAL([DEBUG], [[[[ "$DEBUG" = "1" ]]]])

dnl
dnl Compactness
dnl

AC_MSG_CHECKING(Optimize for size; strip symbols; force no debugging)
AC_ARG_ENABLE(compact, RB_HELP_STRING([--enable-compact], [Optimize for size and compactness]),
[
	AC_MSG_RESULT([yes])
	AC_SUBST(COMPACT, 1)
	RB_DEFINE_UNQUOTED([COMPACT], [1], [Not configured for compactness when lit.])
], [
	AC_MSG_RESULT([no])
])

AM_CONDITIONAL([COMPACT], [[[[ "$COMPACT" = "1" ]]]])

dnl
dnl Explicit assert switch for still using assert() without --enable-debug
dnl

AC_MSG_CHECKING(whether to explicitly enable assertions)
AC_ARG_ENABLE(assert, RB_HELP_STRING([--enable-assert], [Enable assertions without --enable-debug]),
[
	AC_MSG_RESULT([yes])
	AC_SUBST(ASSERT, 1)
], [
	AM_COND_IF(DEBUG,
	[
		AC_MSG_RESULT([no, but assertions are enabled anyway])
		AC_SUBST(ASSERT, 1)
	], [
		AC_MSG_RESULT([no])
	])
])

AM_CONDITIONAL([ASSERT], [[[[ "$ASSERT" = "1" ]]]])

AM_COND_IF(ASSERT,
[
	assert="yes"
	assert_type="abort"
], [
	assert="no"
	CPPDEFINE([NDEBUG])
])

dnl
dnl Switch to control the action of assert()
dnl

AC_MSG_CHECKING(whether to change the behavior of assertions)
AC_ARG_WITH(assert, RB_HELP_STRING([--with-assert[[[=abort]]]], [Soften assertion behavior]),
[
	assert_type=$withval
	AC_MSG_RESULT([yes, "$assert_type"])
	AC_SUBST(ASSERT_TYPE, $assert_type)
], [
	AC_MSG_RESULT([no])
])

AM_COND_IF(ASSERT,
[
	if [[ ! -z "$assert_type" ]]; then

		if [[ "$assert_type" != "abort" ]]; then
			RB_DEFINE_UNQUOTED([ASSERT], ["$assert_type"], [Assertion behavior])
		fi

		if [[ "$assert_type" == "trap" ]]; then
			RB_DEFINE([ASSERT_INTRINSIC], [1], [Intrinsic assertion behavior])
		fi

	fi
])

dnl
dnl Explicit optimize switch for enabling optimization when using --enable-debug
dnl

AC_MSG_CHECKING(whether to explicitly enable optimized build)
AC_ARG_ENABLE(optimize, RB_HELP_STRING([--enable-optimize], [Enable optimization even with --enable-debug]),
[
	AC_MSG_RESULT([yes])
	AC_SUBST(OPTIMIZE, 1)
], [
	AM_COND_IF(DEBUG,
	[
		AC_MSG_RESULT([no])
	], [
		AC_MSG_RESULT([no, but optimized build is enabled anyway])
		AC_SUBST(OPTIMIZE, 1)
	])
])

AM_CONDITIONAL([OPTIMIZE], [[[[ "$OPTIMIZE" = "1" ]]]])

dnl
dnl Explicit link-time-optimization switch
dnl

AC_MSG_CHECKING(whether to enable link-time optimization)
AC_ARG_ENABLE(lto, RB_HELP_STRING([--disable-lto], [Enable link-time optimization]),
[
	lto=$enableval
], [
	lto="yes"
])

if test "$lto" = "yes"; then
	AM_COND_IF(OPTIMIZE,
	[
		AM_COND_IF(CLANG,
		[
			AS_CASE([$host_cpu],
			[aarch64],
			[
				lto="no"
				AC_MSG_RESULT([no, doesn't work on aarch64])
			], [
				lto="yes"
				AC_MSG_RESULT([yes])
				AC_SUBST(LTO, 1)
			])
		], [
			lto="no"
			AC_MSG_RESULT([no, only available with clang])
		])
	], [
		lto="no"
		AC_MSG_RESULT([no, optimizations disabled])
	])
fi

AM_CONDITIONAL([LTO], [[[[ "$LTO" = "1" ]]]])

dnl
dnl Explicit optimization level switch
dnl

AC_MSG_CHECKING(whether to explicitly set the optimization level)
AC_ARG_WITH(optimize-level, RB_HELP_STRING([--with-optimize-level[[[=3]]]], [Explicitly set the optimize level]),
[
	optimize_level=$withval
	AC_MSG_RESULT([yes, enabling optimization level -O$optimize_level])
	AC_SUBST(OPTMIZE_LEVEL, $optimize_level)
	RB_DEFINE_UNQUOTED([OPTIMIZE_LEVEL], [$optimize_level], [User configured optimization level])
], [
	AM_COND_IF(OPTIMIZE,
	[
		AM_COND_IF(COMPACT,
		[
			optimize_level="s"
			AC_MSG_RESULT([no, but --enable-optimize and --enable-compact means -O$optimize_level])
			AC_SUBST(OPTIMIZE_LEVEL, $optimize_level)
			RB_DEFINE_UNQUOTED([OPTIMIZE_LEVEL], [$optimize_level], [Implicitly set release mode optimization])
		], [
			optimize_level="3"
			AC_MSG_RESULT([no, but --enable-optimize has implied level -O$optimize_level])
			AC_SUBST(OPTIMIZE_LEVEL, $optimize_level)
			RB_DEFINE_UNQUOTED([OPTIMIZE_LEVEL], [$optimize_level], [Explicitly set release mode optimization])
		])
	], [
		AM_COND_IF(DEBUG,
		[
			optimize_level="g"
			AC_MSG_RESULT([no, but --enable-debug has implied level -O$optimize_level])
			AC_SUBST(OPTIMIZE_LEVEL, $optimize_level)
			RB_DEFINE_UNQUOTED([OPTIMIZE_LEVEL], ['$optimize_level'], [Debug mode optimization level])
		], [
			optimize_level="3"
			AC_MSG_RESULT([no, but release mode has implied level -O$optimize_level])
			AC_SUBST(OPTIMIZE_LEVEL, $optimize_level)
			RB_DEFINE_UNQUOTED([OPTIMIZE_LEVEL], [$optimize_level], [Release mode optimization])
		])
	])
])

dnl
dnl Explicit log level
dnl
dnl NOTE: Log levels here must be manually sync'ed with ircd/logger.h
dnl

AC_MSG_CHECKING(whether to explicitly set the logging level ceiling)
AC_ARG_WITH(log-level, RB_HELP_STRING([--with-log-level[[[=INFO]]]], [Explicitly set the log level ceiling]),
[
	AC_MSG_RESULT([yes, enabling log level $withval and below])

	level=4
	if [[ $withval = "DEBUG" ]]; then
		level=7
	elif [[ $withval = "DWARNING" ]]; then
		level=6
	elif [[ $withval = "DERROR" ]]; then
		level=5
	elif [[ $withval = "INFO" ]]; then
		level=4
	elif [[ $withval = "NOTICE" ]]; then
		level=3
	elif [[ $withval = "WARNING" ]]; then
		level=2
	elif [[ $withval = "ERROR" ]]; then
		level=1
	elif [[ $withval = "CRITICAL" ]]; then
		level=0
	elif [[ $withval = "-1" ]]; then
		level=-1
	fi

	AC_SUBST(LOG_LEVEL, $level)
	RB_DEFINE_UNQUOTED([LOG_LEVEL], [$level], [Log messages for user configured level and below.])
], [
	AM_COND_IF(DEBUG,
	[
		AC_MSG_RESULT([no, but debug mode enables all log levels through DEBUG])
		AC_SUBST(LOG_LEVEL, 7)
		RB_DEFINE_UNQUOTED([LOG_LEVEL], [7], [Log message level default for DEBUG mode])
	], [
		AC_MSG_RESULT([no])
		AC_SUBST(LOG_LEVEL, 4)
		RB_DEFINE_UNQUOTED([LOG_LEVEL], [4], [Log message level default for release mode])
	])
])

dnl
dnl Profiling mode
dnl

AC_MSG_CHECKING(whether to instrument build for runtime profile generation)
AC_ARG_ENABLE(profile, RB_HELP_STRING([--enable-profile], [Generate profile at runtime]),
[
	profiling="yes"
	AC_DEFINE(CHARYBDIS_PROFILE, 1, [Define this if you are profiling.])

	AM_COND_IF(GCC,
	[
		AC_MSG_RESULT([yes, adding -pg])
		RB_VAR_PREPEND([CXXFLAGS], ["-pg"])
	])

	AM_COND_IF(CLANG,
	[
		AC_MSG_RESULT([yes, adding profiling flags])

		#RB_VAR_PREPEND([CXXFLAGS], ["-fprofile-generate"])
		#RB_VAR_PREPEND([LDFLAGS], ["-Wl,-fprofile-generate"])

		dnl TODO: $enableval=instr?
		dnl RB_VAR_PREPEND([CXXFLAGS], ["-fprofile-instr-generate"])
		dnl RB_VAR_PREPEND([CXXFLAGS], ["-fcoverage-mapping"])
		dnl RB_VAR_PREPEND([LDFLAGS], ["-Wl,-fprofile-instr-generate"])
	])

], [
	AC_MSG_RESULT([no])
	profiling="no"
])

AC_MSG_CHECKING(whether to use a specific runtime profile)
AC_ARG_WITH(profile, RB_HELP_STRING([--with-profile[[[=$PWD]]]], [Path to profdata]),
[
	profiling_use=$withval
	AC_MSG_RESULT([yes, "$profiling_use"])
	AC_SUBST(PROFILE_USE, $profiling_use)

dnl	AM_COND_IF(CLANG,
dnl	[
dnl		RB_VAR_PREPEND([CXXFLAGS], [-fprofile-use="$profiling_use"])
dnl	])

], [
	AC_MSG_RESULT([no])
])

AM_CONDITIONAL([PROFILE], [[[[ "$profiling" = "yes" ]]]])

dnl Provide the correct target to the Makefiles
AM_COND_IF(CLANG9,  [ AC_PATH_PROG(LLVM_PROFDATA, llvm-profdata-9)  ])
AM_COND_IF(CLANG10, [ AC_PATH_PROG(LLVM_PROFDATA, llvm-profdata-10) ])
AM_COND_IF(CLANG11, [ AC_PATH_PROG(LLVM_PROFDATA, llvm-profdata-11) ])
AM_COND_IF(CLANG12, [ AC_PATH_PROG(LLVM_PROFDATA, llvm-profdata-12) ])

dnl
dnl Precompiled headers
dnl

AC_MSG_CHECKING(whether to enable precompiled headers)
AC_ARG_ENABLE(pch, RB_HELP_STRING([--disable-pch], [Disable precompiled header generation]),
[
	build_pch=$enableval
	AC_MSG_RESULT([$enableval])
], [
	build_pch="yes"
	CPPDEFINE([PCH])
	AC_MSG_RESULT([yes])
])

AM_CONDITIONAL([BUILD_PCH], [[[[ "$build_pch" = "yes" ]]]])

dnl
dnl Generic Mode compilation
dnl

AC_MSG_CHECKING(whether to enable generic mode or tune for this host)
AC_ARG_ENABLE(generic, RB_HELP_STRING([--enable-generic], [Emit more generic code for pkg binaries]),
[
	enable_generic="yes"
	AC_MSG_RESULT([yes])
	RB_DEFINE([GENERIC], [1], [Building binary tuned for generic architectures])
], [
	enable_generic="no"
	AC_MSG_RESULT([no])
])

AM_CONDITIONAL([GENERIC], [[[[ "$enable_generic" = "yes" ]]]])

dnl
dnl Untuned Mode compilation
dnl

AC_MSG_CHECKING(whether to enable fully untuned mode)
AC_ARG_ENABLE(untuned, RB_HELP_STRING([--enable-untuned], [Emit no special feature instructions]),
[
	enable_untuned="yes"
	AC_MSG_RESULT([yes])
	RB_DEFINE([UNTUNED], [1], [Building binary without extended-feature cpu instructions.])
], [
	enable_untuned="no"
	AC_MSG_RESULT([no])
])

AM_CONDITIONAL([UNTUNED], [[[[ "$enable_untuned" = "yes" ]]]])

dnl
dnl Disable third-party allocators
dnl

AC_MSG_CHECKING(whether to allow available third-party allocator libraries from being used)
AC_ARG_ENABLE(malloc-libs, RB_HELP_STRING([--disable-malloc-libs], [Disable third-party dynamic memory libraries (jemalloc/tcmalloc/etc)]),
[
	use_malloc_libs=$enableval
	AC_MSG_RESULT([$enableval])
], [
	use_malloc_libs="yes"
	AC_MSG_RESULT([yes])
])

AM_CONDITIONAL([MALLOC_LIBS], [[[[ "$use_malloc_libs" = "yes" ]]]])

dnl
dnl Low memory compilation mode
dnl

AC_ARG_ENABLE(lowmem-compile, RB_HELP_STRING([--enable-lowmem-compile], [Enable low-memory compilation options]),
[
	lowmem_compile="yes"
], [
	lowmem_compile="no"
])

AM_CONDITIONAL([LOWMEM_COMPILE], [[[[ $lowmem_compile = "yes" ]]]])


dnl ***************************************************************************
dnl
dnl Base Build Flags
dnl

dnl
dnl Reserved for user
dnl
dnl ex. `make EXTRA_CXXFLAGS="-Weverything" ircd` to append CXXFLAGS

dnl AC_SUBST(EXTRA_CPPFLAGS, "$EXTRA_CPPFLAGS")
dnl AC_SUBST(EXTRA_CFLAGS, "$EXTRA_CFLAGS")
dnl AC_SUBST(EXTRA_CXXFLAGS, "$EXTRA_CXXFLAGS")
dnl AC_SUBST(EXTRA_LDFLAGS, "$EXTRA_LDFLAGS")
dnl AC_SUBST(EXTRA_LIBS, "$EXTRA_LIBS")

dnl
dnl Gold linker
dnl

dnl Sorry there is no way to disable gold right now. You can try the
dnl binutils-gold package if your binutils package doesn't ship with it
dnl by default these days. Not sure if this is a problem with musl; please
dnl report any issues so we can weigh the need to change course here.
dnl
ld_gold="yes"

AM_CONDITIONAL([LD_GOLD], [[[[ $ld_gold = "yes" ]]]])

AM_COND_IF([LD_GOLD],
[
	AM_COND_IF([GCC],
	[
		RB_VAR_PREPEND([LDFLAGS], ["-Wl,-fuse-ld=gold"])
		RB_VAR_PREPEND([LDFLAGS], ["-Wl,--gdb-index"])
	])

	AM_COND_IF([CLANG],
	[
		RB_VAR_PREPEND([LDFLAGS], ["-fuse-ld=gold"])
		RB_VAR_PREPEND([LDFLAGS], ["-Wc,-fuse-ld=gold"])
	])

	RB_VAR_PREPEND([LDFLAGS], ["-Wl,--warn-common"])
	RB_VAR_PREPEND([LDFLAGS], ["-Wl,--warn-execstack"])
	RB_VAR_PREPEND([LDFLAGS], ["-Wl,--detect-odr-violations"])
	RB_VAR_PREPEND([LDFLAGS], ["-Wl,--rosegment"])
dnl	RB_VAR_PREPEND([LDFLAGS], ["-Wl,--icf=all"])
	RB_VAR_PREPEND([LDFLAGS], ["-Wl,-z,noexecstack"])
	RB_VAR_PREPEND([LDFLAGS], ["-Wl,-z,combreloc"])
	RB_VAR_PREPEND([LDFLAGS], ["-Wl,-z,text-unlikely-segment"])
], [
	AC_MSG_ERROR([Sorry, disabling the gold linker is not supported at this time.])
])

dnl Note compact mode options available in both GOLD and LLD
AM_COND_IF([COMPACT],
[
	RB_VAR_PREPEND([LDFLAGS], ["-Wl,--strip-all"])
	RB_VAR_PREPEND([LDFLAGS], ["-Wl,--gc-sections"])
])

dnl
dnl Optimization
dnl

RB_VAR_PREPEND([CXXFLAGS], ["-O$optimize_level"])

AM_COND_IF([OPTIMIZE],
[
	optimize="yes"

	AM_COND_IF([GCC],
	[
		RB_VAR_PREPEND([CXXFLAGS], ["-fgcse-sm"])
		RB_VAR_PREPEND([CXXFLAGS], ["-fgcse-las"])
		RB_VAR_PREPEND([CXXFLAGS], ["-fsched-stalled-insns=0"])
		RB_VAR_PREPEND([CXXFLAGS], ["-fsched-pressure"])
		RB_VAR_PREPEND([CXXFLAGS], ["-fsched-spec-load"])
dnl		RB_VAR_PREPEND([CXXFLAGS], ["-fsched2-use-superblocks"])
dnl		RB_VAR_PREPEND([CXXFLAGS], ["-fsched2-use-superblocks"])
		RB_VAR_PREPEND([CXXFLAGS], ["-fira-hoist-pressure"])
		RB_VAR_PREPEND([CXXFLAGS], ["-fbranch-target-load-optimize"])
		RB_VAR_PREPEND([CXXFLAGS], ["-frerun-loop-opt"])
		RB_VAR_PREPEND([CXXFLAGS], ["-fdevirtualize-at-ltrans"])
dnl		RB_VAR_PREPEND([CXXFLAGS], ["-fbtr-bb-exclusive"])
		RB_VAR_PREPEND([CXXFLAGS], ["-fipa-pta"])
		RB_VAR_PREPEND([CXXFLAGS], ["-fmodulo-sched"])
		RB_VAR_PREPEND([CXXFLAGS], ["-fmodulo-sched-allow-regmoves"])
		RB_VAR_PREPEND([CXXFLAGS], ["-ftracer"])
		RB_VAR_PREPEND([CXXFLAGS], ["-ftree-loop-im"])
		RB_VAR_PREPEND([CXXFLAGS], ["-ftree-switch-conversion"])
	])
], [
	optimize="no"

	dnl Optimization options we enable in non-optimized mode. These will be enabled
	dnl in non-optimized mode as well as optimized mode so they are as similar as
	dnl possible without impacting debuggability.
	RB_VAR_PREPEND([CXXFLAGS], ["-falign-functions"])
dnl	RB_VAR_PREPEND([CXXFLAGS], ["-fdata-sections"])
dnl	RB_VAR_PREPEND([CXXFLAGS], ["-ffunction-sections"])
	RB_VAR_PREPEND([CXXFLAGS], ["-ftree-vectorize"])

	AM_COND_IF([GCC],
	[
		RB_VAR_PREPEND([CXXFLAGS], ["-falign-labels"])
		RB_VAR_PREPEND([CXXFLAGS], ["-falign-loops"])
		RB_VAR_PREPEND([CXXFLAGS], ["-falign-jumps"])

		RB_VAR_PREPEND([CXXFLAGS], ["-ftree-vrp"])
		RB_VAR_PREPEND([CXXFLAGS], ["-ftree-dce"])
		RB_VAR_PREPEND([CXXFLAGS], ["-ftree-dse"])
		RB_VAR_PREPEND([CXXFLAGS], ["-ftree-loop-im"])
		RB_VAR_PREPEND([CXXFLAGS], ["-ftree-switch-conversion"])
	])
])

AM_COND_IF([CLANG],
[
	RB_VAR_PREPEND([CXXFLAGS], ["-fno-data-sections"])
	RB_VAR_PREPEND([CXXFLAGS], ["-fno-function-sections"])
])

dnl
dnl Compiler intrumentation
dnl

AM_COND_IF([DEBUG],
[
	AM_COND_IF([GCC],
	[
		RB_VAR_PREPEND([CXXFLAGS], ["-fmax-errors=2"])
	])

	AM_COND_IF([CLANG],
	[
		RB_VAR_PREPEND([CXXFLAGS], ["-ferror-limit=2"])
	])
])

AM_COND_IF([DEBUG],
[
	RB_VAR_PREPEND([CXXFLAGS], ["-g"])
	RB_VAR_PREPEND([CXXFLAGS], ["-ggdb"])
	AM_COND_IF([GCC],
	[
		RB_VAR_PREPEND([CXXFLAGS], ["-fvar-tracking"])
		RB_VAR_PREPEND([CXXFLAGS], ["-fvar-tracking-assignments"])
	])
], [
	AM_COND_IF([COMPACT],
	[

	], [
		dnl Note these flags are unreliable with LTO
		RB_VAR_PREPEND([CXXFLAGS], ["-g"])
		RB_VAR_PREPEND([CXXFLAGS], ["-ggdb"])
	])
])

dnl  Creates an ELF section in the output which contains a list of
dnl  the command line arguments used for compilation.
AM_COND_IF([GCC],
[
	RB_VAR_PREPEND([CXXFLAGS], ["-frecord-gcc-switches"])
])

AM_COND_IF([CLANG],
[
	RB_VAR_PREPEND([CXXFLAGS], ["-grecord-gcc-switches"])
])

AM_COND_IF([GCC],
[
	RB_VAR_PREPEND([CXXFLAGS], ["-fstack-protector-explicit"])

	dnl These flags should not be used on Intel-CET capable platforms
	dnl TODO: XXX
	AS_IF([test "$CXX_EPOCH" -ge "9"],
	[
		AM_COND_IF([GENERIC],
		[
			RB_VAR_PREPEND([CXXFLAGS], ["-fcf-protection=full"])
		], [
			RB_VAR_PREPEND([CXXFLAGS], ["-fcf-protection=none"])
			RB_VAR_PREPEND([CXXFLAGS], ["-mmanual-endbr"])
		])
	])
])

AM_COND_IF([GCC],
[
	RB_VAR_PREPEND([CXXFLAGS], ["-fvtable-verify=none"])
])

dnl Not useful without GCC support...
dnl AM_COND_IF([CLANG],
dnl [
dnl 	RB_VAR_PREPEND([CXXFLAGS], ["-fasm-blocks"])
dnl ])

AM_COND_IF([GCC],
[
	RB_VAR_PREPEND([CXXFLAGS], ["-fverbose-asm"])
])

dnl
dnl Misc
dnl

dnl Exception safety at instruction boundary rather than function boundary
dnl This option is needed to enable noexcept(false) i.e to throw from a
dnl destructor; without this noexcept(false) still terminates.
RB_VAR_PREPEND([CXXFLAGS], ["-fasynchronous-unwind-tables"])

dnl  -fvisibility-inlines-hidden - for optimization; note the address of inline
dnl  functions won't be the same between translation units
RB_VAR_PREPEND([CXXFLAGS], ["-fvisibility-inlines-hidden"])

dnl see gcc(1)
AM_COND_IF([GCC],
[
	RB_VAR_PREPEND([CXXFLAGS], ["-fnothrow-opt"])
])

dnl This disables thread-safety for static variable initialization
dnl (specifically local static variables inside functions) which reduces
dnl generated code size and complexity. This is alright since construct is
dnl primarily single threaded. Even if other threads are spawned for special
dnl tasks we consider the likelyhood of a static initialization race to either
dnl be sufficiently low or simply immaterial for our uses of them.
dnl
RB_VAR_PREPEND([CXXFLAGS], ["-fno-threadsafe-statics"])

AM_COND_IF([GCC],
[
	RB_VAR_PREPEND([CXXFLAGS], ["-flax-vector-conversions"])
])

dnl Enables matrix_type attribute for vectors of vectors etc
AM_COND_IF([CLANG11],
[
	RB_VAR_PREPEND([CXXFLAGS], ["-fenable-matrix"])
])

dnl
dnl Machine Tuning
dnl

AM_COND_IF([UNTUNED],
[
	machine_tuning="generic ${host_cpu} and untuned"
	RB_VAR_PREPEND([CXXFLAGS], ["-mno-default"])
], [
	AM_COND_IF([GENERIC],
	[
		machine_tuning="generic ${host_cpu} instruction set"
		RB_VAR_PREPEND([CXXFLAGS], ["-mtune=generic"])
	], [
		machine_tuning="${host_cpu} native instruction set"

		AS_CASE([$host_cpu],
		[x86_64],
		[
			dnl AMD K10's SSE4a doesn't work with valgrind
			RB_VAR_PREPEND([CXXFLAGS], ["-mno-sse4a"])

			dnl Not accepted by clang on aarch64
			RB_VAR_PREPEND([CXXFLAGS], ["-march=native"])
		])

		RB_VAR_PREPEND([CXXFLAGS], ["-mtune=native"])
	])
])

dnl
dnl Compiler warnings
dnl

STACK_USAGE_WARNING=16384
FRAME_SIZE_WARNING=8192
CWARNS=""
AC_MSG_CHECKING(whether to disable warnings)
AC_ARG_ENABLE(warnings,
RB_HELP_STRING([--disable-warnings], [Disable all sorts of warnings like a rockstar]),
[],
[

CHARYBDIS_C_GCC_TRY_FLAGS([-Wall], charybdis_cv_c_gcc_w_all)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wextra], charybdis_cv_c_gcc_w_extra)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wpointer-arith], charybdis_cv_c_gcc_w_pointer_arith)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wcast-align], charybdis_cv_c_gcc_w_cast_align)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wcast-qual], charybdis_cv_c_gcc_w_cast_qual)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wfloat-equal], charybdis_cv_c_gcc_w_float_equal)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wwrite-strings], charybdis_cv_c_gcc_w_write_strings)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wparentheses], charybdis_cv_c_gcc_parentheses)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wundef], charybdis_cv_c_gcc_w_undef)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wpacked], charybdis_cv_c_gcc_w_packed)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wformat -Wformat-y2k -Wformat-nonliteral], charybdis_cv_c_gcc_w_format)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wstrict-aliasing=2 -Wstrict-overflow=5], charybdis_cv_c_gcc_w_strict)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wdisabled-optimization], charybdis_cv_c_gcc_w_disabled_optimization)
CHARYBDIS_C_GCC_TRY_FLAGS([-Winvalid-pch], charybdis_cv_c_gcc_w_invalid_pch)
CHARYBDIS_C_GCC_TRY_FLAGS([-Winit-self], charybdis_cv_c_gcc_w_init_self)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wuninitialized], charybdis_cv_c_gcc_w_uninitialized)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wunreachable-code], charybdis_cv_c_gcc_w_unreachable_code)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wno-overloaded-virtual], charybdis_cv_c_gcc_w_overloaded_virtual)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wnon-virtual-dtor], charybdis_cv_c_gcc_w_non_virtual_dtor)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wnoexcept], charybdis_cv_c_gcc_w_noexcept)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wsized-deallocation], charybdis_cv_c_gcc_w_sized_deallocation)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wctor-dtor-privacy], charybdis_cv_c_gcc_w_ctor_dtor_privacy)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wsign-promo], charybdis_cv_c_gcc_w_sign_promo)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wtrampolines], charybdis_cv_c_gcc_w_trampolines)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wduplicated-cond], charybdis_cv_c_gcc_w_duplicated_cond)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wduplicate-branches], charybdis_cv_c_gcc_w_duplicate_branches)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wrestrict], charybdis_cv_c_gcc_w_restrict)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wnull-dereference], charybdis_cv_c_gcc_w_null_dereference)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wplacement-new=2], charybdis_cv_c_gcc_w_placement_new)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wundef], charybdis_cv_c_gcc_w_undef)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wodr], charybdis_cv_c_gcc_w_odr)

dnl
dnl List of warnings treated as errors
dnl

dnl Uninitialized return values are not useful in this project; forgetting
dnl to return from a function is always a developer mistake here.
CHARYBDIS_C_GCC_TRY_FLAGS([-Werror=return-type], charybdis_cv_c_gcc_w_error_return_type)

dnl
dnl List of warnings we suppress:
dnl

dnl Missing field initializers should have default init or some zero-extension as per std, this
dnl warning is only useful if you want to gauge where that's happening for a specific reason
CHARYBDIS_C_GCC_TRY_FLAGS([-Wno-missing-field-initializers], charybdis_cv_c_gcc_w_missing_field_initializers)

dnl Unused variables, functions, labels, etc are just fine as long as they get optimized
dnl away. These warnings can be re-enabled during a code cleanup, otherwise shut up.
CHARYBDIS_C_GCC_TRY_FLAGS([-Wno-unused], charybdis_cv_c_gcc_w_unused)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wno-unused-function], charybdis_cv_c_gcc_w_unused_function)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wno-unused-label], charybdis_cv_c_gcc_w_unused_label)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wno-unused-value], charybdis_cv_c_gcc_w_unused_value)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wno-unused-variable], charybdis_cv_c_gcc_w_unused_variable)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wno-unused-parameter], charybdis_cv_c_gcc_w_unused_parameter)

dnl This warning is disabled to allow preprocessor statements like #endif to
dnl contain text on the same line. This is used for example to write the
dnl identifier in the matching prior #ifdef, which has no actual effect except
dnl clarity.
CHARYBDIS_C_GCC_TRY_FLAGS([-Wno-endif-labels], charybdis_cv_c_gcc_w_endif_labels)

dnl
dnl Suppressed unless in debug mode
dnl

AM_COND_IF([DEBUG],
[
	CHARYBDIS_C_GCC_TRY_FLAGS([-Wsuggest-final-types], charybdis_cv_c_gcc_w_suggest_final_types)
	CHARYBDIS_C_GCC_TRY_FLAGS([-Wsuggest-final-methods], charybdis_cv_c_gcc_w_suggest_final_methods)
	CHARYBDIS_C_GCC_TRY_FLAGS([-Wsuggest-override], charybdis_cv_c_gcc_w_suggest_override)
	CHARYBDIS_C_GCC_TRY_FLAGS([-Wmissing-noreturn], charybdis_cv_c_gcc_w_missing_noreturn)
], [
	CHARYBDIS_C_GCC_TRY_FLAGS([-Wno-unknown-pragmas], charybdis_cv_c_gcc_w_unknown_pragmas)

	AM_COND_IF([CLANG],
	[
		CHARYBDIS_C_GCC_TRY_FLAGS([-Wno-unknown-attributes], charybdis_cv_c_gcc_w_unknown_attributes)
		CHARYBDIS_C_GCC_TRY_FLAGS([-Wno-profile-instr-unprofiled], charybdis_cv_c_gcc_w_profile_instr_unprofiled)
	])

	AM_COND_IF([GCC],
	[
		CHARYBDIS_C_GCC_TRY_FLAGS([-Wno-ignored-attributes], charybdis_cv_c_gcc_w_ignored_attributes)
	])
])

AM_COND_IF([UNTUNED],
[

], [
	dnl When tuning for a specific target ignore the ABI compat warnings
	CHARYBDIS_C_GCC_TRY_FLAGS([-Wno-psabi], charybdis_cv_c_gcc_w_psabi)
])

dnl
dnl List of compiler-specific / incompatible warning options
dnl

AM_COND_IF([GCC],
[
	CHARYBDIS_C_GCC_TRY_FLAGS([-Wlogical-op], charybdis_cv_c_gcc_w_logical_op)
	CHARYBDIS_C_GCC_TRY_FLAGS([-Wformat-security], charybdis_cv_c_gcc_w_format_security)
	CHARYBDIS_C_GCC_TRY_FLAGS([-Wstack-usage=$STACK_USAGE_WARNING], charybdis_cv_c_gcc_w_stack_usage)
	CHARYBDIS_C_GCC_TRY_FLAGS([-Wframe-larger-than=$FRAME_SIZE_WARNING], charybdis_cv_c_gcc_w_frame_larger_than)
	CHARYBDIS_C_GCC_TRY_FLAGS([-Walloca], charybdis_cv_c_gcc_w_alloca)
	CHARYBDIS_C_GCC_TRY_FLAGS([-Wvector-operation-performance], charybdis_cv_c_gcc_w_vector_operation_performance)

	AM_COND_IF([DEBUG],
	[
		CHARYBDIS_C_GCC_TRY_FLAGS([-Wsuggest-attribute=format], charybdis_cv_c_gcc_w_suggest_attribute_format)
		CHARYBDIS_C_GCC_TRY_FLAGS([-Wsuggest-attribute=noreturn], charybdis_cv_c_gcc_w_suggest_attribute_noreturn)
		CHARYBDIS_C_GCC_TRY_FLAGS([-Wsuggest-attribute=malloc], charybdis_cv_c_gcc_w_suggest_attribute_malloc)
		CHARYBDIS_C_GCC_TRY_FLAGS([-Wsuggest-attribute=cold], charybdis_cv_c_gcc_w_suggest_attribute_cold)
		dnl CHARYBDIS_C_GCC_TRY_FLAGS([-Wsuggest-attribute=pure], charybdis_cv_c_gcc_w_suggest_attribute_pure)
		dnl CHARYBDIS_C_GCC_TRY_FLAGS([-Wsuggest-attribute=const], charybdis_cv_c_gcc_w_suggest_attribute_const)
	])
])

AM_COND_IF([CLANG],
[
	CHARYBDIS_C_GCC_TRY_FLAGS([-Wno-mismatched-tags], charybdis_cv_c_gcc_w_mismatched_tags)
	CHARYBDIS_C_GCC_TRY_FLAGS([-Wno-format-security], charybdis_cv_c_gcc_w_format_security)
	CHARYBDIS_C_GCC_TRY_FLAGS([-Wno-unknown-warning-option], charybdis_cv_c_gcc_w_unknown_warning_option)
])

])

dnl assume the warning flags
RB_VAR_PREPEND([CPPFLAGS], ["$CWARNS"])

dnl mirror the base CFLAGS and CXXFLAGS up to this point
CFLAGS="$CXXFLAGS"


dnl ***************************************************************************
dnl
dnl Philology checks
dnl

AC_ISC_POSIX
AC_C_CONST
AC_C_INLINE
if test "$ac_cv_header_machine_endian_h" = "no" ; then
    AC_C_BIGENDIAN
fi

AC_TYPE_PID_T
AC_TYPE_UID_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_INTPTR_T
AC_TYPE_UINTPTR_T

dnl C standard type sizes on this platform
AC_CHECK_SIZEOF([char])
AC_CHECK_SIZEOF([short])
AC_CHECK_SIZEOF([int])
AC_CHECK_SIZEOF([float])
AC_CHECK_SIZEOF([long])
AC_CHECK_SIZEOF([double])
AC_CHECK_SIZEOF([long long])
AC_CHECK_SIZEOF([long double])

dnl Check for the C++11/C++20 unicode character types and sizes
AC_CHECK_TYPES([char8_t])
AC_CHECK_SIZEOF([char8_t])
AC_CHECK_TYPES([char16_t])
AC_CHECK_SIZEOF([char16_t])
AC_CHECK_TYPES([char32_t])
AC_CHECK_SIZEOF([char32_t])

dnl Probe the details of 128-bit arithmetic support on this platform.
dnl see: include/ircd/portable.h
AC_CHECK_TYPES([int128_t])
AC_CHECK_SIZEOF([int128_t])
AC_CHECK_TYPES([uint128_t])
AC_CHECK_SIZEOF([uint128_t])
AC_CHECK_TYPES([__int128])
AC_CHECK_SIZEOF([__int128])
AC_CHECK_TYPES([__int128_t])
AC_CHECK_SIZEOF([__int128_t])
AC_CHECK_TYPES([__uint128_t])
AC_CHECK_SIZEOF([__uint128_t])

dnl Gauge the support of experimental compact floating point formats
AC_CHECK_TYPES([_Float16])
AC_CHECK_SIZEOF([_Float16])
AC_CHECK_TYPES([__fp16])
AC_CHECK_SIZEOF([__fp16])
AC_CHECK_TYPES([__bf16])
AC_CHECK_SIZEOF([__bf16])

dnl -fsigned-char ensures 'char' is signed on all platforms
RB_VAR_PREPEND([CFLAGS], ["-fsigned-char"])
RB_VAR_PREPEND([CXXFLAGS], ["-fsigned-char"])


dnl ***************************************************************************
dnl
dnl Header files
dnl

AC_DEFUN([RB_CHK_SYSHEADER],
[
	if test -z "$rb_have_sysheader_$2"; then
		AC_CHECK_HEADER([$1],
		[
			rb_have_sysheader_$2=1;
			AC_DEFINE([HAVE_$2], [1], [ Indication $1 is available. ])
			RB_DEFINE_UNQUOTED([INC_$2], [$1>], [ The computed-include location of $1. ])
		], [
			RB_DEFINE_UNQUOTED([INC_$2], [stddef.h>], [ The dead-header in place of $1. ])
		])
	fi
])

AC_HEADER_STDBOOL
dnl AC_HEADER_STDC

dnl libc
RB_CHK_SYSHEADER(assert.h, [ASSERT_H])
RB_CHK_SYSHEADER(stdarg.h, [STDARG_H])

dnl libc++
RB_CHK_SYSHEADER(cstddef, [CSTDDEF])
RB_CHK_SYSHEADER(cstdint, [CSTDINT])
RB_CHK_SYSHEADER(cstdlib, [CSTDLIB])
RB_CHK_SYSHEADER(cstdarg, [CSTDARG])
RB_CHK_SYSHEADER(cassert, [CASSERT])
RB_CHK_SYSHEADER(limits, [LIMITS])
RB_CHK_SYSHEADER(type_traits, [TYPE_TRAITS])
RB_CHK_SYSHEADER(typeindex, [TYPEINDEX])
RB_CHK_SYSHEADER(variant, [VARIANT])
RB_CHK_SYSHEADER(utility, [UTILITY])
RB_CHK_SYSHEADER(execution, [EXECUTION])
RB_CHK_SYSHEADER(functional, [FUNCTIONAL])
RB_CHK_SYSHEADER(algorithm, [ALGORITHM])
RB_CHK_SYSHEADER(numeric, [NUMERIC])
RB_CHK_SYSHEADER(cmath, [CMATH])
RB_CHK_SYSHEADER(cfenv, [CFENV])
RB_CHK_SYSHEADER(memory, [MEMORY])
RB_CHK_SYSHEADER(exception, [EXCEPTION])
RB_CHK_SYSHEADER(cerrno, [CERRNO])
RB_CHK_SYSHEADER(system_error, [SYSTEM_ERROR])
RB_CHK_SYSHEADER(map, [MAP])
RB_CHK_SYSHEADER(set, [SET])
RB_CHK_SYSHEADER(list, [LIST])
RB_CHK_SYSHEADER(stack, [STACK])
RB_CHK_SYSHEADER(queue, [QUEUE])
RB_CHK_SYSHEADER(deque, [DEQUE])
RB_CHK_SYSHEADER(array, [ARRAY])
RB_CHK_SYSHEADER(vector, [VECTOR])
RB_CHK_SYSHEADER(forward_list, [FORWARD_LIST])
RB_CHK_SYSHEADER(unordered_map, [UNORDERED_MAP])
RB_CHK_SYSHEADER(string, [STRING])
RB_CHK_SYSHEADER(cstring, [CSTRING])
RB_CHK_SYSHEADER(locale, [LOCALE])
RB_CHK_SYSHEADER(cuchar, [CUCHAR])
RB_CHK_SYSHEADER(codecvt, [CODECVT])
RB_CHK_SYSHEADER(sstream, [SSTREAM])
RB_CHK_SYSHEADER(fstream, [FSTREAM])
RB_CHK_SYSHEADER(iostream, [IOSTREAM])
RB_CHK_SYSHEADER(iosfwd, [IOSFWD])
RB_CHK_SYSHEADER(iomanip, [IOMANIP])
RB_CHK_SYSHEADER(cstdio, [CSTDIO])
RB_CHK_SYSHEADER(chrono, [CHRONO])
RB_CHK_SYSHEADER(ctime, [CTIME])
RB_CHK_SYSHEADER(atomic, [ATOMIC])
RB_CHK_SYSHEADER(thread, [THREAD])
RB_CHK_SYSHEADER(mutex, [MUTEX])
RB_CHK_SYSHEADER(shared_mutex, [SHARED_MUTEX])
RB_CHK_SYSHEADER(condition_variable, [CONDITION_VARIABLE])
RB_CHK_SYSHEADER(random, [RANDOM])
RB_CHK_SYSHEADER(bitset, [BITSET])
RB_CHK_SYSHEADER(string_view, [STRING_VIEW])
RB_CHK_SYSHEADER(optional, [OPTIONAL])
RB_CHK_SYSHEADER(new, [NEW])
RB_CHK_SYSHEADER(regex, [REGEX])
RB_CHK_SYSHEADER(memory_resource, [MEMORY_RESOURCE])
RB_CHK_SYSHEADER(filesystem, [FILESYSTEM])
RB_CHK_SYSHEADER(cxxabi.h, [CXXABI_H])

dnl unix platform
RB_CHK_SYSHEADER(unistd.h, [UNISTD_H])
RB_CHK_SYSHEADER(signal.h, [SIGNAL_H])
RB_CHK_SYSHEADER(ifaddrs.h, [IFADDRS_H])
RB_CHK_SYSHEADER(netdb.h, [NETDB_H])
RB_CHK_SYSHEADER(fcntl.h, [FCNTL_H])
RB_CHK_SYSHEADER(elf.h, [ELF_H])
RB_CHK_SYSHEADER(link.h, [LINK_H])
RB_CHK_SYSHEADER(dlfcn.h, [DLFCN_H])
RB_CHK_SYSHEADER(execinfo.h, [EXECINFO_H])
RB_CHK_SYSHEADER(sys/types.h, [SYS_TYPES_H])
RB_CHK_SYSHEADER(sys/time.h, [SYS_TIME_H])
RB_CHK_SYSHEADER(sys/stat.h, [SYS_STAT_H])
RB_CHK_SYSHEADER(sys/statfs.h, [SYS_STATFS_H])
RB_CHK_SYSHEADER(sys/statvfs.h, [SYS_STATVFS_H])
RB_CHK_SYSHEADER(sys/resource.h, [SYS_RESOURCE_H])
RB_CHK_SYSHEADER(sys/syscall.h, [SYS_SYSCALL_H])
RB_CHK_SYSHEADER(sys/utsname.h, [SYS_UTSNAME_H])
RB_CHK_SYSHEADER(sys/ioctl.h, [SYS_IOCTL_H])
RB_CHK_SYSHEADER(sys/mman.h, [SYS_MMAN_H])
RB_CHK_SYSHEADER(gnu/libc-version.h, [GNU_LIBC_VERSION_H])
RB_CHK_SYSHEADER(gnu/lib-names.h, [GNU_LIB_NAMES_H])

dnl linux platform
RB_CHK_SYSHEADER(sys/auxv.h, [SYS_AUXV_H])
RB_CHK_SYSHEADER(sys/sysinfo.h, [SYS_SYSINFO_H])
RB_CHK_SYSHEADER(sys/eventfd.h, [SYS_EVENTFD_H])
RB_CHK_SYSHEADER(sys/inotify.h, [SYS_INOTIFY_H])
RB_CHK_SYSHEADER(sys/sysmacros.h, [SYS_SYSMACROS_H])
RB_CHK_SYSHEADER(sys/prctl.h, [SYS_PRCTL_H])
RB_CHK_SYSHEADER(linux/aio_abi.h, [LINUX_AIO_ABI_H])
RB_CHK_SYSHEADER(linux/magic.h, [LINUX_MAGIC_H])
RB_CHK_SYSHEADER(linux/perf_event.h, [LINUX_PERF_EVENT_H])
RB_CHK_SYSHEADER(linux/hw_breakpoint.h, [LINUX_HW_BREAKPOINT_H])
RB_CHK_SYSHEADER(linux/io_uring.h, [LINUX_IO_URING_H])
RB_CHK_SYSHEADER(linux/icmp.h, [LINUX_ICMP_H])

dnl windows platform
RB_CHK_SYSHEADER(windows.h, [WINDOWS_H])
RB_CHK_SYSHEADER(winsock2.h, [WINSOCK2_H])
RB_CHK_SYSHEADER(ws2tcpip.h, [WS2TCPIP_H])
RB_CHK_SYSHEADER(iphlpapi.h, [IPHLPAPI_H])

dnl x86 platform
RB_CHK_SYSHEADER(x86intrin.h, [X86INTRIN_H])
RB_CHK_SYSHEADER(cpuid.h, [CPUID_H])

dnl arm platform
RB_CHK_SYSHEADER(arm_acle.h, [ARM_ACLE_H])
RB_CHK_SYSHEADER(arm_cmse.h, [ARM_ACLE_H])
RB_CHK_SYSHEADER(arm_neon.h, [ARM_NEON_H])
RB_CHK_SYSHEADER(arm_sve.h, [ARM_SVE_H])

dnl instrumentation
RB_CHK_SYSHEADER(malloc.h, [MALLOC_H])
RB_CHK_SYSHEADER(valgrind/valgrind.h, [VALGRIND_VALGRIND_H])
RB_CHK_SYSHEADER(valgrind/memcheck.h, [VALGRIND_MEMCHECK_H])
RB_CHK_SYSHEADER(valgrind/callgrind.h, [VALGRIND_CALLGRIND_H])

dnl experimental
RB_CHK_SYSHEADER(experimental/string_view, [EXPERIMENTAL_STRING_VIEW])
RB_CHK_SYSHEADER(experimental/optional, [EXPERIMENTAL_OPTIONAL])
RB_CHK_SYSHEADER(experimental/memory_resource, [EXPERIMENTAL_MEMORY_RESOURCE])


dnl ***************************************************************************
dnl
dnl Specific function checks
dnl

AC_CHECK_FUNCS([          \
	backtrace             \
	fadvise               \
	madvise               \
	mlock                 \
	mlock2                \
	mmap                  \
	posix_fadvise         \
	posix_madvise         \
	preadv2               \
	pwritev2              \
	strlcat               \
	strlcpy               \
])

AC_SEARCH_LIBS(dlinfo, dl, AC_DEFINE(HAVE_DLINFO, 1, [Define if you have dlinfo]))
AC_SEARCH_LIBS(nanosleep, rt posix4, AC_DEFINE(HAVE_NANOSLEEP, 1, [Define if you have nanosleep]))

dnl
dnl Networking Functions
dnl

dnl
dnl ip6
dnl

AC_ARG_ENABLE(ipv6, RB_HELP_STRING([--disable-ipv6], [Disable IPv6 support]),
[
	ipv6=$enableval
], [
	ipv6="yes"
])

AM_CONDITIONAL([HAVE_IPV6], [[[[ "$ipv6" = "yes" ]]]])

AM_COND_IF(HAVE_IPV6,
[
	AC_DEFINE([HAVE_IPV6], [1], [IPv6 is supported])
],[])

dnl
dnl netdb / libnss_db
dnl

AC_CHECK_FUNCS([          \
	endprotoent           \
	getprotobyname        \
	getprotobyname_r      \
	setprotoent           \
])

dnl Even if getprotobyname(3) is found linux tries to open nss_db at runtime and
dnl that surprises our user with trouble. The check catches it here FWIW.
AM_COND_IF(LINUX,
[
	AC_CHECK_LIB([nss_db], [_nss_db_getprotobyname_r],
	[
		AC_DEFINE(HAVE_LIBNSS_DB, 1, [Define if you have libnss_db])
	], [
		AC_MSG_WARN([Missing libnss_db.so for getprotobyname(3) impl. Try apt-get install libnss-db])
	])
])


dnl ***************************************************************************
dnl
dnl Miscellaneous operating system specific
dnl

AM_COND_IF(MINGW,
[
	AC_DEFINE(HAVE_WIN32, [1], [Define to 1 if you are on windows])
	RB_VAR_PREPEND([LIBS], ["-lws2_32 -liphlpapi"])
], [

	RB_VAR_PREPEND([LIBS], ["-latomic"])
	RB_VAR_PREPEND([LIBS], ["-lpthread"])
	AM_COND_IF([GCC],
	[
		RB_VAR_PREPEND([CXXFLAGS], ["-pthread"])
	])
])

AM_COND_IF(INTERIX,
[
	CPPDEFINE([_ALL_SOURCE])
	CPPDEFINE([_XOPEN_SOURCE], [500])
	CPPDEFINE([_POSIX_PTHREAD_SEMANTICS])
	CPPDEFINE([_XPG4_2])
],[])

dnl
dnl Linux AIO support
dnl

AM_COND_IF(LINUX,
[
	AC_ARG_ENABLE(aio, RB_HELP_STRING([--disable-aio], [Disable kernel AIO support]),
	[
		aio=$enableval
	], [
		aio=yes
	])
], [])

if test "$aio" = "yes"; then
	IRCD_DEFINE(USE_AIO, [1], [Linux AIO is supported and will be used])
fi

AM_CONDITIONAL([AIO], [[[[ $aio = yes ]]]])

dnl
dnl Linux io_uring support
dnl

AM_COND_IF(LINUX,
[
	AC_ARG_ENABLE(io_uring, RB_HELP_STRING([--disable-io_uring], [Disable kernel io_uring support]),
	[
		io_uring=$enableval
	], [
		io_uring="$ac_cv_header_linux_io_uring_h"
	])
])

AM_CONDITIONAL([IOU], [[[[ $io_uring = yes ]]]])

AM_COND_IF([IOU],
[
	IRCD_DEFINE(USE_IOU, [1], [Linux io_uring is supported and may be used])
])


dnl ***************************************************************************
dnl
dnl Installation Layout
dnl

AC_PREFIX_DEFAULT([/usr])
AC_PREFIX_PROGRAM([/usr])

PKG_PROG_PKG_CONFIG

AC_SUBST_DIR([prefix])
if test "$prefix" = "NONE"; then
	prefix="$ac_default_prefix"
else
	dnl Don't get bitten by Cygwin's stupidity if the user specified
	dnl a custom prefix with a trailing slash
	prefix=`echo $prefix | sed 's/\/$//'`
fi

RB_DEFINE_UNQUOTED(PREFIX, ["$prefix"], [Prefix of IRCd install])

dnl
dnl bindir
dnl

AC_SUBST_DIR([bindir])
RB_DEFINE_UNQUOTED([BIN_DIR], ["$bindir"], [Directory where binary executables are to be found.])

dnl
dnl Check for --with-confdir [deprecated, use --sysconfdir instead]
dnl

AC_SUBST_DIR([sysconfdir])
RB_DEFINE_UNQUOTED([CONF_DIR], ["$sysconfdir"], [Prefix where config files are installed.])

dnl
dnl includedir
dnl

AC_SUBST_DIR([includedir])
RB_DEFINE_UNQUOTED([INCLUDE_DIR], ["$includedir"], [Prefix where headers are installed.])

dnl
dnl libdir
dnl

AC_SUBST_DIR([libdir])
RB_DEFINE_UNQUOTED([LIB_DIR], ["$libdir"], [Prefix where libs are installed.])

dnl
dnl Check for --with-moduledir
dnl

AC_SUBST_DIR([moduledir])
AC_MSG_CHECKING([whether to modify moduledir])
AC_ARG_WITH(moduledir, RB_HELP_STRING([--with-moduledir=DIR], [Directory to install modules]),
[
	AC_MSG_RESULT(yes)
	moduledir=`echo $withval | sed 's/\/$//'`
], [
	AC_MSG_RESULT(no)
	moduledir="${libdir}/modules/${PACKAGE_TARNAME}"
])

RB_DEFINE_UNQUOTED([MODULE_DIR], ["$moduledir"], [Prefix where modules are installed.])

dnl
dnl datarootdir
dnl

AC_SUBST_DIR([datarootdir])
RB_DEFINE_UNQUOTED([DATAROOT_DIR], ["$datarootdir"], [Directory where read-only assets go.])

dnl
dnl datadir
dnl

AC_SUBST_DIR([datadir])

if test "x$datarootdir" = "x$datadir"; then
	datadir="$datarootdir/${PACKAGE_TARNAME}"
fi

RB_DEFINE_UNQUOTED([DATA_DIR], ["$datadir"], [Directory where read-only assets go.])

dnl
dnl Check for --with-webappdir
dnl

AC_MSG_CHECKING([whether to modify webappdir])
AC_ARG_WITH(webappdir, RB_HELP_STRING([--with-webappdir=DIR], [Directory to install served assets]),
[
	AC_MSG_RESULT(yes)
	webappdir=`echo $withval | sed 's/\/$//'`
], [
	AC_MSG_RESULT(no)
	webappdir="${datadir}/webapp"
])

RB_DEFINE_UNQUOTED([WEBAPP_DIR], ["$webappdir"], [Prefix where website assets are served from.])
AC_SUBST_DIR([webappdir])


dnl
dnl localstatedir
dnl

dnl Avoid name collisions.
AC_SUBST_DIR([localstatedir])
RB_DEFINE_UNQUOTED([LOCALSTATE_DIR], ["$localstatedir"], [Prefix where db and logs go.])

dnl
dnl Check for --with-dbdir
dnl

AC_MSG_CHECKING([whether to modify database directory])
AC_ARG_WITH(dbdir, RB_HELP_STRING([--with-dbdir=DIR], [Directory where databases are located]),
[
	AC_MSG_RESULT(yes)
	dbdir=`echo $withval | sed 's/\/$//'`
], [
	AC_MSG_RESULT(no)
	dbdir="${localstatedir}/db/${PACKAGE_TARNAME}"
])

RB_DEFINE_UNQUOTED([DB_DIR], ["$dbdir"], [Directory where databases are located])
AC_SUBST_DIR([dbdir])

dnl
dnl Check for --with-logdir
dnl

AC_MSG_CHECKING([whether to modify logdir])
AC_ARG_WITH(logdir, RB_HELP_STRING([--with-logdir=DIR], [Directory where to write logfiles]),
[
	AC_MSG_RESULT(yes)
	logdir=`echo $withval | sed 's/\/$//'`
], [
	AC_MSG_RESULT(no)
	logdir="${localstatedir}/log/${PACKAGE_TARNAME}"
])

RB_DEFINE_UNQUOTED([LOG_DIR], ["$logdir"], [Prefix where to write logfiles.])
AC_SUBST_DIR([logdir])

dnl
dnl runstatedir
dnl

AC_SUBST_DIR([runstatedir])
runstatedir="$runstatedir/${PACKAGE_TARNAME}"
RB_DEFINE_UNQUOTED([RUN_DIR], ["$runstatedir"], [Prefix where runtime ephemeral files go.])


dnl ***************************************************************************
dnl
dnl Third party
dnl

dnl
dnl
dnl unicode / icu
dnl
dnl

AC_SUBST(ICU_CPPFLAGS, [])
AC_SUBST(ICU_LDFLAGS, [])
AC_SUBST(ICU_LIBS, [])

AC_ARG_WITH(icu-includes,
RB_HELP_STRING([--with-icu-includes=[[[DIR]]]], [Path to unicode include directory]),
[
	ICU_CPPFLAGS+="-isystem $withval"
])

AC_ARG_WITH(icu-libs,
RB_HELP_STRING([--with-icu-libs=[[[DIR]]]], [Path to unicode libraries directory]),
[
	ICU_LDFLAGS+=" -L$withval"
])

dnl RB_CHK_SYSHEADER(unicode/utypes.h, [UNICODE_UTYPES_H])

dnl RB_CHK_SYSHEADER(unicode/ustring.h, [UNICODE_USTRING_H])
dnl RB_CHK_SYSHEADER(unicode/utext.h, [UNICODE_UTEXT_H])
dnl RB_CHK_SYSHEADER(unicode/unistr.h, [UNICODE_UNISTR_H])
dnl RB_CHK_SYSHEADER(unicode/chariter.h, [UNICODE_CHARITER_H])
dnl RB_CHK_SYSHEADER(unicode/stringpiece.h, [UNICODE_STRINGPIECE_H])
dnl RB_CHK_SYSHEADER(unicode/utf.h, [UNICODE_UTF_H])

dnl RB_CHK_SYSHEADER(unicode/uchar.h, [UNICODE_UCHAR_H])
dnl RB_CHK_SYSHEADER(unicode/uscript.h, [UNICODE_USCRIPT_H])

dnl RB_CHK_SYSHEADER(unicode/uset.h, [UNICODE_USET_H])
dnl RB_CHK_SYSHEADER(unicode/uniset.h, [UNICODE_UNISET_H])

dnl RB_CHK_SYSHEADER(unicode/uloc.h, [UNICODE_ULOC_H])
dnl RB_CHK_SYSHEADER(unicode/locid.h, [UNICODE_LOCID_H])

dnl RB_CHK_SYSHEADER(unicode/usearch.h, [UNICODE_USEARCH_H])
dnl RB_CHK_SYSHEADER(unicode/stsearch.h, [UNICODE_STSEARCH_H])

dnl RB_CHK_SYSHEADER(unicode/ubrk.h, [UNICODE_UBRK_H])
dnl RB_CHK_SYSHEADER(unicode/brkiter.h, [UNICODE_BRKITER_H])

dnl RB_CHK_SYSHEADER(unicode/uidna.h, [UNICODE_UIDNA_H]	)
dnl RB_CHK_SYSHEADER(unicode/idna.h, [UNICODE_IDNA_H])

dnl RB_CHK_SYSHEADER(unicode/utmscale.h, [UNICODE_UTMSCALE_H])

have_icu="no"
PKG_CHECK_MODULES(icuuc, [icuuc],
[
	have_icu="yes"
	have_icuuc="yes"
	ICU_LIBS+=" -licuuc"
], [
	AC_CHECK_LIB([icuuc], [_init],
	[
		have_icu="yes"
		have_icuuc="yes"
		ICU_LIBS+=" -licuuc"
	], [
		have_icuuc="no"
	])
])

AM_CONDITIONAL([ICUUC], [test "x$have_icuuc" = "xyes" ])
AM_CONDITIONAL([ICU], [test "x$have_icu" = "xyes" ])

dnl
dnl
dnl libsodium (NaCl) crypto support
dnl
dnl

AC_SUBST(SODIUM_CPPFLAGS, [])
AC_SUBST(SODIUM_LDFLAGS, [])
AC_SUBST(SODIUM_LIBS, [])

AC_ARG_WITH(sodium-includes,
RB_HELP_STRING([--with-sodium-includes=[[[DIR]]]], [Path to sodium include directory]),
[
	SODIUM_CPPFLAGS+=" -I$withval"
], [])

AC_ARG_WITH(sodium-libs,
RB_HELP_STRING([--with-sodium-libs=[[[DIR]]]], [Path to sodium library directory]),
[
	SODIUM_LDFLAGS+=" -L$withval"
], [])

PKG_CHECK_MODULES(sodium, libsodium,
[
	have_sodium="yes"
], [
	dnl NOTE that PKG_CHECK_MODULES is preferred, but on some systems it gives
	dnl a false negative and we end up here with an AC_CHECK_LIB fallback.
	AC_CHECK_LIB([sodium], sodium_init,
	[
		have_sodium="yes"
	], [
		have_sodium="no"
	])
])

AM_CONDITIONAL([SODIUM], [test "x$have_sodium" = "xyes"])

AM_COND_IF([SODIUM],
[
	RB_CHK_SYSHEADER(sodium.h, [SODIUM_H])
	AC_DEFINE([HAVE_SODIUM], [1], [Have libsodium])
	SODIUM_LIBS+=" -lsodium"
], [
	AC_MSG_ERROR([libsodium is required but not found. Try apt-get install libsodium-dev])
])

dnl
dnl
dnl OpenSSL support
dnl
dnl

AC_SUBST(SSL_CPPFLAGS, [])
AC_SUBST(SSL_LDFLAGS, [])
AC_SUBST(SSL_LIBS, [])

AC_ARG_WITH(ssl,
RB_HELP_STRING([--with-ssl=[[[DIR]]]], [Path to SSL source directory]),
[
	SSL_CPPFLAGS="-I$withval/include"
	SSL_LDFLAGS="-L$withval"
], [])

AC_ARG_WITH(ssl-includes,
RB_HELP_STRING([--with-ssl-includes=[[[DIR]]]], [Path to SSL include directory]),
[
	SSL_CPPFLAGS="-I$withval"
], [])

AC_ARG_WITH(ssl-libs,
RB_HELP_STRING([--with-ssl-libs=[[[DIR]]]], [Path to SSL library directory]),
[
	SSL_LDFLAGS="-L$withval"
], [])

RB_CHK_SYSHEADER(openssl/err.h, [OPENSSL_ERR_H])
RB_CHK_SYSHEADER(openssl/asn1.h, [OPENSSL_ASN1_H])
RB_CHK_SYSHEADER(openssl/sha.h, [OPENSSL_SHA_H])
RB_CHK_SYSHEADER(openssl/hmac.h, [OPENSSL_HMAC_H])
RB_CHK_SYSHEADER(openssl/ssl.h, [OPENSSL_SSL_H])
RB_CHK_SYSHEADER(openssl/ec.h, [OPENSSL_EC_H])
RB_CHK_SYSHEADER(openssl/rsa.h, [OPENSSL_RSA_H])
RB_CHK_SYSHEADER(openssl/x509.h, [OPENSSL_X509_H])
RB_CHK_SYSHEADER(openssl/evp.h, [OPENSSL_EVP_H])
RB_CHK_SYSHEADER(openssl/ripemd.h, [OPENSSL_RIPEMD_H])
RB_CHK_SYSHEADER(openssl/dh.h, [OPENSSL_DH_H])
RB_CHK_SYSHEADER(openssl/tls1.h, [OPENSSL_TLS1_H])
PKG_CHECK_MODULES(ssl, [ssl],
[
	have_ssl="yes"
], [
	AC_CHECK_LIB(ssl, SSL_version,
	[
		have_ssl="yes"
	], [
		have_ssl="no"
		AC_MSG_ERROR([libssl is required but not found. Try apt-get install libssl-dev])
	])
])

AM_CONDITIONAL([SSL], [test "x$have_ssl" = "xyes"])

AM_COND_IF([SSL],
[
	SSL_LIBS="-lssl"
])

AC_SUBST(CRYPTO_CPPFLAGS, [])
AC_SUBST(CRYPTO_LDFLAGS, [])
AC_SUBST(CRYPTO_LIBS, [])

PKG_CHECK_MODULES(crypto, [crypto],
[
	have_crypto="yes"
], [
	AC_CHECK_LIB(crypto, OPENSSL_init,
	[
		have_crypto="yes"
		CRYPTO_LIBS="-lcrypto"
	], [
		have_crypto="no"
		AC_MSG_ERROR([libcrypto is required but not found.])
	])
])

AM_CONDITIONAL([CRYPTO], [test "x$have_crypto" = "xyes"])

AM_COND_IF([CRYPTO],
[
	AC_DEFINE(HAVE_CRYPTO, 1, [Define to 1 if libcrypto (-lcrypto) is available.])
	CRYPTO_LIBS="-lcrypto"
])

dnl
dnl
dnl libmagic support
dnl
dnl

AC_SUBST(MAGIC_CPPFLAGS, [])
AC_SUBST(MAGIC_LDFLAGS, [])
AC_SUBST(MAGIC_LIBS, [])

AC_ARG_WITH(magic-includes,
RB_HELP_STRING([--with-magic-includes=[[[DIR]]]], [Path to magic include directory]),
[
	MAGIC_CPPFLAGS="-I$withval"
], [])

AC_ARG_WITH(magic-libs,
RB_HELP_STRING([--with-magic-libs=[[[DIR]]]], [Path to magic library directory]),
[
	MAGIC_LDFLAGS="-L$withval"
], [])

AC_ARG_WITH(magic-file,
RB_HELP_STRING([--with-magic-file=[[[FILE]]]], [Path to magic database file]),
[
	MAGIC_FILE="$withval"
	RB_DEFINE_UNQUOTED([MAGIC_FILE], ["$MAGIC_FILE"], [Magic file path])
], [
	RB_DEFINE_UNQUOTED([MAGIC_FILE], [""], [Magic file path])
])

RB_CHK_SYSHEADER(magic.h, [MAGIC_H])
PKG_CHECK_MODULES(magic, [magic],
[
	have_magic="yes"
], [
	AC_CHECK_LIB(magic, magic_version,
	[
		have_magic="yes"
	], [
		have_magic="no"
		AC_MSG_ERROR([libmagic is required but not found. Try apt-get install libmagic-dev])
	])
])

AM_CONDITIONAL([MAGIC], [test "x$have_magic" = "xyes"])

AM_COND_IF([MAGIC],
[
	MAGIC_LIBS="-lmagic"
])

dnl
dnl
dnl zstd
dnl
dnl

AC_SUBST(ZSTD_CPPFLAGS)
AC_SUBST(ZSTD_LDFLAGS)
AC_SUBST(ZSTD_LIBS)

AC_ARG_WITH(zstd-includes,
RB_HELP_STRING([--with-zstd-includes=[[[DIR]]]], [Path to zstd include directory]),
[
	ZSTD_CPPFLAGS="-I$withval"
], [])

AC_ARG_WITH(zstd-libs,
RB_HELP_STRING([--with-zstd-libs=[[[DIR]]]], [Path to zstd library directory]),
[
	ZSTD_LDFLAGS="-L$withval"
], [])

RB_CHK_SYSHEADER(zstd.h, [ZSTD_H])
PKG_CHECK_MODULES(zstd, [zstd],
[
	have_zstd="yes"
], [
	AC_CHECK_LIB(zstd, ZSTD_versionNumber,
	[
		have_zstd="yes"
	], [
		have_zstd="no"
	])
])

AM_CONDITIONAL([ZSTD], [test "x$have_zstd" = "xyes"])

AM_COND_IF([ZSTD],
[
	ZSTD_LIBS="-lzstd"
])

dnl
dnl
dnl zlib support
dnl
dnl

AC_SUBST(Z_CPPFLAGS)
AC_SUBST(Z_LDFLAGS)
AC_SUBST(Z_LIBS)

AC_ARG_WITH(z-includes,
RB_HELP_STRING([--with-z-includes=[[[DIR]]]], [Path to zlib include directory]),
[
	Z_CPPFLAGS="-I$withval"
], [])

AC_ARG_WITH(z-libs,
RB_HELP_STRING([--with-z-libs=[[[DIR]]]], [Path to zlib library directory]),
[
	Z_LDFLAGS="-L$withval"
], [])

RB_CHK_SYSHEADER(zlib.h, [ZLIB_H])
PKG_CHECK_MODULES(zlib, [zlib],
[
	have_zlib="yes"
], [
	AC_CHECK_LIB(z, zlibVersion,
	[
		have_zlib="yes"
	], [
		have_zlib="no"
	])
])

AM_CONDITIONAL([ZLIB], [test "x$have_zlib" = "xyes"])

AM_COND_IF([ZLIB],
[
	ZLIB_LIBS="-lz"
])

dnl
dnl
dnl lz4 support
dnl
dnl

AC_SUBST(LZ4_CPPFLAGS)
AC_SUBST(LZ4_LDFLAGS)
AC_SUBST(LZ4_LIBS)

AC_ARG_WITH(lz4-includes,
RB_HELP_STRING([--with-lz4-includes=[[[DIR]]]], [Path to LZ4 include directory]),
[
	LZ4_CPPFLAGS="-I$withval"
], [])

AC_ARG_WITH(lz4-libs,
RB_HELP_STRING([--with-lz4-libs=[[[DIR]]]], [Path to LZ4 library directory]),
[
	LZ4_LDFLAGS="-L$withval"
], [])

RB_CHK_SYSHEADER(lz4.h, [LZ4_H])
PKG_CHECK_MODULES(lz4, [lz4],
[
	have_lz4="yes"
], [
	AC_CHECK_LIB(lz4, LZ4_versionNumber,
	[
		have_lz4="yes"
	], [
		have_lz4="no"
	])
])

AM_CONDITIONAL([LZ4], [test "x$have_lz4" = "xyes"])

AM_COND_IF([LZ4],
[
	LZ4_LIBS="-llz4"
])

dnl
dnl
dnl snappy support
dnl
dnl

AC_SUBST(SNAPPY_CPPFLAGS)
AC_SUBST(SNAPPY_LDFLAGS)
AC_SUBST(SNAPPY_LIBS)

AC_ARG_WITH(snappy-includes,
RB_HELP_STRING([--with-snappy-includes=[[[DIR]]]], [Path to snappy include directory]),
[
	SNAPPY_CPPFLAGS="-I$withval"
], [])

AC_ARG_WITH(snappy-libs,
RB_HELP_STRING([--with-snappy-libs=[[[DIR]]]], [Path to snappy library directory]),
[
	SNAPPY_LDFLAGS="-L$withval"
], [])

RB_CHK_SYSHEADER(snappy.h, [SNAPPY_H])
PKG_CHECK_MODULES(snappy, [snappy],
[
	have_snappy="yes"
], [
	AC_CHECK_LIB(snappy, snappy_compress,
	[
		have_snappy="yes"
	], [
		have_snappy="no"
	])
])

AM_CONDITIONAL([SNAPPY], [test "x$have_snappy" = "xyes"])

AM_COND_IF([SNAPPY],
[
	SNAPPY_LIBS="-lsnappy"
])

dnl
dnl
dnl libgmp support
dnl
dnl

AC_SUBST(GMP_CPPFLAGS, [])
AC_SUBST(GMP_LDFLAGS, [])
AC_SUBST(GMP_LIBS, [])

AC_ARG_WITH(gmp-includes,
RB_HELP_STRING([--with-gmp-includes=[[[DIR]]]], [Path to libgmp include directory]),
[
	GMP_CPPFLAGS="-isystem $withval"
], [])

AC_ARG_WITH(gmp-libs,
RB_HELP_STRING([--with-gmp-libs=[[[DIR]]]], [Path to libgmp library directory]),
[
	GMP_LDFLAGS="-L$withval"
], [])

PKG_CHECK_MODULES(gmp, [gmp],
[
	have_gmp="yes"
], [
	AC_CHECK_LIB([gmp], [__gmp_version],
	[
		have_gmp="yes"
	], [
		have_gmp="no"
	])
])

AM_CONDITIONAL([GMP], [test "x$have_gmp" = "xyes" ])

AM_COND_IF([GMP],
[
	GMP_LIBS="-lgmp"
], [
	AC_MSG_WARN([Unable to find GNU Multi-Precision library (libgmp) on this system.])
])

dnl
dnl
dnl libpbc support
dnl
dnl

AC_SUBST(PBC_CPPFLAGS, [])
AC_SUBST(PBC_LDFLAGS, [])
AC_SUBST(PBC_LIBS, [])

AC_ARG_WITH(pbc-includes,
RB_HELP_STRING([--with-pbc-includes=[[[DIR]]]], [Path to libpbc include directory]),
[
	PBC_CPPFLAGS="-isystem $withval"
], [])

AC_ARG_WITH(pbc-libs,
RB_HELP_STRING([--with-pbc-libs=[[[DIR]]]], [Path to libpbc library directory]),
[
	PBC_LDFLAGS="-L$withval"
], [])

AC_MSG_CHECKING([whether you asked to use the Pairing-Based Cryptography library included here])
AC_ARG_WITH(included-pbc,
RB_HELP_STRING([--with-included-pbc], [Use the libpbc sources from included submodule]),
[
	AC_MSG_RESULT([yes])
	have_pbc="yes"
	with_included_pbc="yes"
	AC_DEFINE(HAVE_PBC, 1, [Define to 1 if libpbc is available.])

	PBC_CPPFLAGS="-isystem $PWD/deps/pbc/include"
	PBC_LDFLAGS="-L$PWD/deps/pbc/"
	PBC_LIBS="$PWD/deps/pbc/libpbc.a"

	bash tools/buildpbc.sh
	AS_IF([ test $? != 0 ],
	[
		AC_MSG_ERROR([Failed to build Pairing-Based Cryptography library included submodule])
	])
], [
	with_included_pbc="no"
	PKG_CHECK_MODULES(pbc, [pbc],
	[
		have_pbc="yes"
		AC_DEFINE(HAVE_PBC, 1, [Define to 1 if libpbc is available.])
		PBC_LIBS="-lpbc"
	], [
		AC_CHECK_LIB([pbc], [pbc_set_memory_functions],
		[
			have_pbc="yes"
			AC_DEFINE(HAVE_PBC, 1, [Define to 1 if libpbc is available.])
			PBC_LIBS="-lpbc"
		], [
			have_pbc="no"
			AC_MSG_WARN([Unable to find Pairing-Based Cryptography (libpbc) on this system.])
		])
	])
])

AM_CONDITIONAL([PBC], [test "x$have_pbc" = "xyes" ])

dnl
dnl
dnl Portable Network Graphics (PNG) support
dnl
dnl

AC_SUBST(PNG_CPPFLAGS, [])
AC_SUBST(PNG_LDFLAGS, [])
AC_SUBST(PNG_LIBS, [])

AC_ARG_WITH(png-includes,
RB_HELP_STRING([--with-png-includes=[[[DIR]]]], [Path to PNG include directory]),
[
	PNG_CPPFLAGS+="-I$withval"
], [
	PNG_CPPFLAGS+="-isystem /usr/include/GraphicsMagick/"
])

AC_ARG_WITH(png-libs,
RB_HELP_STRING([--with-png-libs=[[[DIR]]]], [Path to PNG library directory]),
[
	PNG_LDFLAGS+="-L$withval"
], [])

RB_CHK_SYSHEADER([png.h], [PNG_H])
PKG_CHECK_MODULES(PNG, [png],
[
	dnl May not be DT_NEEDED in actual library since Magick loads it dynamically.
	PNG_LIBS+="-lpng"
	have_png="yes"
], [
	AC_CHECK_LIB([png], [png_access_version_number],
	[
		have_png="yes"
		PNG_LIBS+="-lpng"
	], [
		have_png="no"
		AC_MSG_WARN([Unable to find libpng for portable network graphics on this system.])
	])
])

AM_CONDITIONAL([PNG], [test "x$have_png" = "xyes"])

dnl IRCD_ENABLE_MAGICK is always defined for use in a constexpr if
AM_COND_IF([PNG],
[
	IRCD_DEFINE(USE_PNG, [1], [Portable Network Graphics (PNG) support is available and enabled])
], [
	IRCD_DEFINE(USE_PNG, [0], [Portable Network Graphics (PNG) support is unavailable or disabled])
])

dnl
dnl
dnl ImageMagick support
dnl
dnl

AC_SUBST(IMAGEMAGICK_CPPFLAGS, [])
AC_SUBST(IMAGEMAGICK_LDFLAGS, [])
AC_SUBST(IMAGEMAGICK_LIBS, [])

AC_ARG_WITH(imagemagick-includes,
RB_HELP_STRING([--with-imagemagick-includes=[[[DIR]]]], [Path to imagemagick include directory]),
[
	IMAGEMAGICK_CPPFLAGS+="-I$withval"
], [
	IMAGEMAGICK_CPPFLAGS+="-isystem /usr/include/GraphicsMagick/"
])

AC_ARG_WITH(imagemagick-libs,
RB_HELP_STRING([--with-imagemagick-libs=[[[DIR]]]], [Path to imagemagick library directory]),
[
	IMAGEMAGICK_LDFLAGS+="-L$withval"
], [])

dnl push
restore_cppflags=$CPPFLAGS
restore_ldflags=$LDFLAGS
RB_VAR_PREPEND([CPPFLAGS], ["${IMAGEMAGICK_CPPFLAGS}"])
RB_VAR_PREPEND([LDFLAGS], ["${IMAGEMAGICK_LDFLAGS}"])

RB_CHK_SYSHEADER([Magick++.h], [MAGICKPP_H])
PKG_CHECK_MODULES(GraphicsMagickPP, [GraphicsMagick++],
[
	IMAGEMAGICK_LIBS+=" -lGraphicsMagick++"
	have_imagemagickpp="yes"
], [
	have_imagemagickpp="no"
])

RB_CHK_SYSHEADER([wand/wand_api.h], [MAGICK_WAND_API_H])
PKG_CHECK_MODULES(GraphicsMagickWand, [GraphicsMagickWand],
[
	IMAGEMAGICK_LIBS+=" -lGraphicsMagickWand"
	have_imagemagickwand="yes"
], [
	have_imagemagickwand="no"
])

RB_CHK_SYSHEADER([magick/api.h], [MAGICK_API_H])
PKG_CHECK_MODULES(GraphicsMagick, [GraphicsMagick],
[
	IMAGEMAGICK_LIBS+=" -lGraphicsMagick"
	have_imagemagick="yes"
], [
	have_imagemagick="no"
])

dnl pop
CPPFLAGS=$restore_cppflags
LDFLAGS=$restore_ldflags

AM_CONDITIONAL([IMAGEMAGICK], [test "x$have_imagemagick" = "xyes"])

dnl IRCD_ENABLE_MAGICK is always defined for use in a constexpr if
AM_COND_IF([IMAGEMAGICK],
[
	IRCD_DEFINE(USE_MAGICK, [1], [Magick support is available and enabled])
])

dnl
dnl
dnl jemalloc
dnl
dnl

RB_CHK_SYSHEADER(jemalloc/jemalloc.h, [JEMALLOC_H])
PKG_CHECK_MODULES(jemalloc, [jemalloc],
[
	have_jemalloc="yes"
], [
	AC_CHECK_LIB(jemalloc, malloc,
	[
		have_jemalloc="yes"
	], 	[
		have_jemalloc="no"
	])
])

AC_MSG_CHECKING(whether to enable use of any found jemalloc)
AC_ARG_ENABLE(jemalloc, RB_HELP_STRING([--disable-jemalloc], [Disable jemalloc as third-party dynamic memory manager]),
[
	AC_MSG_RESULT([$enableval])
	enable_jemalloc=$enableval
], [
	AC_MSG_RESULT([no])
	enable_jemalloc="yes"
])

AM_CONDITIONAL([JEMALLOC], [test "x$have_jemalloc" = "xyes" && test "x$enable_jemalloc" = "xyes" ])

dnl
dnl
dnl Select a third-party allocator
dnl
dnl

AC_SUBST(MALLOC_CPPFLAGS)
AC_SUBST(MALLOC_CXXFLAGS)
AC_SUBST(MALLOC_LDFLAGS)
AC_SUBST(MALLOC_LIBS)
alloc_lib="standard"

dnl ALLOC_LIBS true unless --disable-alloc-libs configured
AM_COND_IF([MALLOC_LIBS],
[
	AM_COND_IF([JEMALLOC],
	[
		alloc_lib="jemalloc"
		MALLOC_LIBS+=" -ljemalloc"
		AC_DEFINE(IRCD_ALLOCATOR_USE_JEMALLOC, 1, [Use jemalloc as the allocator])
	], [
		AC_DEFINE(IRCD_ALLOCATOR_USE_DEFAULT, 1, [Use the default allocator])
	])
], [
	AC_DEFINE(IRCD_ALLOCATOR_USE_DEFAULT, 1, [Use the default allocator])
])

dnl
dnl
dnl libLLVM
dnl
dnl

AC_SUBST(LLVM_CPPFLAGS, [])
AC_SUBST(LLVM_LDFLAGS, [])
AC_SUBST(LLVM_LIBS, [])

AC_ARG_WITH(llvm-includes,
RB_HELP_STRING([--with-llvm-includes=[[[DIR]]]], [Path to libllvm include directory]),
[
	LLVM_CPPFLAGS="-isystem $withval"
], [
	LLVM_CPPFLAGS="-isystem /usr/lib/llvm-9/include"
])

AC_ARG_WITH(llvm-libs,
RB_HELP_STRING([--with-llvm-libs=[[[DIR]]]], [Path to libllvm library directory]),
[
	LLVM_LDFLAGS="-L$withval"
], [
	LLVM_LDFLAGS="-L/usr/lib/llvm-9/lib"
])

PKG_CHECK_MODULES(LLVM, [LLVM],
[
	have_libllvm="yes"
	LLVM_LIBS="-lLLVM"
], [
	AC_CHECK_LIB([LLVM], [LLVMBuildAnd],
	[
		have_libllvm="yes"
		LLVM_LIBS="-lLLVM"
	], [
		have_libllvm="no"
		AC_MSG_WARN([Unable to find LLVM compiler infrastructure library on this system.])
	])
])

AC_MSG_CHECKING(whether to enable experimental libllvm module)
AC_ARG_ENABLE(devllvm, RB_HELP_STRING([--enable-devllvm], [Enable experimental LLVM module development]),
[
	AC_MSG_RESULT([yes])
], [
	AC_MSG_RESULT([no])
	have_libllvm="no"
])

AM_CONDITIONAL([LLVM], [test "x$have_libllvm" = "xyes" ])

dnl
dnl
dnl OpenCL
dnl
dnl

AC_SUBST(OPENCL_CPPFLAGS, [])
AC_SUBST(OPENCL_LDFLAGS, [])
AC_SUBST(OPENCL_LIBS, [])

AC_ARG_WITH(opencl-includes,
RB_HELP_STRING([--with-opencl-includes=[[[DIR]]]], [Path to OpenCL include directory]),
[
	opencl_incdir="$withval"
	OPENCL_CPPFLAGS="-isystem $withval"
], [])

AC_ARG_WITH(opencl-libs,
RB_HELP_STRING([--with-opencl-libs=[[[DIR]]]], [Path to OpenCL library directory]),
[
	opencl_libdir="$withval"
	OPENCL_LDFLAGS="-L$withval"
], [])

RB_CHK_SYSHEADER(CL/cl.h, [CL_CL_H])
RB_CHK_SYSHEADER(CL/opencl.h, [CL_OPENCL_H])
PKG_CHECK_MODULES(OpenCL, [OpenCL],
[
	have_opencl="yes"
], [
	AC_CHECK_LIB(OpenCL, clGetPlatformInfo,
	[
		have_opencl="yes"
	], 	[
		have_opencl="no"
	])
])

PKG_CHECK_MODULES(MesaOpenCL, [MesaOpenCL],
[
	have_mesa_opencl="yes"
], [
	AC_CHECK_LIB(MesaOpenCL, clGetPlatformInfo,
	[
		have_mesa_opencl="yes"
	], 	[
		have_mesa_opencl="no"
	])
])

AC_MSG_CHECKING(whether to enable use of any found OpenCL)
AC_ARG_ENABLE(opencl, RB_HELP_STRING([--disable-opencl], [Disable OpenCL support]),
[
	AC_MSG_RESULT([$enableval])
	enable_opencl=$enableval
], [
	AC_MSG_RESULT([yes])
	enable_opencl="yes"
])

AM_CONDITIONAL([OPENCL], [test "x$have_opencl" = "xyes" && test "x$enable_opencl" = "xyes" ])
AM_CONDITIONAL([MESA_OPENCL], [test "x$have_mesa_opencl" = "xyes"])

AM_COND_IF([OPENCL],
[
	IRCD_DEFINE(USE_OPENCL, [1], [OpenCL support is available and enabled])
	OPENCL_LIBS+="-lOpenCL"

	AM_COND_IF([MESA_OPENCL],
	[
		IRCD_DEFINE(USE_MESA_OPENCL, [1], [MesaOpenCL support is available and enabled])
		OPENCL_LIBS+=" -lMesaOpenCL"
	])
])

dnl
dnl
dnl Arm NN
dnl
dnl

AC_SUBST(ARMNN_CPPFLAGS, [])
AC_SUBST(ARMNN_LDFLAGS, [])
AC_SUBST(ARMNN_LIBS, [])

AC_ARG_WITH(armnn-includes,
RB_HELP_STRING([--with-armnn-includes=[[[DIR]]]], [Path to Arm NN include directory]),
[
	armnn_incdir="$withval"
	ARMNN_CPPFLAGS="-isystem $withval"
], [])

AC_ARG_WITH(armnn-libs,
RB_HELP_STRING([--with-armnn-libs=[[[DIR]]]], [Path to Arm NN library directory]),
[
	armnn_libdir="$withval"
	ARMNN_LDFLAGS="-L$withval"
], [])

RB_CHK_SYSHEADER(armnn/ArmNN.hpp, [ARMNN_ARMNN_HPP])
PKG_CHECK_MODULES(armnn, [armnn],
[
	have_armnn="yes"
], [
	AC_CHECK_LIB(armnn, _ZN5armnn7RuntimeD1Ev,
	[
		have_armnn="yes"
	], 	[
		have_armnn="no"
	])
])

AC_MSG_CHECKING(whether to enable use of any found ArmNN)
AC_ARG_ENABLE(armnn, RB_HELP_STRING([--disable-armnn], [Disable Arm NN support]),
[
	AC_MSG_RESULT([$enableval])
	enable_armnn=$enableval
], [
	AC_MSG_RESULT([yes])
	enable_armnn="yes"
])

AM_CONDITIONAL([ARMNN], [test "x$have_armnn" = "xyes" && test "x$enable_armnn" = "xyes" ])

AM_COND_IF([ARMNN],
[
	IRCD_DEFINE(USE_ARMNN, [1], [Arm NN support is available and enabled])
	ARMNN_LIBS+="-larmnn"
])

dnl
dnl
dnl boost support
dnl
dnl

BOOST_VERSION_MIN="1.66"
BOOST_VERSION_MIN_PATCH="0"
BOOST_BUILT_LIBS="system,filesystem,context,coroutine,chrono"

AC_SUBST(BOOST_CPPFLAGS, [])
AC_SUBST(BOOST_LDFLAGS, [])
AC_SUBST(BOOST_LIBS, [])

AC_ARG_WITH(boost-includes,
RB_HELP_STRING([--with-boost-includes=[[[DIR]]]], [Path to boost include directory]),
[
	boost_incdir="$withval"
	BOOST_CPPFLAGS="-isystem $withval"
], [])

AC_ARG_WITH(boost-libs,
RB_HELP_STRING([--with-boost-libs=[[[DIR]]]], [Path to boost library directory]),
[
	boost_libdir="$withval"
	BOOST_LDFLAGS="-L$withval"
], [])

AC_MSG_CHECKING([whether you asked to use boost sources included here])
AC_ARG_WITH(included-boost,
RB_HELP_STRING([--with-included-boost[[[=shared]]]], [Use the boost sources from included submodule]),
[
	AC_MSG_RESULT([yes])
	AC_MSG_NOTICE([Bootstrapping boost])

	AM_COND_IF([DEBUG],
	[
		dnl boost_variant="debug"
		boost_variant="release"
	], [
		boost_variant="release"
	])

	AM_COND_IF([MINGW],
	[
		BTOOLSET="mingw"
		boost_libdir="$PWD/deps/boost/stage/lib"
	], [
		dnl the CXX may have a space and then arguments so we must chop them
		BTOOLSET=`echo $CXX | cut -d' ' -f1`
		boost_libdir="$PWD/deps/boost/lib"
	])

	BOOST_LDFLAGS="-L$boost_libdir"
	BOOST_CPPFLAGS="-isystem $PWD/deps/boost/include"

	AC_MSG_CHECKING([whether to use shared boost])
	if [[ $withval = "static" ]]; then
		AC_MSG_RESULT([no])
		withval="static"
	else
		AC_MSG_RESULT([yes])
		BOOST_LDFLAGS+=" -Wl,-rpath=$boost_libdir"
		withval="shared"
	fi

	boost_linkage="$withval"
	boost_version="boost-${BOOST_VERSION_MIN}.${BOOST_VERSION_MIN_PATCH}"
	boost_cxxflags="-std=gnu++17"
	bash tools/boostrap.sh $BTOOLSET $BOOST_BUILT_LIBS $boost_variant $withval multi $boost_version $boost_cxxflags
	AS_IF([ test $? != 0 ],
	[
		AC_MSG_ERROR([Failed to build included boost.])
	])

	AC_DEFINE(RB_INCLUDED_BOOST, 1, [Defined if configured with included boost])
	AC_MSG_NOTICE([Bootstrapping boost: done])
],[
	AC_MSG_RESULT([no])
	AX_BOOST_BASE([$BOOST_VERSION_MIN],
	[
		with_included_boost="no"
	], [
		with_included_boost="yes"
		AC_MSG_ERROR([Failed to find boost_system library. (try --with-included-boost)])
	])

	AX_BOOST_ASIO
	AX_BOOST_SYSTEM
	AX_BOOST_FILESYSTEM
	AX_BOOST_COROUTINE
	AX_BOOST_CONTEXT
	AX_BOOST_CHRONO
	boost_linkage="shared"
])

if [[ "$boost_linkage" = "shared" ]]; then
	AM_COND_IF([MINGW],
	[
		AC_SUBST(BOOST_LIBS, ["-lboost_coroutine-mgw53-1_61.dll -lboost_context-mgw53-1_61.dll -lboost_thread-mgw53-1_61.dll -lboost_filesystem-mgw53-1_61.dll -lboost_system-mgw53-1_61.dll"])
	], [
		AC_SUBST(BOOST_LIBS, ["-lboost_coroutine -lboost_context -lboost_thread -lboost_filesystem -lboost_chrono -lboost_system"])
	])
else
	AM_COND_IF([MINGW],
	[
		AC_SUBST(BOOST_LIBS, ["$boost_libdir/libboost_coroutine-mgw53-1_61.dll.a $boost_libdir/libboost_context-mgw53-1_61.dll.a $boost_libdir/libboost_thread-mgw53-1_61.dll.a $boost_libdir/libboost_filesystem-mgw53-1_61.dll.a $boost_libdir/libboost_system-mgw53-1_61.dll.a"])
	], [
		AC_SUBST(BOOST_LIBS, ["$boost_libdir/libboost_coroutine.a $boost_libdir/libboost_context.a $boost_libdir/libboost_thread.a $boost_libdir/libboost_filesystem.a $boost_libdir/libboost_system.a"])
	])
fi

dnl Units which require boost::asio use these flags. This includes the
dnl ircd/asio.h PCH, which includes ircd.h upstream in the precompile.
AC_SUBST(ASIO_UNIT_CPPFLAGS)
ASIO_UNIT_CPPFLAGS+=" $BOOST_CPPFLAGS"
ASIO_UNIT_CPPFLAGS+=" $SSL_CPPFLAGS"
ASIO_UNIT_CPPFLAGS+=" $CRYPTO_CPPFLAGS"
ASIO_UNIT_CPPFLAGS+=" -include ircd/asio.h"

dnl Units defining spirit grammars may benefit from special compiler flags due
dnl to their various complexities. The ./configure may have generated flags
dnl in $GRAMMAR_UNIT_CXXFLAGS to improve compile time and reduce debug symbol.
AC_SUBST(SPIRIT_UNIT_CPPFLAGS)
SPIRIT_UNIT_CPPFLAGS+=" $ICU_CPPFLAGS"
SPIRIT_UNIT_CPPFLAGS+=" $BOOST_CPPFLAGS"
SPIRIT_UNIT_CPPFLAGS+=" -include ircd/spirit.h"

AC_SUBST(SPIRIT_UNIT_CXXFLAGS)

AM_COND_IF([GCC],
[
	SPIRIT_UNIT_CXXFLAGS+=" -fno-var-tracking"
	SPIRIT_UNIT_CXXFLAGS+=" -fno-var-tracking-assignments"
	SPIRIT_UNIT_CXXFLAGS+=" -femit-struct-debug-baseonly"

	AM_COND_IF([LOWMEM_COMPILE],
	[
		SPIRIT_UNIT_CXXFLAGS+=" --param ggc-min-expand=1"
		SPIRIT_UNIT_CXXFLAGS+=" -fno-ipa-pta"
	])
])

dnl
dnl
dnl RocksDB support
dnl
dnl

ROCKSDB_VERSION_DEFAULT="v6.6.4"

AC_SUBST(ROCKSDB_CPPFLAGS, [])
AC_SUBST(ROCKSDB_LDFLAGS, [])
AC_SUBST(ROCKSDB_LIBS, [])

AC_ARG_WITH(rocksdb-includes,
RB_HELP_STRING([--with-rocksdb-includes=[[[DIR]]]], [Path to RocksDB include directory]),
[
	ROCKSDB_CPPFLAGS="-isystem $withval"
], [])

AC_ARG_WITH(rocksdb-libs,
RB_HELP_STRING([--with-rocksdb-libs=[[[DIR]]]], [Path to RocksDB library directory]),
[
	ROCKSDB_LDFLAGS="-L$withval"
], [])

AC_MSG_CHECKING([whether you asked to use the RocksDB included here])
AC_ARG_WITH(included-rocksdb,
RB_HELP_STRING([--with-included-rocksdb[[[=version]]]], [Use the RocksDB sources from included submodule]),
[
	AC_MSG_RESULT([yes])
	with_included_rocksdb="yes"

	AC_MSG_NOTICE([Shared RocksDB linkage requires running charybdis with an intact build directory])
	ROCKSDB_CPPFLAGS="-isystem $PWD/deps/rocksdb/include"
	ROCKSDB_LDFLAGS="-L$PWD/deps/rocksdb/"
	ROCKSDB_LDFLAGS+=" -Wl,-rpath=$PWD/deps/rocksdb"
	ROCKSDB_LIBS="-lrocksdb"

	rocksdb_linkage="shared_lib"
	rocksdb_version="$withval"
	if test x"$rocksdb_version" == x"yes"; then
		rocksdb_version="$ROCKSDB_VERSION_DEFAULT"
	fi

	bash tools/buildrocks.sh $rocksdb_linkage $rocksdb_version
	AS_IF([ test $? != 0 ],
	[
		AC_MSG_ERROR([Failed to build RocksDB])
	])
],[
	AC_MSG_RESULT([no])
	with_included_rocksdb="no"

	PKG_CHECK_MODULES(rocksdb, [rocksdb],
	[
		ROCKSDB_LIBS="-lrocksdb"
	], [
		AC_CHECK_LIB(rocksdb, rocksdb_open,
		[
			ROCKSDB_LIBS="-lrocksdb"
		], [
			AC_MSG_ERROR([Unable to find required RocksDB package. Try apt-get install librocksdb-dev])
		])
	])
])

dnl Units which include rocksdb headers use these flags.
AC_SUBST(ROCKSDB_UNIT_CPPFLAGS)
ROCKSDB_UNIT_CPPFLAGS+=" $ROCKSDB_CPPFLAGS"
ROCKSDB_UNIT_CPPFLAGS+=" $ZSTD_CPPFLAGS"
ROCKSDB_UNIT_CPPFLAGS+=" $SNAPPY_CPPFLAGS"
ROCKSDB_UNIT_CPPFLAGS+=" $LZ4_CPPFLAGS"
ROCKSDB_UNIT_CPPFLAGS+=" $Z_CPPFLAGS"

dnl
dnl
dnl SpiderMonkey support
dnl
dnl

AC_MSG_CHECKING([whether you asked to use the JS engine included here])
AC_ARG_WITH(included-js,
RB_HELP_STRING([--with-included-js[[[=shared]]]], [Use the JS engine (SpiderMonkey) sources we include here]),
[
	AC_MSG_RESULT([yes])
	with_included_js="yes"

	AC_SUBST(JS_CPPFLAGS)
	JS_CPPFLAGS+=" -isystem $PWD/deps/gecko-dev/js/src/build_OPT.OBJ/dist/include"
	#JS_CPPFLAGS+=" -I$PWD/deps/gecko-dev/js/src/build_OPT.OBJ/dist/include"

	AC_SUBST(JS_LDFLAGS)
	JS_LDFLAGS+=" -L$PWD/deps/gecko-dev/js/src/build_OPT.OBJ/dist/sdk/lib"
	JS_LDFLAGS+=" -L$PWD/deps/gecko-dev/js/src/build_OPT.OBJ/intl/icu/target/lib"
	JS_LDFLAGS+=" -L$PWD/deps/gecko-dev/js/src/build_OPT.OBJ/js/src"

	AC_SUBST(JS_LIBS)
	JS_LIBS+=" -lmozjs-68"
	JS_LIBS+=" -lmozglue"

	dnl !!!!
	dnl HACK BUG-WORKAROUND - Mozilla forgot to include this in their lib?
	dnl Runtime segfault (unresolved dynamic function address) if this is not specified
	#JS_LIBS+=" $PWD/deps/gecko-dev/js/src/build_OPT.OBJ/mfbt/Unified_cpp_mfbt0.o"
	#JS_LIBS+=" $PWD/deps/gecko-dev/js/src/build_OPT.OBJ/mfbt/Unified_cpp_mfbt1.o"
	JS_LIBS+=" $PWD/deps/gecko-dev/js/src/build_OPT.OBJ/mozglue/misc/TimeStamp.o"

	AC_MSG_CHECKING([whether to use shared JS engine])
	if [[ $withval = "shared" ]]; then
		AC_MSG_RESULT([yes])
		AC_MSG_NOTICE([Shared SpiderMonkey linkage requires running charybdis with an intact build directory])
		js_linkage="shared"
		JS_LDFLAGS+=" -Wl,-rpath=$PWD/deps/gecko-dev/js/src/build_OPT.OBJ/dist/sdk/lib"
		JS_LDFLAGS+=" -Wl,-rpath=$PWD/deps/gecko-dev/js/src/build_OPT.OBJ/intl/icu/target/lib"
	else
		AC_MSG_RESULT([no])
		js_linkage="static"
		JS_LIBS+=" -ljs_static"
	fi

	js_branch="esr68"

	if [[ $DEBUG ]]; then
		js_options="--disable-optimize --enable-debug --disable-js-shell"
	else
		js_options="--enable-optimize --disable-js-shell"
	fi

	bash tools/buildjs.sh "$js_branch" "$js_options"

	RB_DEFINE_UNQUOTED([ENABLE_JS], [1], [ Enable JavaScript support. ])
	RB_DEFINE_UNQUOTED([INC_JSAPI_H], [jsapi.h>], [ SpiderMonkey JavaScript API. ])
	RB_DEFINE_UNQUOTED([INC_JSFRIENDAPI_H], [jsfriendapi.h>], [ SpiderMonkey JavaScript Extended API. ])
	RB_DEFINE_UNQUOTED([INC_JS_CONVERSIONS_H], [js/Conversions.h>], [ SpiderMonkey JavaScript Conversions. ])
],[
	AC_MSG_RESULT([no])
	with_included_js="no"

	dnl TODO mumble mumble
	AC_CHECK_LIB(mozjs-68, _Z11JS_ShutDownv,
	[

	], [
		AC_MSG_WARN([Unable to find JS engine (SpiderMonkey 68) package. Try apt-get install libmozjs-dev])
	])

	dnl TODO
	dnl AC_SUBST(JS_CPPFLAGS, ["-I/usr/include/mozjs-68"])
	dnl AC_SUBST(JS_LDFLAGS, [])
	dnl AC_SUBST(JS_LIBS, ["-lmozjs-68"])
])

dnl TODO use an $enable_js var
AM_CONDITIONAL([JS], [[[[ $with_included_js = yes ]]]])


dnl ***************************************************************************
dnl
dnl Application settings
dnl

dnl
dnl --with-mxid-maxlen
dnl

AC_ARG_WITH(mxid-maxlen,
RB_HELP_STRING([--with-mxid-maxlen=LENGTH],[Set the upper-bound mxid length to LENGTH (default 255, max 511)]),
[
  if ! expr "$withval" + 0 >/dev/null 2>&1; then
	AC_ERROR([MXID_MAXLEN must be a numeric value])
  fi
  if test $withval -gt 511; then
	MXID_MAXLEN=511
	AC_MSG_WARN([MXID_MAXLEN has a hard limit of 511. Setting MXID_MAXLEN=511])
  elif test $withval -lt 4; then
	MXID_MAXLEN=4
	AC_MSG_WARN([MXID_MAXLEN has a lower limit of 4. Setting MXID_MAXLEN=4])
  else
	MXID_MAXLEN="$withval"
  fi
], [
	MXID_MAXLEN=255
])

RB_DEFINE_UNQUOTED([MXID_MAXLEN], [$MXID_MAXLEN], [Maximum string length of an MXID (not including null)])


dnl ***************************************************************************
dnl
dnl Branding defined in the compilation
dnl

VERSION_COMMIT_CMD="git rev-parse --short HEAD"
RB_VERSION_COMMIT=`$VERSION_COMMIT_CMD`
RB_DEFINE_UNQUOTED([VERSION_COMMIT], ["$RB_VERSION_COMMIT"], [Git commit at configuration time.])
#CPPDEFINE([RB_VERSION_COMMIT], [`$VERSION_COMMIT_CMD`])

VERSION_BRANCH_CMD="git rev-parse --abbrev-ref HEAD"
RB_VERSION_BRANCH=`$VERSION_BRANCH_CMD`
RB_DEFINE_UNQUOTED([VERSION_BRANCH], ["$RB_VERSION_BRANCH"], [Git branch at configuration time.])
#CPPDEFINE([RB_VERSION_BRANCH], [`$VERSION_BRANCH_CMD`])

VERSION_TAG_CMD="git describe --tags --abbrev=0 --dirty --always --broken"
RB_VERSION_TAG=`$VERSION_TAG_CMD`
RB_DEFINE_UNQUOTED([VERSION_TAG], ["$RB_VERSION_TAG"], [Git tag at configuration time.])
#CPPDEFINE([RB_VERSION_TAG], [`$VERSION_TAG_CMD`])

VERSION_CMD="git describe --tags --always --broken"
RB_VERSION=`$VERSION_CMD`
RB_DEFINE_UNQUOTED([VERSION], ["$RB_VERSION"], [Version generated at configuration time.])

DATESTR_CMD="date"
RB_DATESTR=`$DATESTR_CMD`
RB_DEFINE_UNQUOTED([DATESTR], ["$RB_DATESTR"], [Readable date string of configuration time.])

DATECODE_CMD="date +%s"
RB_DATECODE=`$DATECODE_CMD`
RB_DEFINE_UNQUOTED([TIME_CONFIGURED], [$RB_DATECODE], [UNIX epoch time at configuration time.])
RB_DEFINE_UNQUOTED([DATE_CONFIGURED], ["$RB_DATESTR"], [Convenience string of RB_TIME_CONFIGURED])

dnl
dnl --with-custom-branding
dnl

AC_MSG_CHECKING([whether custom branding is requested])
AC_ARG_WITH(custom-branding, RB_HELP_STRING([--with-custom-branding=NAME], [Custom branding name]),
[
	BRANDING_NAME="$withval"
	AC_DEFINE_UNQUOTED(BRANDING_NAME, ["$BRANDING_NAME"], [Custom branding name.])
	AC_DEFINE(CUSTOM_BRANDING, 1, [Define if custom branding is enabled.])
	AC_MSG_RESULT([yes])
],[
	BRANDING_NAME="$PACKAGE_NAME"
	AC_DEFINE_UNQUOTED(BRANDING_NAME, ["$BRANDING_NAME"], [Current package])
	AC_MSG_RESULT([no])
])

dnl
dnl --with-custom-version
dnl

AC_MSG_CHECKING([whether a custom version is requested])
AC_ARG_WITH(custom-version, RB_HELP_STRING([--with-custom-version=NAME], [Custom version branding]),
[
	BRANDING_VERSION="$withval"
	AC_DEFINE_UNQUOTED(BRANDING_VERSION, ["$BRANDING_VERSION"], [Custom branding version.])
	AC_MSG_RESULT([yes])
], [
	BRANDING_VERSION="$RB_VERSION"
	AC_DEFINE_UNQUOTED(BRANDING_VERSION, ["$BRANDING_VERSION"], [Current version])
	AC_MSG_RESULT([no])
])


dnl ***************************************************************************
dnl
dnl Output
dnl

AC_OUTPUT

dnl Make it look sexay!
echo
echo "Configured ........................ $PACKAGE_NAME $PACKAGE_VERSION"
echo "Configured time ................... $RB_DATESTR"
echo
echo "C++ Compiler ...................... $CXX"
echo "C++ Compiler flags (CXXFLAGS) ..... $CXXFLAGS"
echo
echo "Linker ............................ $LD"
echo "Linker flags (LDFLAGS) ............ $LDFLAGS"
echo
echo "Target platform (host) ............ $host_cpu $host_vendor $host_os"
echo "Developer platform (build) ........ $build_cpu $build_vendor $build_os"
echo "Developer debug ................... $debug"
echo "Developer assert .................. $assert ($assert_type)"
echo "Developer profiling ............... $profiling"
echo "Precompiled headers ............... $build_pch"
echo "Optimized compile ................. $optimize"
echo "Optimized linking ................. $lto"
echo "Optimized runtime ................. $profiling_used"
echo "Optimization level ................ $optimize_level"
echo "Machine tuned ..................... $machine_tuning"
echo
echo "Magic support ..................... $have_magic"
echo "ImageMagick support ............... $have_imagemagick"
echo "PNG support ....................... $have_png"
echo "Unicode (icuuc) support ........... $have_icuuc"
echo "Ziplinks (libz) support ........... $have_zlib"
echo "Zstandard (zstd) support .......... $have_zstd"
echo "Snappy support .................... $have_snappy"
echo "LZ4 support ....................... $have_lz4"
echo "SSL support ....................... $have_ssl"
echo "Crypto support .................... $have_crypto"
echo "Sodium support .................... $have_sodium"
echo "GNU MP support .................... $have_gmp"
echo "PBC support ....................... $have_pbc"
echo "LLVM library support .............. $have_libllvm"
echo "ARM NN support .................... $have_armnn"
echo "OpenCL support .................... $have_opencl"
echo "MesaOpenCL support ................ $have_mesa_opencl"
echo "Linux AIO support ................. $aio"
echo "Linux io_uring support ............ $io_uring"
echo "Memory allocator .................. $alloc_lib"
echo
echo "Using bundled Boost ............... $with_included_boost"
echo "Using bundled RocksDB ............. $with_included_rocksdb"
echo
echo "Install Prefix .................... $prefix"
echo "Binary directory .................. $bindir"
echo "Config directory .................. $sysconfdir"
echo "Include directory ................. $includedir"
echo "Library directory ................. $libdir"
echo "Modules directory ................. $moduledir"
echo "Shared dataroot directory ......... $datarootdir"
echo "Shared data directory ............. $datadir"
echo "Webapp directory .................. $webappdir"
echo "Runtime state directory ........... $runstatedir"
echo "Database directory ................ $dbdir"
echo "Logging directory ................. $logdir"
echo
echo "Matrix ID (mxid) length ........... $MXID_MAXLEN"
echo "Logging level ..................... $LOG_LEVEL"
echo "Branding .......................... $BRANDING_NAME $BRANDING_VERSION"
echo
echo "* Ready to build $PACKAGE_NAME"
echo "* Now run 'make'"
