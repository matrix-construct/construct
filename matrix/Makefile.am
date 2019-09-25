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

#TODO: split options for if LD_GOLD
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
libircd_matrix_LTLIBRARIES = libircd_matrix.la

libircd_matrix_LIBADD =#
libircd_matrix_LIBADD += -lircd

libircd_matrix_la_SOURCES =#
libircd_matrix_la_SOURCES += name.cc
libircd_matrix_la_SOURCES += id.cc
libircd_matrix_la_SOURCES += dbs.cc
libircd_matrix_la_SOURCES += room.cc
libircd_matrix_la_SOURCES += fed.cc
libircd_matrix_la_SOURCES += init_bootstrap.cc
libircd_matrix_la_SOURCES += matrix.cc
libircd_matrix_la_SOURCES += event.cc

### TODO: dedup with ircd/Makefile.am

# Units containing a spirit grammar have some special needs to mitigate
# larger-than-normal compile time, compile memory, and output objects.
# A composite of CXXFLAGS is used specifically on units with grammars.
GUNIT_CXXFLAGS = ###

# Grammar templates can generate a huge number of individual debug symbols
# for each template instantiation deep within spirit; we try to reduce...
GUNIT_CXXFLAGS += -fno-var-tracking
if GCC
GUNIT_CXXFLAGS += -fno-var-tracking-assignments
GUNIT_CXXFLAGS += -femit-struct-debug-baseonly
endif

# The recursive grammars can consume a large amount of RAM when compiling
# (>= 2 GiB) which can thrash small systems and even sometimes crash GCC.
# This option reduces memory consumption at the cost of compilation speed.
if GCC
if LOWMEM_COMPILE
GUNIT_CXXFLAGS += --param ggc-min-expand=1
endif
endif

id.lo: AM_CPPFLAGS := -include ircd/spirit.h ${AM_CPPFLAGS}
id.lo: CXXFLAGS += ${GUNIT_CXXFLAGS}
