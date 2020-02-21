AC_SUBST([CXX_VERSION])
AC_DEFUN([AX_CXX_VERSION],
[
	AC_CACHE_CHECK([cxx version], [ax_cv_cxx_version],
	[
		AM_COND_IF(GCC,
		[
			ax_cv_cxx_version="`$CXX -dumpfullversion`"
		])

		AM_COND_IF(CLANG,
		[
			ax_cv_cxx_version="`$CXX -dumpversion`"
		])

		AS_IF([test "x$ax_cv_cxx_version" = "x"],
		[
			ax_cv_cxx_version=""
		])
	])

	CXX_VERSION=$ax_cv_cxx_version
])

AC_SUBST([CXX_EPOCH])
AC_DEFUN([AX_CXX_EPOCH],
[
	AC_CACHE_CHECK([cxx epoch], [ax_cv_cxx_epoch],
	[
		AM_COND_IF(GCC,
		[
			ax_cv_cxx_epoch="`$CXX -dumpversion`"
		])

		AM_COND_IF(CLANG,
		[
			ax_cv_cxx_epoch="`$CXX -dumpversion | cut -d'.' -f1`"
		])

		AS_IF([test "x$ax_cv_cxx_epoch" = "x"],
		[
			ax_cv_cxx_epoch=""
		])
	])

	CXX_EPOCH=$ax_cv_cxx_epoch
])
