AUTOMAKE_OPTIONS = foreign

DEFS += \
	-DIRCD_UNIT \
	-DIRCD_MATRIX_UNIT \
	###

AM_CXXFLAGS = \
	-ftls-model=global-dynamic \
	@EXTRA_CXXFLAGS@ \
	###

if DEBUG
if GCC
AM_CXXFLAGS += -fmax-errors=2
endif
endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-include ircd/ircd.pic.h \
	-include ircd/mods/mapi.h \
	@EXTRA_CPPFLAGS@ \
	###

AM_LDFLAGS = \
	-version-info 0:1:0 \
	-Wl,--no-undefined-version \
	-Wl,--unresolved-symbols=report-all \
	-Wl,--allow-shlib-undefined \
	-Wl,--weak-unresolved-symbols \
	-Wl,-z,now \
	-L$(top_srcdir)/ircd \
	$(PLATFORM_LDFLAGS) \
	@EXTRA_LDFLAGS@ \
	###

if MINGW
AM_LDFLAGS += \
	-Wl,--enable-runtime-pseudo-reloc \
	###
endif

libircd_matrixdir = @libdir@
libircd_matrix_LTLIBRARIES = \
	libircd_matrix.la \
	###

libircd_matrix_la_LIBADD = \
	-lircd \
	###

#
# Unit list
#

libircd_matrix_la_SOURCES =#
libircd_matrix_la_SOURCES += name.cc
libircd_matrix_la_SOURCES += id.cc
libircd_matrix_la_SOURCES += dbs.cc
libircd_matrix_la_SOURCES += room.cc
libircd_matrix_la_SOURCES += fed.cc
libircd_matrix_la_SOURCES += init_bootstrap.cc
libircd_matrix_la_SOURCES += matrix.cc
libircd_matrix_la_SOURCES += event.cc

#
# Unit configurations
#

id.lo: AM_CPPFLAGS   := ${SPIRIT_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
id.lo: CXXFLAGS      += ${SPIRIT_UNIT_CXXFLAGS} ${AM_CXXFLAGS}
