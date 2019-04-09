AC_PREREQ(2.63)
AUTOMAKE_OPTIONS = 1.11

AC_INIT([construct], [1.0-dev])
AC_GNU_SOURCE
AC_USE_SYSTEM_EXTENSIONS
AC_PREFIX_DEFAULT([/])
AC_CONFIG_MACRO_DIR([tools/m4])

AC_PATH_PROG(AUTOMAKE, automake)
AC_PATH_PROG(ACLOCAL, aclocal)
AC_PATH_PROG(AUTOHEADER, autoheader)
AC_PATH_PROG(AS, as)
AC_PATH_PROG(RM, rm)
AC_PATH_PROG(CP, cp)
AC_PATH_PROG(MV, mv)
AC_PATH_PROG(LN, ln)
AC_PATH_PROG(TOUCH, touch)

LT_INIT([dlopen shared disable-static])
LT_LANG([C++])

CXXFLAGS="-pipe"
AC_PROG_CXX
AC_PROG_CXX_C_O
AC_PROG_LIBTOOL
AC_PROG_MAKE_SET
AC_LANG(C++)
AC_CONFIG_HEADER(include/ircd/config.h)
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
	modules/Makefile        \
	share/Makefile          \
)

AM_INIT_AUTOMAKE([subdir-objects])

dnl
dnl Initialization of variables exported to Makefile.am.
dnl These will be further composed throughout this file...
dnl

AC_SUBST(CC)
AC_SUBST(CXX)
AC_SUBST(CXXFLAGS)
AC_SUBST(CPPFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(LIBS)

dnl
dnl Preprocessor define macros
dnl
dnl Use RB_DEFINE() et al to generate defines in config.h
dnl Use CPPDEFINE() to generate defines on the command line if required
dnl

AC_DEFUN([RB_DEFINE],
[
	AC_DEFINE([RB_$1], [$2], [$3])
])

AC_DEFUN([RB_DEFINE_UNQUOTED],
[
	AC_DEFINE_UNQUOTED([RB_$1], [$2], [$3])
])

AC_DEFUN([IRCD_DEFINE],
[
	AC_DEFINE([IRCD_$1], [$2], [$3])
])

AC_DEFUN([IRCD_DEFINE_UNQUOTED],
[
	AC_DEFINE_UNQUOTED([IRCD_$1], [$2], [$3])
])

AC_DEFUN([CPPDEFINE],
[
	if [[ -z "$2" ]]; then
		CPPFLAGS="-D$1 $CPPFLAGS"
	else
		CPPFLAGS="-D$1=$2 $CPPFLAGS"
	fi
])

dnl ***************************************************************************
dnl
dnl Platform
dnl

dnl
dnl Recursive local targets (clean-local is implied)
dnl

#AM_EXTRA_RECURSIVE_TARGETS([
#	mrproper
#])

dnl
dnl Coarse compiler test macros
dnl

AM_CONDITIONAL([GCC],        [[[[ $CXX = g\+\+* ]]]])
AM_CONDITIONAL([CLANG],      [[[[ $CXX = clang* ]]]])

RB_DEFINE_UNQUOTED([CXX], ["$CXX"], [CXX Compiler])

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

dnl ***************************************************************************
dnl
dnl Developer options
dnl

dnl
dnl Debugging mode
dnl

AC_MSG_CHECKING(whether to enable debugging)
AC_ARG_ENABLE(debug, AC_HELP_STRING([--enable-debug], [Enable debugging suite for development.]),
[
	AC_MSG_RESULT([yes])
	AC_SUBST(DEBUG, 1)
	RB_DEFINE_UNQUOTED([DEBUG], [1], [Not configured for release when lit.])
	RB_DEFINE_UNQUOTED([DEBUG_LEVEL], [1], [Defined to 0 for release; or > 0 otherwise])
], [
	AC_MSG_RESULT([no])
	RB_DEFINE_UNQUOTED([DEBUG_LEVEL], [0], [Defined to 0 for release])
])

AM_CONDITIONAL([DEBUG], [[[[ "$DEBUG" = "1" ]]]])

dnl
dnl Explicit assert switch for still using assert() without --enable-debug
dnl

AC_MSG_CHECKING(whether to explicitly enable assertions)
AC_ARG_ENABLE(assert, AC_HELP_STRING([--enable-assert], [Enable assertions without --enable-debug]),
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
], [
	assert="no"
	CPPDEFINE([NDEBUG])
])

dnl
dnl Switch to control the action of assert()
dnl

AC_MSG_CHECKING(whether to change the behavior of assertions)
AC_ARG_WITH(assert, AC_HELP_STRING([--with-assert[[[=trap]]]], [Soften assertion behavior]),
[
	assert_type=$withval
	AC_MSG_RESULT([yes])
	AC_SUBST(ASSERT_TYPE, $assert_type)
], [
	AM_COND_IF(DEBUG,
	[
		assert_type="trap"
		AC_MSG_RESULT([no, defaulting to "$assert_type"])
		AC_SUBST(ASSERT_TYPE, $assert_type)
	], [
		AC_MSG_RESULT([no])
	])
])

AM_COND_IF(ASSERT,
[
	if [[ ! -z "$assert_type" ]]; then
		RB_DEFINE_UNQUOTED([ASSERT], ["$assert_type"], [Assertion behavior])
	fi
])

