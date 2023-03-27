AC_DEFUN([RB_HELP_STRING], [AS_HELP_STRING([$1], [$2], 40, 200)])

AC_DEFUN([RB_VAR_PREPEND],
[
	AS_VAR_SET([$1], ["$2 ${$1}"])
])

AC_DEFUN([RB_VAR_APPEND],
[
	AS_VAR_SET([$1], ["${$1} $2"])
])

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
		RB_VAR_PREPEND([CPPFLAGS], ["-D$1"])
	else
		RB_VAR_PREPEND([CPPFLAGS], ["-D$1=$2"])
	fi
])

AC_DEFUN([AM_COND_IF_NOT],
[
	AM_COND_IF([$1], [], [$2])
])

AC_DEFUN([AM_COND_IF_NAND],
[
	AM_COND_IF([$1], [],
	[
		AM_COND_IF([$2], [], [$3])
	])
])

AC_DEFUN([RB_CHK_SYSHEADER],
[
	if test -z "$rb_cv_header_have_$2"; then
		AC_CHECK_HEADER([$1],
		[
			rb_cv_header_have_$2=1;
			AC_DEFINE([HAVE_$2], [1], [ Indication $1 is available. ])
			RB_DEFINE_UNQUOTED([INC_$2], [$1>], [ The computed-include location of $1. ])
		], [
			RB_DEFINE_UNQUOTED([INC_$2], [stddef.h>], [ The dead-header in place of $1. ])
		])
	fi
])

AC_DEFUN([RB_CHECK_SIZEOF],
[
	AC_CHECK_SIZEOF([$1])
	if [[ "${ac_cv_sizeof_$1}" != "$2" ]]; then
		AC_MSG_ERROR([sizeof($1) must be $2 not ${ac_cv_sizeof_$1}. Check config.log for compiler error.])
	fi
])
