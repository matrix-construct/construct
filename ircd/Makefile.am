AUTOMAKE_OPTIONS = foreign

DEFS += \
	-DIRCD_UNIT \
	###

AM_CXXFLAGS = \
	-ftls-model=initial-exec \
	@EXTRA_CXXFLAGS@ \
	###

if DEBUG
if GCC
AM_CXXFLAGS += -fmax-errors=2
endif
endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@ROCKSDB_CPPFLAGS@ \
	@JS_CPPFLAGS@ \
	@BOOST_CPPFLAGS@ \
	@SSL_CPPFLAGS@ \
	@CRYPTO_CPPFLAGS@ \
	@SODIUM_CPPFLAGS@ \
	@MAGIC_CPPFLAGS@ \
	@SNAPPY_CPPFLAGS@ \
	@LZ4_CPPFLAGS@ \
	@Z_CPPFLAGS@ \
	-include ircd/ircd.pic.h \
	@EXTRA_CPPFLAGS@ \
	###

AM_LDFLAGS = \
	-version-info 3:1:0 \
	-Wl,-fuse-ld=gold \
	-Wl,--warn-common \
	-Wl,--warn-execstack \
	-Wl,--detect-odr-violations \
	-Wl,--unresolved-symbols=report-all \
	-Wl,--no-undefined-version \
	-Wl,--weak-unresolved-symbols \
	-Wl,--no-gc-sections \
	-Wl,--icf=all \
	-Wl,--rosegment \
	-Wl,--gdb-index \
	-Wl,-z -Wl,lazy \
	-Wl,-z -Wl,nodelete \
	-Wl,-z -Wl,nodlopen \
	-Wl,-z -Wl,noexecstack \
	-Wl,-z -Wl,combreloc \
	-Wl,-z -Wl,text-unlikely-segment \
	$(PLATFORM_LDFLAGS) \
	@EXTRA_LDFLAGS@ \
	###

if MINGW
AM_LDFLAGS += \
	-Wl,--enable-runtime-pseudo-reloc \
	-export-symbols-regex '*' \
	###
endif

libircddir = @libdir@
libircd_LTLIBRARIES = libircd.la

libircd_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	@ROCKSDB_LDFLAGS@ \
	@JS_LDFLAGS@ \
	@BOOST_LDFLAGS@ \
	@SSL_LDFLAGS@ \
	@CRYPTO_LDFLAGS@ \
	@SODIUM_LDFLAGS@ \
	@MAGIC_LDFLAGS@ \
	@SNAPPY_LDFLAGS@ \
	@LZ4_LDFLAGS@ \
	@Z_LDFLAGS@ \
	###

libircd_la_LIBADD = \
	@ROCKSDB_LIBS@ \
	@JS_LIBS@ \
	@BOOST_LIBS@ \
	@SSL_LIBS@ \
	@CRYPTO_LIBS@ \
	@SODIUM_LIBS@ \
	@MAGIC_LIBS@ \
	@SNAPPY_LIBS@ \
	@LZ4_LIBS@ \
	@Z_LIBS@ \
	@EXTRA_LIBS@ \
	###