dnl
dnl Explicit optimize switch for enabling optimization when using --enable-debug
dnl

AC_MSG_CHECKING(whether to explicitly enable optimized build)
AC_ARG_ENABLE(optimize, AC_HELP_STRING([--enable-optimize], [Enable optimization even with --enable-debug]),
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
AC_ARG_ENABLE(lto, AC_HELP_STRING([--enable-lto], [Enable link-time optimization]),
[
	AC_MSG_RESULT([yes])
	AC_SUBST(LTO, 1)
], [
	AM_COND_IF(DEBUG,
	[
		AC_MSG_RESULT([no])
	], [
		AC_MSG_RESULT([no, link-time optimization is explicit-only at this time])
	])
])

AM_CONDITIONAL([LTO], [[[[ "$LTO" = "1" ]]]])

dnl
dnl Explicit optimization level switch
dnl

AC_MSG_CHECKING(whether to explicitly set the optimization level)
AC_ARG_WITH(optimize-level, AC_HELP_STRING([--with-optimize-level[[[=3]]]], [Explicitly set the optimize level]),
[
	optimize_level=$withval
	AC_MSG_RESULT([yes, enabling optimization level -O$optimize_level])
	AC_SUBST(OPTMIZE_LEVEL, $optimize_level)
	RB_DEFINE_UNQUOTED([OPTIMIZE_LEVEL], [$optimize_level], [User configured optimization level])
], [
	AM_COND_IF(OPTIMIZE,
	[
		optimize_level="3"
		AC_MSG_RESULT([no, but --enable-optimize has implied level -O$optimize_level])
		AC_SUBST(OPTIMIZE_LEVEL, $optimize_level)
		RB_DEFINE_UNQUOTED([OPTIMIZE_LEVEL], [$optimize_level], [Explicitly set release mode optimization])
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
AC_ARG_WITH(log-level, AC_HELP_STRING([--with-log-level[[[=INFO]]]], [Explicitly set the log level ceiling]),
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

AC_MSG_CHECKING(whether to do a profile build)
AC_ARG_ENABLE(profile, AC_HELP_STRING([--enable-profile], [Enable profiling]),
[
	if test "$ac_cv_c_compiler_gnu" = yes; then
		AC_MSG_RESULT([yes, adding -pg])
		CXXFLAGS+=" -pg"
		AC_DEFINE(CHARYBDIS_PROFILE, 1, [Define this if you are profiling.])
	else
		AC_MSG_RESULT([no, profile builds only work with gcc])
	fi
], [
])

dnl
dnl Precompiled headers
dnl

AC_MSG_CHECKING(whether to disable precompiled headers)
AC_ARG_ENABLE(pch, AC_HELP_STRING([--disable-pch], [Disable precompiled header generation]),
[
	build_pch="no"
	AC_MSG_RESULT([no])
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
AC_ARG_ENABLE(generic, AC_HELP_STRING([--enable-generic], [Emit more generic code for pkg binaries.]),
[
	enable_generic="yes"
	AC_MSG_RESULT([yes])
	RB_DEFINE([GENERIC], [1], [Building binary tuned for generic architectures])
], [
	enable_generic="no"
	AC_MSG_RESULT([no])
])

AM_CONDITIONAL([GENERIC], [[[[ "$enable_generic" = "yes" ]]]])

AM_COND_IF([GENERIC],
[
	CXXFLAGS+=" -mtune=generic"
], [
	CXXFLAGS+=" -march=native"
])

dnl  !!! Experimental !!!
dnl
dnl  Allows a signal handler to throw a C++ exception if the signal is from
dnl  a CPU trap on an instruction (ex. SIGFPE from a floating point divide
dnl  by zero). This allows the signal handler to transfer control to the
dnl  catch block wrapping the area which errored, thereby aborting any further
dnl  execution after the signal was received which would have to manually check
dnl  for such an error after each floating point operation and throw.

AC_ARG_ENABLE(asyncexcept, AC_HELP_STRING([--enable-asyncexcept], [Enable experimental asynchronous exceptions]),
[
	asyncexcept="yes"
], [
	asyncexcept="no"
])

dnl CHARYBDIS_C_GCC_TRY_FLAGS([-fnon-call-exceptions], charybdis_cv_c_gcc_f_non_call_exceptions)

if test "$asyncexcept" = "yes"; then
	IRCD_DEFINE(USE_ASYNC_EXCEPTIONS, [1], [Experimental asynchronous exceptions are supported])
	CXXFLAGS+=" -fasynchronous-unwind-tables"
	CXXFLAGS+=" -fnon-call-exceptions"
fi

AM_CONDITIONAL([ASYNCEXCEPT], [[[[ $asyncexcept = "yes" ]]]])

dnl
dnl Low memory compilation mode
dnl

AC_ARG_ENABLE(lowmem-compile, AC_HELP_STRING([--enable-lowmem-compile], [Enable low-memory compilation options]),
[
	lowmem_compile="yes"
], [
	lowmem_compile="no"
])

AM_CONDITIONAL([LOWMEM_COMPILE], [[[[ $lowmem_compile = "yes" ]]]])

dnl
dnl Optimization
dnl

CXXFLAGS+=" -O$optimize_level"

AM_COND_IF([LTO],
[
	lto="yes"
	CXXFLAGS+=" -flto=`nproc`"
], [
	lto="no"
])

AM_COND_IF([OPTIMIZE],
[
	optimize="yes"
	CXXFLAGS+=" -fweb"
	CXXFLAGS+=" -fdata-sections"
	CXXFLAGS+=" -ffunction-sections"
	CXXFLAGS+=" -ftree-vectorize"
	CXXFLAGS+=" -fgcse-sm"
	CXXFLAGS+=" -fgcse-las"
	CXXFLAGS+=" -fgcse-after-reload"
	CXXFLAGS+=" -fsched-stalled-insns=0"
	CXXFLAGS+=" -frename-registers"
	CXXFLAGS+=" -fira-hoist-pressure"
	CXXFLAGS+=" -fbranch-target-load-optimize"
	CXXFLAGS+=" -frerun-loop-opt"
	CXXFLAGS+=" -fdevirtualize-at-ltrans"

	dnl Note these flags are unreliable with LTO
	CXXFLAGS+=" -g"
	CXXFLAGS+=" -ggdb"
], [
	optimize="no"
	CXXFLAGS+=" -g"
	CXXFLAGS+=" -ggdb"
	CXXFLAGS+=" -gdwarf-4"
	CXXFLAGS+=" -fvar-tracking"
	CXXFLAGS+=" -fvar-tracking-assignments"
])

dnl
dnl Compiler intrumentation
dnl

dnl  Creates an ELF section in the output which contains a list of
dnl  the command line arguments used for compilation.
CXXFLAGS+=" -frecord-gcc-switches"

dnl CXXFLAGS+=" -mmpx"
dnl CXXFLAGS+=" -fcheck-pointer-bounds"
CXXFLAGS+=" -fchkp-instrument-marked-only"
CXXFLAGS+=" -fstack-protector-explicit"

dnl  -fvisibility-inlines-hidden - for optimization; note the address of inline
dnl  functions won't be the same between translation units
CXXFLAGS+=" -fvisibility-inlines-hidden"

dnl
dnl Compiler warnings
dnl

STACK_USAGE_WARNING=16384
FRAME_SIZE_WARNING=8192
CWARNS=""
AC_MSG_CHECKING(whether to disable warnings)
AC_ARG_ENABLE(warnings,
AC_HELP_STRING([--disable-warnings],[Disable all sorts of warnings like a rockstar]),
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
CHARYBDIS_C_GCC_TRY_FLAGS([-Wmissing-noreturn], charybdis_cv_c_gcc_w_missing_noreturn)
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
CHARYBDIS_C_GCC_TRY_FLAGS([-Wchkp], charybdis_cv_c_gcc_w_chkp)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wsized-deallocation], charybdis_cv_c_gcc_w_sized_deallocation)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wctor-dtor-privacy], charybdis_cv_c_gcc_w_ctor_dtor_privacy)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wsign-promo], charybdis_cv_c_gcc_w_sign_promo)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wsuggest-final-types], charybdis_cv_c_gcc_w_suggest_final_types)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wsuggest-final-methods], charybdis_cv_c_gcc_w_suggest_final_methods)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wsuggest-override], charybdis_cv_c_gcc_w_suggest_override)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wtrampolines], charybdis_cv_c_gcc_w_trampolines)
CHARYBDIS_C_GCC_TRY_FLAGS([-Wduplicate-cond], charybdis_cv_c_gcc_w_duplicate_cond)
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

