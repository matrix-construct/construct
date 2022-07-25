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