# Since this is a GNU C++ project we assume the non-standard respect for
# the order of this sources list. Static initialization will occur in
# the same order as this list, and destruction in the reverse order.
#
# Add the most dependent units at the end and the least dependent units
# at the beginning.
#
libircd_la_SOURCES =#
libircd_la_SOURCES += assert.cc
libircd_la_SOURCES += info.cc
libircd_la_SOURCES += allocator.cc
libircd_la_SOURCES += exception.cc
libircd_la_SOURCES += util.cc
libircd_la_SOURCES += demangle.cc
libircd_la_SOURCES += locale.cc
libircd_la_SOURCES += lexical.cc
libircd_la_SOURCES += stringops.cc
libircd_la_SOURCES += tokens.cc
libircd_la_SOURCES += parse.cc
libircd_la_SOURCES += rand.cc
libircd_la_SOURCES += base.cc
libircd_la_SOURCES += crh.cc
libircd_la_SOURCES += fmt.cc
libircd_la_SOURCES += json.cc
libircd_la_SOURCES += conf.cc
libircd_la_SOURCES += logger.cc
libircd_la_SOURCES += magic.cc
libircd_la_SOURCES += sodium.cc
libircd_la_SOURCES += openssl.cc
libircd_la_SOURCES += rfc1459.cc
libircd_la_SOURCES += rfc3986.cc
libircd_la_SOURCES += rfc1035.cc
libircd_la_SOURCES += http.cc
libircd_la_SOURCES += stats.cc
libircd_la_SOURCES += prof.cc
libircd_la_SOURCES += fs.cc
libircd_la_SOURCES += ios.cc
libircd_la_SOURCES += ctx.cc
if AIO
libircd_la_SOURCES += fs_aio.cc
endif
libircd_la_SOURCES += mods.cc
if LINUX
libircd_la_SOURCES += mods_ldso.cc
endif
libircd_la_SOURCES += db_port.cc
libircd_la_SOURCES += db_env.cc
libircd_la_SOURCES += db.cc
libircd_la_SOURCES += net.cc
libircd_la_SOURCES += server.cc
libircd_la_SOURCES += client.cc
libircd_la_SOURCES += resource.cc
if JS
libircd_la_SOURCES += js.cc
endif
libircd_la_SOURCES += m_name.cc
libircd_la_SOURCES += m_id.cc
libircd_la_SOURCES += m_dbs.cc
libircd_la_SOURCES += m_state.cc
libircd_la_SOURCES += m_room.cc
libircd_la_SOURCES += m_v1.cc
libircd_la_SOURCES += m.cc
libircd_la_SOURCES += m_event.cc
libircd_la_SOURCES += ircd.cc

# Units containing a spirit grammar have some special needs to mitigate
# larger-than-normal compile time, compile memory, and output objects.
# A composite of CXXFLAGS is used specifically on units with grammars.
GUNIT_CXXFLAGS = ###

# Grammar templates can generate a huge number of individual debug symbols
# for each template instantiation deep within spirit; we try to reduce...
GUNIT_CXXFLAGS += -fno-var-tracking
GUNIT_CXXFLAGS += -fno-var-tracking-assignments
GUNIT_CXXFLAGS += -femit-struct-debug-baseonly

# The recursive grammars can consume a large amount of RAM when compiling
# (>= 2 GiB) which can thrash small systems and even sometimes crash GCC.
# This option reduces memory consumption at the cost of compilation speed.
if GCC
if LOWMEM_COMPILE
GUNIT_CXXFLAGS += --param ggc-min-expand=1
endif
endif

parse.lo:      AM_CPPFLAGS := -include ircd/spirit.h ${AM_CPPFLAGS}
parse.lo:      CXXFLAGS += ${GUNIT_CXXFLAGS}

json.lo:       AM_CPPFLAGS := -include ircd/spirit.h ${AM_CPPFLAGS}
json.lo:       CXXFLAGS += ${GUNIT_CXXFLAGS}

http.lo:       AM_CPPFLAGS := -include ircd/spirit.h ${AM_CPPFLAGS}
http.lo:       CXXFLAGS += ${GUNIT_CXXFLAGS}

fmt.lo:        AM_CPPFLAGS := -include ircd/spirit.h ${AM_CPPFLAGS}
fmt.lo:        CXXFLAGS += ${GUNIT_CXXFLAGS}

rfc3986.lo:    AM_CPPFLAGS := -include ircd/spirit.h ${AM_CPPFLAGS}
rfc3986.lo:    CXXFLAGS += ${GUNIT_CXXFLAGS}

rfc1459.lo:    AM_CPPFLAGS := -include ircd/spirit.h ${AM_CPPFLAGS}
rfc1459.lo:    CXXFLAGS += ${GUNIT_CXXFLAGS}

m_id.lo:       AM_CPPFLAGS := -include ircd/spirit.h ${AM_CPPFLAGS}
m_id.lo:       CXXFLAGS += ${GUNIT_CXXFLAGS}