dnl This warns when the compiler sees a statement that 'throws into a noexcept' which
dnl which will certainly terminate the program; it wants us to use std::terminate()
dnl rather than the rethrow. Unless throwing into a noexcept that way has UB in an
dnl optimized build I really don't care and just want to type `throw;` thx.
CHARYBDIS_C_GCC_TRY_FLAGS([-Wno-terminate], charybdis_cv_c_gcc_w_terminate)

dnl
dnl List of compiler-specific / incompatible warning options
dnl

AM_COND_IF([GCC],
[
	CHARYBDIS_C_GCC_TRY_FLAGS([-Wlogical-op], charybdis_cv_c_gcc_w_logical_op)
	CHARYBDIS_C_GCC_TRY_FLAGS([-Wformat-security], charybdis_cv_c_gcc_w_format_security)
	CHARYBDIS_C_GCC_TRY_FLAGS([-Wsuggest-attribute=format], charybdis_cv_c_gcc_w_suggest_attribute_format)
	CHARYBDIS_C_GCC_TRY_FLAGS([-Wstack-usage=$STACK_USAGE_WARNING], charybdis_cv_c_gcc_w_stack_usage)
	CHARYBDIS_C_GCC_TRY_FLAGS([-Wframe-larger-than=$FRAME_SIZE_WARNING], charybdis_cv_c_gcc_w_frame_larger_than)
],[])

AM_COND_IF([CLANG],
[
	CHARYBDIS_C_GCC_TRY_FLAGS([-Wno-mismatched-tags], charybdis_cv_c_gcc_w_mismatched_tags)
	CHARYBDIS_C_GCC_TRY_FLAGS([-Wno-format-security], charybdis_cv_c_gcc_w_format_security)
],[])

])

CPPFLAGS+="$CWARNS"


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
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UID_T
AC_TYPE_PID_T

AC_CHECK_TYPES([intptr_t])
AC_CHECK_TYPES([uintptr_t])
AC_CHECK_TYPES([int128_t])
AC_CHECK_TYPES([uint128_t])
AC_CHECK_TYPES([__int128])
AC_CHECK_TYPES([__int128_t])
AC_CHECK_TYPES([__uint128_t])
AC_CHECK_TYPES([long double])

AC_CHECK_SIZEOF(char)
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(long long)
AC_CHECK_SIZEOF(float)
AC_CHECK_SIZEOF(double)
AC_CHECK_SIZEOF(long double)
AC_CHECK_SIZEOF(int128_t)
AC_CHECK_SIZEOF(uint128_t)
AC_CHECK_SIZEOF(__int128)
AC_CHECK_SIZEOF(__int128_t)
AC_CHECK_SIZEOF(__uint128_t)

dnl -fsigned-char ensures 'char' is signed on all platforms
CXXFLAGS+=" -fsigned-char"

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
RB_CHK_SYSHEADER(limits, [LIMITS])
RB_CHK_SYSHEADER(type_traits, [TYPE_TRAITS])
RB_CHK_SYSHEADER(typeindex, [TYPEINDEX])
RB_CHK_SYSHEADER(variant, [VARIANT])
RB_CHK_SYSHEADER(utility, [UTILITY])
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

dnl linux platform
RB_CHK_SYSHEADER(malloc.h, [MALLOC_H])
RB_CHK_SYSHEADER(sys/auxv.h, [SYS_AUXV_H])
RB_CHK_SYSHEADER(sys/eventfd.h, [SYS_EVENTFD_H])
RB_CHK_SYSHEADER(sys/inotify.h, [SYS_INOTIFY_H])
RB_CHK_SYSHEADER(sys/sysmacros.h, [SYS_SYSMACROS_H])
RB_CHK_SYSHEADER(linux/aio_abi.h, [LINUX_AIO_ABI_H])
RB_CHK_SYSHEADER(linux/magic.h, [LINUX_MAGIC_H])
RB_CHK_SYSHEADER(linux/perf_event.h, [LINUX_PERF_EVENT_H])
RB_CHK_SYSHEADER(linux/hw_breakpoint.h, [LINUX_HW_BREAKPOINT_H])

dnl windows platform
RB_CHK_SYSHEADER(windows.h, [WINDOWS_H])
RB_CHK_SYSHEADER(winsock2.h, [WINSOCK2_H])
RB_CHK_SYSHEADER(ws2tcpip.h, [WS2TCPIP_H])
RB_CHK_SYSHEADER(iphlpapi.h, [IPHLPAPI_H])

dnl x86 platform
RB_CHK_SYSHEADER(x86intrin.h, [X86INTRIN_H])
RB_CHK_SYSHEADER(cpuid.h, [CPUID_H])

dnl experimental
RB_CHK_SYSHEADER(experimental/string_view, [EXPERIMENTAL_STRING_VIEW])
RB_CHK_SYSHEADER(experimental/optional, [EXPERIMENTAL_OPTIONAL])

dnl
dnl Specific function checks
dnl

AC_CHECK_FUNCS([        \
	strlcpy             \
	strlcat             \
	strnlen             \
	snprintf            \
	vsnprintf           \
	posix_fadvise       \
	pwritev2            \
	preadv2             \
])

AC_SEARCH_LIBS(dlinfo, dl, AC_DEFINE(HAVE_DLINFO, 1, [Define if you have dlinfo]))
AC_SEARCH_LIBS(nanosleep, rt posix4, AC_DEFINE(HAVE_NANOSLEEP, 1, [Define if you have nanosleep]))

dnl
dnl Networking Functions
dnl

AC_ARG_ENABLE(ipv6, AC_HELP_STRING([--disable-ipv6], [Disable IPv6 support]),
[
	ipv6="no"
], [
	ipv6="yes"
])

AM_CONDITIONAL([HAVE_IPV6], [[[[ "$ipv6" = "yes" ]]]])

AM_COND_IF(HAVE_IPV6,
[
	AC_DEFINE([HAVE_IPV6], [1], [IPv6 is supported])
],[])


dnl ***************************************************************************
dnl
dnl Miscellaneous operating system specific
dnl

AM_COND_IF(MINGW,
[
	AC_DEFINE(HAVE_WIN32, [1], [Define to 1 if you are on windows])
	LIBS="$LIBS -lws2_32 -liphlpapi"
],[])


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
	AC_ARG_ENABLE(aio, AC_HELP_STRING([--disable-aio], [Disable kernel AIO support]),
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


dnl ***************************************************************************
dnl
dnl Installation Layout
dnl

PKG_PROG_PKG_CONFIG

if test "$prefix" = "NONE"; then
	prefix="$ac_default_prefix"
else
	dnl Don't get bitten by Cygwin's stupidity if the user specified
	dnl a custom prefix with a trailing slash
	prefix=`echo $prefix | sed 's/\/$//'`
fi

RB_DEFINE_UNQUOTED(PREFIX, ["$prefix"], [Prefix of IRCd install])
AC_SUBST_DIR([prefix])

dnl
dnl bindir
dnl

AC_SUBST_DIR([bindir])
RB_DEFINE_UNQUOTED([BIN_DIR], ["$bindir"], [Directory where binary executables are to be found.])

dnl
dnl libdir
dnl

AC_SUBST_DIR([libdir])
RB_DEFINE_UNQUOTED([LIB_DIR], ["$libdir"], [Prefix where libs are installed.])

dnl
dnl localstatedir
dnl

dnl Avoid name collisions.
AC_SUBST_DIR([localstatedir])
RB_DEFINE_UNQUOTED([LOCALSTATE_DIR], ["$localstatedir"], [Prefix where db and logs go.])

dnl
dnl Check for --with-confdir [deprecated, use --sysconfdir instead]
dnl

AC_SUBST_DIR([sysconfdir])
RB_DEFINE_UNQUOTED([CONF_DIR], ["$sysconfdir"], [Prefix where config files are installed.])

dnl
dnl datadir
dnl

AC_SUBST_DIR([datadir])
RB_DEFINE_UNQUOTED([DATA_DIR], ["$datadir/${PACKAGE_TARNAME}"], [Directory where read-only assets go.])

dnl
dnl Check for --with-dbdir
dnl

AC_MSG_CHECKING([whether to modify database directory])
AC_ARG_WITH(dbdir, AC_HELP_STRING([--with-dbdir=DIR], [Directory where databases are located]),
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
AC_ARG_WITH(logdir, AC_HELP_STRING([--with-logdir=DIR], [Directory where to write logfiles.]),
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
dnl Check for --with-moduledir
dnl

AC_MSG_CHECKING([whether to modify moduledir])
AC_ARG_WITH(moduledir, [AC_HELP_STRING([--with-moduledir=DIR], [Directory to install modules.])],
[
	AC_MSG_RESULT(yes)
	moduledir=`echo $withval | sed 's/\/$//'`
], [
	AC_MSG_RESULT(no)
	moduledir="${libdir}/modules/${PACKAGE_TARNAME}"
])

RB_DEFINE_UNQUOTED([MODULE_DIR], ["$moduledir"], [Prefix where modules are installed.])
AC_SUBST_DIR([moduledir])

dnl
dnl Check for --with-webappdir
dnl

AC_MSG_CHECKING([whether to modify webappdir])
AC_ARG_WITH(webappdir, [AC_HELP_STRING([--with-webappdir=DIR], [Directory to install served assets])],
[
	AC_MSG_RESULT(yes)
	webappdir=`echo $withval | sed 's/\/$//'`
], [
	AC_MSG_RESULT(no)
	webappdir="${datadir}/webapp"
])

RB_DEFINE_UNQUOTED([WEBAPP_DIR], ["$webappdir"], [Prefix where website assets are served from.])
AC_SUBST_DIR([webappdir])


dnl ***************************************************************************
dnl
dnl Third party
dnl

dnl
dnl
dnl libsodium (NaCl) crypto support
dnl
dnl

AC_SUBST(SODIUM_CPPFLAGS, [])
AC_SUBST(SODIUM_LDFLAGS, [])
AC_SUBST(SODIUM_LIBS, [])

AC_ARG_WITH(sodium-includes,
AC_HELP_STRING([--with-sodium-includes=[[[DIR]]]], [Path to sodium include directory]),
[
	SODIUM_CPPFLAGS="-I$withval"
], [])

AC_ARG_WITH(sodium-libs,
AC_HELP_STRING([--with-sodium-libs=[[[DIR]]]], [Path to sodium library directory]),
[
	SODIUM_LDFLAGS="-L$withval"
], [])

RB_CHK_SYSHEADER(sodium.h, [SODIUM_H])
AC_CHECK_LIB([sodium], sodium_init,
[
	have_sodium="yes"
	SODIUM_LIBS="-lsodium"
], [
	have_sodium="no"
	AC_MSG_ERROR([libsodium is required but not found. Try apt-get install libsodium-dev])
])

AM_CONDITIONAL([SODIUM], [test "x$have_sodium" = "xyes"])

dnl
dnl
dnl OpenSSL support
dnl
dnl

AC_SUBST(SSL_CPPFLAGS, [])
AC_SUBST(SSL_LDFLAGS, [])
AC_SUBST(SSL_LIBS, [])

AC_ARG_WITH(ssl,
AC_HELP_STRING([--with-ssl=[[[DIR]]]], [Path to SSL source directory]),
[
	SSL_CPPFLAGS="-I$withval/include"
	SSL_LDFLAGS="-L$withval"
], [])

AC_ARG_WITH(ssl-includes,
AC_HELP_STRING([--with-ssl-includes=[[[DIR]]]], [Path to SSL include directory]),
[
	SSL_CPPFLAGS="-I$withval"
], [])

AC_ARG_WITH(ssl-libs,
AC_HELP_STRING([--with-ssl-libs=[[[DIR]]]], [Path to SSL library directory]),
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
AC_CHECK_LIB(ssl, SSL_version,
[
	have_ssl="yes"
	SSL_LIBS="-lssl"
], [
	have_ssl="no"
	AC_MSG_ERROR([libssl is required but not found. Try apt-get install libssl1.0-dev])
])

AM_CONDITIONAL([SSL], [test "x$have_ssl" = "xyes"])

AC_SUBST(CRYPTO_CPPFLAGS, [])
AC_SUBST(CRYPTO_LDFLAGS, [])
AC_SUBST(CRYPTO_LIBS, [])

AC_CHECK_LIB(crypto, OPENSSL_init,
[
	have_crypto="yes"
	AC_DEFINE(HAVE_CRYPTO, 1, [Define to 1 if libcrypto (-lcrypto) is available.])
	CRYPTO_LIBS="-lcrypto"
], [
	have_crypto="no"
	AC_MSG_ERROR([libcrypto is required but not found.])
])

AM_CONDITIONAL([CRYPTO], [test "x$have_crypto" = "xyes"])

dnl
dnl
dnl libmagic support
dnl
dnl

AC_SUBST(MAGIC_CPPFLAGS, [])
AC_SUBST(MAGIC_LDFLAGS, [])
AC_SUBST(MAGIC_LIBS, [])

AC_ARG_WITH(magic-includes,
AC_HELP_STRING([--with-magic-includes=[[[DIR]]]], [Path to magic include directory]),
[
	MAGIC_CPPFLAGS="-I$withval"
], [])

AC_ARG_WITH(magic-libs,
AC_HELP_STRING([--with-magic-libs=[[[DIR]]]], [Path to magic library directory]),
[
	MAGIC_LDFLAGS="-L$withval"
], [])

RB_CHK_SYSHEADER(magic.h, [MAGIC_H])
AC_CHECK_LIB(magic, magic_version,
[
	have_magic="yes"
	MAGIC_LIBS="-lmagic"
], [
	have_magic="no"
	AC_MSG_ERROR([libmagic is required but not found. Try apt-get install libmagic-dev])
])

AM_CONDITIONAL([MAGIC], [test "x$have_magic" = "xyes"])

dnl
dnl
dnl zlib support
dnl
dnl

AC_SUBST(Z_CPPFLAGS)
AC_SUBST(Z_LDFLAGS)
AC_SUBST(Z_LIBS)

AC_ARG_WITH(z-includes,
AC_HELP_STRING([--with-z-includes=[[[DIR]]]], [Path to zlib include directory]),
[
	Z_CPPFLAGS="-I$withval"
], [])

AC_ARG_WITH(z-libs,
AC_HELP_STRING([--with-z-libs=[[[DIR]]]], [Path to zlib library directory]),
[
	Z_LDFLAGS="-L$withval"
], [])

RB_CHK_SYSHEADER(zlib.h, [ZLIB_H])
AC_CHECK_LIB(z, zlibVersion,
[
	have_zlib="yes"
	Z_LIBS="-lz"
], [
	have_zlib="no"
])

AM_CONDITIONAL([ZLIB], [test "x$have_zlib" = "xyes"])

dnl
dnl
dnl lz4 support
dnl
dnl

AC_SUBST(LZ4_CPPFLAGS)
AC_SUBST(LZ4_LDFLAGS)
AC_SUBST(LZ4_LIBS)

AC_ARG_WITH(lz4-includes,
AC_HELP_STRING([--with-lz4-includes=[[[DIR]]]], [Path to LZ4 include directory]),
[
	LZ4_CPPFLAGS="-I$withval"
], [])

AC_ARG_WITH(lz4-libs,
AC_HELP_STRING([--with-lz4-libs=[[[DIR]]]], [Path to LZ4 library directory]),
[
	LZ4_LDFLAGS="-L$withval"
], [])

RB_CHK_SYSHEADER(lz4.h, [LZ4_H])
AC_CHECK_LIB(lz4, LZ4_versionNumber,
[
	have_lz4="yes"
	LZ4_LIBS="-llz4"
], [
	have_lz4="no"
])

AM_CONDITIONAL([LZ4], [test "x$have_lz4" = "xyes"])

dnl
dnl
dnl snappy support
dnl
dnl

AC_SUBST(SNAPPY_CPPFLAGS)
AC_SUBST(SNAPPY_LDFLAGS)
AC_SUBST(SNAPPY_LIBS)

AC_ARG_WITH(snappy-includes,
AC_HELP_STRING([--with-snappy-includes=[[[DIR]]]], [Path to snappy include directory]),
[
	SNAPPY_CPPFLAGS="-I$withval"
], [])

AC_ARG_WITH(snappy-libs,
AC_HELP_STRING([--with-snappy-libs=[[[DIR]]]], [Path to snappy library directory]),
[
	SNAPPY_LDFLAGS="-L$withval"
], [])

RB_CHK_SYSHEADER(snappy.h, [SNAPPY_H])
AC_CHECK_LIB(snappy, snappy_compress,
[
	have_snappy="yes"
	SNAPPY_LIBS="-lsnappy"
], [
	have_snappy="no"
])

AM_CONDITIONAL([SNAPPY], [test "x$have_snappy" = "xyes"])

dnl
dnl
dnl libgmp support
dnl
dnl

PKG_CHECK_MODULES(GMP, [gmp], [have_gmp="yes"], [have_gmp="no"])
AM_CONDITIONAL([GMP], [test "x$have_gmp" = "xyes"])

AC_SUBST(GMP_CPPFLAGS, [])
AC_SUBST(GMP_LDFLAGS, [])
AC_SUBST(GMP_LIBS, ["-lgmp"])

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
AC_HELP_STRING([--with-boost-includes=[[[DIR]]]], [Path to boost include directory]),
[
	boost_incdir="$withval"
	BOOST_CPPFLAGS="-isystem $withval"
], [])

AC_ARG_WITH(boost-libs,
AC_HELP_STRING([--with-boost-libs=[[[DIR]]]], [Path to boost library directory]),
[
	boost_libdir="$withval"
	BOOST_LDFLAGS="-L$withval"
], [])

AC_MSG_CHECKING([whether you asked to use boost sources included here])
AC_ARG_WITH(included-boost,
AC_HELP_STRING([--with-included-boost[[[=shared]]]], [Use the boost sources from included submodule]),
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

	AC_DEFINE(HAVE_LIB_BOOST_SYSTEM, 1, [Define if you have boost_system])
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

dnl
dnl
dnl RocksDB support
dnl
dnl

ROCKSDB_VERSION_MIN="v5.16.6"

AC_SUBST(ROCKSDB_CPPFLAGS, [])
AC_SUBST(ROCKSDB_LDFLAGS, [])
AC_SUBST(ROCKSDB_LIBS, [])

AC_ARG_WITH(rocksdb-includes,
AC_HELP_STRING([--with-rocksdb-includes=[[[DIR]]]], [Path to RocksDB include directory]),
[
	ROCKSDB_CPPFLAGS="-isystem $withval"
], [])

AC_ARG_WITH(rocksdb-libs,
AC_HELP_STRING([--with-rocksdb-libs=[[[DIR]]]], [Path to RocksDB library directory]),
[
	ROCKSDB_LDFLAGS="-L$withval"
], [])

AC_MSG_CHECKING([whether you asked to use the RocksDB included here])
AC_ARG_WITH(included-rocksdb,
AC_HELP_STRING([--with-included-rocksdb[[[=shared]]]], [Use the RocksDB sources from included submodule]),
[
	AC_MSG_RESULT([yes])
	with_included_rocksdb="yes"

	ROCKSDB_CPPFLAGS="-isystem $PWD/deps/rocksdb/include"
	ROCKSDB_LDFLAGS="-L$PWD/deps/rocksdb/"

	AC_MSG_CHECKING([whether to use shared RocksDB])
	if [[ $withval = "static" ]]; then
		AC_MSG_RESULT([no])
		withval="static"
		rocksdb_linkage="static_lib"
		AC_MSG_NOTICE([static RocksDB linkage requires multiple dependencies])
		AC_MSG_NOTICE([| You may need at least: bzip2, zlib, snappy, lz4])
		ROCKSDB_LIBS="$PWD/deps/rocksdb/librocksdb.a"
	else
		AC_MSG_RESULT([yes])
		withval="shared"
		rocksdb_linkage="shared_lib"
		AC_MSG_NOTICE([Shared RocksDB linkage requires running charybdis with an intact build directory])
		ROCKSDB_LDFLAGS+=" -Wl,-rpath=$PWD/deps/rocksdb"
		ROCKSDB_LIBS="-lrocksdb"
	fi

	bash tools/buildrocks.sh $rocksdb_linkage "$ROCKSDB_VERSION_MIN"
	AS_IF([ test $? != 0 ],
	[
		AC_MSG_ERROR([Failed to build RocksDB])
	])
],[
	AC_MSG_RESULT([no])
	with_included_rocksdb="no"

	AC_CHECK_LIB(rocksdb, rocksdb_open, [],
	[
		AC_MSG_ERROR([Unable to find required RocksDB package. Try apt-get install librocksdb-dev])
	])

	ROCKSDB_LIBS="-lrocksdb"
])

dnl
dnl
dnl SpiderMonkey support
dnl
dnl

AC_MSG_CHECKING([whether you asked to use the JS engine included here])
AC_ARG_WITH(included-js,
AC_HELP_STRING([--with-included-js[[[=shared]]]], [Use the JS engine (SpiderMonkey) sources we include here]),
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
	JS_LIBS+=" -lmozjs-52"
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

	js_branch="esr52"

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
	AC_CHECK_LIB(mozjs-52, _Z11JS_ShutDownv,
	[

	], [
		AC_MSG_WARN([Unable to find JS engine (SpiderMonkey 52) package. Try apt-get install libmozjs-dev])
	])

	dnl TODO
	dnl AC_SUBST(JS_CPPFLAGS, ["-I/usr/include/mozjs-52"])
	dnl AC_SUBST(JS_LDFLAGS, [])
	dnl AC_SUBST(JS_LIBS, ["-lmozjs-52"])
])

dnl TODO use an $enable_js var
AM_CONDITIONAL([JS], [[[[ $with_included_js = yes ]]]])

dnl
dnl
dnl jemalloc
dnl
dnl

RB_CHK_SYSHEADER(jemalloc/jemalloc.h, [JEMALLOC_H])
AC_CHECK_LIB(jemalloc, malloc,
[
	jemalloc="yes"
], [
	jemalloc="no"
])

dnl
dnl
dnl Additional linkages
dnl
dnl

AC_SUBST(EXTRA_CPPFLAGS)
AC_SUBST(EXTRA_CXXFLAGS)
AC_SUBST(EXTRA_LDFLAGS)
AC_SUBST(EXTRA_LIBS)

AM_COND_IF([MINGW],
[

], [
	EXTRA_CXXFLAGS+=" -pthread"
	EXTRA_LIBS+="-lpthread -latomic"
])

dnl ***************************************************************************
dnl
dnl Application settings
dnl

dnl
dnl --with-mxid-maxlen
dnl

AC_ARG_WITH(mxid-maxlen,
AC_HELP_STRING([--with-mxid-maxlen=LENGTH],[Set the upper-bound mxid length to LENGTH (default 255, max 511)]),
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

VERSION_TAG_CMD="git describe --tags --abbrev=0 --dirty --always"
RB_VERSION_TAG=`$VERSION_TAG_CMD`
RB_DEFINE_UNQUOTED([VERSION_TAG], ["$RB_VERSION_TAG"], [Git tag at configuration time.])
#CPPDEFINE([RB_VERSION_TAG], [`$VERSION_TAG_CMD`])

VERSION_CMD="git describe --tags --always"
RB_VERSION=`$VERSION_CMD`
RB_DEFINE_UNQUOTED([VERSION], ["$RB_VERSION"], [Version generated at configuration time.])

DATESTR_CMD="date"
RB_DATESTR=`$DATESTR_CMD`
RB_DEFINE_UNQUOTED([DATESTR], ["$RB_DATESTR"], [Readable date string of configuration time.])

DATECODE_CMD="date +%s"
RB_DATECODE=`$DATECODE_CMD`
RB_DEFINE_UNQUOTED([DATECODE], [$RB_DATECODE], [UNIX epoch time at configuration time.])

dnl TODO: XXX: These have better names than the above, rm previous eventually.
RB_DEFINE_UNQUOTED([DATE_CONFIGURED], ["$RB_DATESTR"], [Convenience string of RB_TIME_CONFIGURED])
CPPDEFINE([RB_TIME_CONFIGURED], [$(date +%s)])

dnl
dnl --with-custom-branding
dnl

AC_MSG_CHECKING([whether custom branding is requested])
AC_ARG_WITH(custom-branding, AC_HELP_STRING([--with-custom-branding=NAME], [Custom branding name.]),
[
	AC_DEFINE_UNQUOTED(BRANDING_NAME, ["$withval"], [Custom branding name.])
	AC_DEFINE(CUSTOM_BRANDING, 1, [Define if custom branding is enabled.])
	AC_MSG_RESULT([yes])
],[
	AC_DEFINE_UNQUOTED(BRANDING_NAME, ["$PACKAGE_NAME"], [Current package])
	AC_MSG_RESULT([no])
])

dnl
dnl --with-custom-version
dnl

AC_MSG_CHECKING([whether a custom version is requested])
AC_ARG_WITH(custom-version, AC_HELP_STRING([--with-custom-version=NAME], [Custom version branding.]),
[
	AC_DEFINE_UNQUOTED(BRANDING_VERSION, ["$withval"], [Custom branding version.])
	AC_MSG_RESULT([yes])
], [
	AC_DEFINE_UNQUOTED(BRANDING_VERSION, ["$RB_VERSION"], [Current version])
	AC_MSG_RESULT([no])
])

dnl ***************************************************************************
dnl
dnl Misc
dnl

dnl ***************************************************************************
dnl
dnl Output
dnl

AC_OUTPUT

if [[ $DEBUG ]]; then
	debug="yes"
else
	debug="no"
fi


dnl Make it look sexay!
echo
echo "Configured ........................ $PACKAGE_NAME $PACKAGE_VERSION"
echo "Version ........................... $RB_VERSION"
echo "Configuration time ................ $RB_DATESTR"
echo "Host OS ........................... $host_os"
echo "Compiler .......................... $CXX"
echo "Compiler flags (CXXFLAGS) ......... $CXXFLAGS"
echo "Building boost .................... $with_included_boost"
echo "Building RocksDB .................. $with_included_rocksdb"
echo "Building JS (SpiderMonkey) ........ $with_included_js"
echo "Ziplinks (libz) support ........... $have_zlib"
echo "LZ4 support ....................... $have_lz4"
echo "Snappy support .................... $have_snappy"
echo "GNU MP support .................... $have_gmp"
echo "Sodium support .................... $have_sodium"
echo "SSL support ....................... $have_ssl"
echo "Crypto support .................... $have_crypto"
echo "Magic support ..................... $have_magic"
echo "Linux AIO support ................. $aio"
echo "IPv6 support ...................... $ipv6"
echo "Precompiled headers ............... $build_pch"
echo "Developer debug ................... $debug"
echo "Developer assert .................. $assert ($assert_type)"
echo "Optimized build ................... $optimize"
echo "Link-time optimized ............... $lto"
echo "Optimization level ................ $optimize_level"
echo "Generic binary .................... $enable_generic"
echo "Logging level ..................... $LOG_LEVEL"
echo "Installing into ................... $prefix"
echo
echo "* Ready to build $PACKAGE_NAME"
echo "* Now run 'make'"
